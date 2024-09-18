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

typedef struct AttrListPg
{
	char *attr_name;
	char *pg_type;
} AttrListPg;

typedef struct AttrListType
{
	char *attr_name;
	char *ldap_type;
	char *pg_type;
	bool isarray;
} AttrListType;

AttrListType** Create_AttrListType();
size_t AttrListTypeCount(AttrListType***);
size_t AttrListTypeAppend(AttrListType***, AttrListType *);
void AttrListTypeFreeSingle(AttrListType **);
void AttrListTypeFree(AttrListType***);
AttrListPg ** Create_AttrListPg();
size_t AttrListPgCount(AttrListPg*** );
size_t AttrListPgAppend(AttrListPg*** , AttrListPg *);
void AttrListPgFreeSingle(AttrListPg **);
void AttrListPgFree(AttrListPg*** );


size_t fetch_ldap_typemap(AttrListType***, LDAP *, char *, char *);
size_t fill_AttrListType(AttrListType***, AttrTypemap**);


size_t fetch_objectclass(char ***, LDAP *, char *, char *);

extern struct timeval timeout_struct;

#endif
