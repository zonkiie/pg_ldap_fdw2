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

typedef struct AttrTypemap
{
	char *description;
	char *ldap_type;
	char *pg_type;
} AttrTypemap;

typedef struct AttrListType
{
	char *attr_name;
	char *ldap_type;
	char *pg_type;
	bool isarray;
	bool nullable;
} AttrListType;

AttrListType** Create_AttrListType();
AttrListType * Create_SingleAttrListType();
size_t AttrListTypeCount(AttrListType***);
size_t AttrListTypeAppend(AttrListType***, AttrListType *);
void AttrListTypeFreeSingle(AttrListType **);
void AttrListTypeFree(AttrListType***);
char * getAttrTypeByAttrName(AttrListType ***, char *);

size_t fetch_ldap_typemap(AttrListType***, char ***, LDAP *, char **, char *);
size_t fill_AttrListType(AttrListType***, AttrTypemap[]);

extern struct timeval timeout_struct;
extern AttrTypemap typemap[];

#endif
