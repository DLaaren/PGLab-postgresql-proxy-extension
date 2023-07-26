MODULE_big = proxy
OBJS = \
	$(WIN32RES) \
	proxy.o \
	proxy_bgw.o \
	proxy_log.o \
	proxy_manager.o
	
# EXTENSION = proxy
# DATA = proxy--1.0.sql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/proxy
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif