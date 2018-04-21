EXTENSION = pg_news
DATA = pg_news--0.0.1.sql
REGRESS = pg_news_test
MODULES = pg_news
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
