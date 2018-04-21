#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "executor/executor.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/array.h"
#include "utils/fmgrprotos.h"
#include "access/htup_details.h"
#include "catalog/pg_type.h"

PG_MODULE_MAGIC;

struct string {
    char *data;
    size_t lenght;
};

void initString(struct string *s) {
    s->lenght = 0;
    s->data = palloc(s->lenght + 1);
    s->data[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t newLength = s->lenght + size * nmemb;
    s->data = repalloc(s->data, newLength + 1);
    memcpy(s->data + s->lenght, ptr, size * nmemb);
    s->data[newLength] = '\0';
    s->lenght = newLength;
    return size * nmemb;
}

char *toStr(text *t) {
    size_t length = VARSIZE(t) - VARHDRSZ;
    char *str = palloc(length + 1);
    memcpy(str, VARDATA(t), length);
    str[length] = 0;
    return str;
}

PG_FUNCTION_INFO_V1(load_feed);
Datum load_feed(PG_FUNCTION_ARGS) {
    text *url = PG_GETARG_TEXT_P(0);
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if (!curl) {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unable to initialize curl")));
    }
    struct string s;
    initString(&s);
    char *urlStr = toStr(url);
    curl_easy_setopt(curl, CURLOPT_URL, urlStr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    pfree(urlStr);
    if (res != CURLE_OK) {
        ereport(ERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("curl_easy_perform() failed: %s\n", curl_easy_strerror(res))));
    }
    xmlDoc *doc = xmlParseDoc((xmlChar *) s.data);
    if (doc == NULL) {
        ereport(ERROR, (errcode(ERRCODE_ASSERT_FAILURE), errmsg("enable to parse document")));
    }
    pfree(s.data);
    TupleDesc feedTupleDesc = BlessTupleDesc(RelationNameGetTupleDesc("news.feed"));
    TupleDesc itemTupleDesc = BlessTupleDesc(RelationNameGetTupleDesc("news.item"));
    int16 typlen;
    bool typbyval;
    char typalign;
    get_typlenbyvalalign(itemTupleDesc->tdtypeid, &typlen, &typbyval, &typalign);
    Datum *itemsArrayElements = NULL;
    size_t count = 0;
    xmlNode *rootNode = xmlDocGetRootElement(doc);
    for (xmlNode *channelNode = xmlFirstElementChild(rootNode); channelNode; channelNode = xmlNextElementSibling(channelNode)) {
        if (strcmp((char *) channelNode->name, "channel") == 0) {
            for (xmlNode *itemNode = xmlFirstElementChild(channelNode); itemNode; itemNode = xmlNextElementSibling(itemNode)) {
                if (strcmp((char *) itemNode->name, "item") == 0) {
                    count++;
                }
            }
            itemsArrayElements = palloc(sizeof(Datum) * count);
            size_t i = 0;
            for (xmlNode *itemNode = xmlFirstElementChild(channelNode); itemNode; itemNode = xmlNextElementSibling(itemNode)) {
                if (strcmp((char *) itemNode->name, "item") == 0) {
                    char *title = NULL;
                    char *desc = NULL;
                    char *id = NULL;
                    char *link = NULL;
                    char *pubDate = NULL;
                    for (xmlNode *attrNode = xmlFirstElementChild(itemNode); attrNode; attrNode = xmlNextElementSibling(attrNode)) {
                        if (strcmp((char *) attrNode->name, "title") == 0) {
                            title = (char *) xmlNodeGetContent(attrNode);
                        }
                        if (strcmp((char *) attrNode->name, "description") == 0) {
                            desc = (char *) xmlNodeGetContent(attrNode);
                        }
                        if (strcmp((char *) attrNode->name, "guid") == 0) {
                            id = (char *) xmlNodeGetContent(attrNode);
                        }
                        if (strcmp((char *) attrNode->name, "link") == 0) {
                            link = (char *) xmlNodeGetContent(attrNode);
                        }
                        if (strcmp((char *) attrNode->name, "pubDate") == 0) {
                            pubDate = (char *) xmlNodeGetContent(attrNode);
                        }
                    }
                    Datum *itemValues = palloc(sizeof(Datum) * 5);
                    itemValues[0] = title != NULL ? CStringGetTextDatum(title) : CStringGetTextDatum("NULL");
                    itemValues[1] = desc != NULL ? CStringGetTextDatum(desc) : CStringGetTextDatum("NULL");
                    itemValues[2] = id != NULL ? CStringGetTextDatum(id) : CStringGetTextDatum("NULL");
                    itemValues[3] = link != NULL ? CStringGetTextDatum(link) : CStringGetTextDatum("NULL");
                    itemValues[4] = DirectFunctionCall2(to_timestamp, pubDate != NULL ? CStringGetTextDatum(pubDate) : CStringGetTextDatum("NULL"), CStringGetTextDatum("Dy, DD Mon YYYY HH24:MI:SS"));
                    bool *itemNulls = palloc(sizeof(bool) * 5);
                    itemNulls[0] = title == NULL;
                    itemNulls[1] = desc == NULL;
                    itemNulls[2] = id == NULL;
                    itemNulls[3] = link == NULL;
                    itemNulls[4] = pubDate == NULL;
                    HeapTuple itemHeapTuple = heap_form_tuple(itemTupleDesc, itemValues, itemNulls);
                    itemsArrayElements[i] = HeapTupleGetDatum(itemHeapTuple);
                    i++;
                }
            }
            break;
        }
    }
    if (itemsArrayElements == NULL) {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("no channel element found")));
    }
    ArrayType *itemsArray = construct_array(itemsArrayElements, count, itemTupleDesc->tdtypeid, typlen, typbyval, typalign);
    Datum *feedValues = palloc(sizeof(Datum));
    feedValues[0] = PointerGetDatum(itemsArray);
    bool *feedNulls = palloc(sizeof(bool) * 1);
    feedNulls[0] = false;
    HeapTuple feedHeapTuple = heap_form_tuple(feedTupleDesc, feedValues, feedNulls);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    PG_RETURN_DATUM(HeapTupleGetDatum(feedHeapTuple));
}
