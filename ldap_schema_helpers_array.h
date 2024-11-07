#ifndef __ldap_schema_helpers__
#define __ldap_schema_helpers__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ldap.h>
#include <ldap_schema.h>
#include "helper_functions.h"
#include "utils/elog.h"

#define DEBUGPOINT elog(INFO, "ereport File %s, Func %s, Line %d\n", __FILE__, __FUNCTION__, __LINE__)


AttrListType** Create_AttrListType(void);
AttrListType * Create_SingleAttrListType(void);
size_t AttrListTypeCount(AttrListType***);
size_t AttrListTypeAppend(AttrListType***, AttrListType *);
void AttrListTypeFreeSingle(AttrListType **);
void AttrListTypeFree(AttrListType***);

char * getAttrTypeByAttrName(AttrListType ***, char *);

size_t fetch_ldap_typemap(AttrListType***, char ***, LDAP *, char **, char *);
size_t fill_AttrListType(AttrListType***, AttrTypemap[]);

