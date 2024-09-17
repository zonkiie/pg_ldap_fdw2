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

typedef struct AttrListLdap
{
	char *attr_name;
	char *ldap_type;
	bool isarray;
} AttrListLdap;

AttrListLdap ** Create_AttrListLdap();
size_t AttrListLdapCount(AttrListLdap***);
size_t AttrListLdapAppend(AttrListLdap***, AttrListLdap *);
void AttrListLdapFree(AttrListLdap **);
void AttrListFree(AttrListLdap***);
AttrListPg ** Create_AttrListPg();
size_t AttrListPgCount(AttrListPg*** );
size_t AttrListPgAppend(AttrListPg*** , AttrListPg *);
void AttrListPgFreeSingle(AttrListPg **);
void AttrListPgFree(AttrListPg*** );


size_t fetch_ldap_typemap(AttrListLdap***, LDAP *, char *, char *);
size_t translate_AttrListLdap(AttrListPg***, AttrListLdap**);


size_t fetch_objectclass(char ***, LDAP *, char *, char *);

extern struct timeval timeout_struct;

#endif
