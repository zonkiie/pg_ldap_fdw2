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

List * Create_AttrListType(void);
AttrListType * Create_SingleAttrListType(void);
size_t AttrListTypeAppend(List*, AttrListType *);
void AttrListTypeFreeSingle(AttrListType **);
void AttrListTypeFree(List * attrlist);
char * getAttrTypeByAttrName(List*, char *);
bool getAttrNullableByAttrName(List *, char *);

size_t fetch_ldap_typemap(List**, List**, LDAP *, List*, char *);
size_t fill_AttrListType(List*, AttrTypemap[]);

extern struct timeval timeout_struct;
extern AttrTypemap typemap[];

#endif
