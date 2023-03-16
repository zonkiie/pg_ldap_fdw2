EXTENSION    = ldap2_fdw
EXTVERSION   = 2.0
MODULE_big   = ldap2_fdw
OBJS         = ldap2_fdw.o helper_functions.o ldap_functions.o
DOCS         = $(wildcard *.md)
DATA		 = ldap2_fdw--1.0.sql

BUILD_DIR	 = $(shell pwd)
PG_CONFIG	 = pg_config
PGXS		 := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

