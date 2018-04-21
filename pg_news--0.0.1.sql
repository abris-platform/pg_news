-- complain IF script is sourced IN psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_news" to load this file. \quit

CREATE SCHEMA news;

CREATE TYPE news.item AS (
    title TEXT,
    descr TEXT, -- description
    id TEXT, -- guid
    link TEXT,
    pubDate TIMESTAMP WITH TIME ZONE
);

CREATE TYPE news.feed AS (
    items news.item[]
);

CREATE FUNCTION news.load_feed (TEXT) RETURNS news.feed AS '$libdir/pg_news' LANGUAGE C IMMUTABLE STRICT;
