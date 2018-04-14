EXTENSION = pgnews
DATA = pgnews--0.0.1.sql
REGRESS = pgnews_test
MODULES = pgnews
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
