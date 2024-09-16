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
	char *attribute_name;
	char *ldap_type;
	char *pg_type;
} AttrTypemap;

AttrTypemap ** Create_AttrTypemap();

size_t fetch_ldap_typemap(AttrTypemap***, LDAP *, char *, char *);

size_t fetch_objectclass(char ***, LDAP *, char *, char *);

extern struct timeval timeout_struct;

#endif
