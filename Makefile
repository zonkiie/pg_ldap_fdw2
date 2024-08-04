EXTENSION        = ldap2_fdw
EXTVERSION       = 2.0
MODULE_big       = ldap2_fdw
OBJS             = ldap2_fdw.o deparse.o helper_functions.o ldap_functions.o LdapFdwOptions.o
DOCS             = $(wildcard *.md)
DATA		 = ldap2_fdw--1.0.sql

BUILD_DIR	 = $(shell pwd)
PG_CONFIG	 = pg_config
PGXS		 := $(shell $(PG_CONFIG) --pgxs)
PG_CPPFLAGS	 = -std=c11
include $(PGXS)

