#ifndef __ldapfdwoptions__
#define __ldapfdwoptions__

#include <unistd.h>
#include "helper_functions.h"


typedef struct LdapFdwOptions
{
	char * uri;
	char * username;
	char * password;
	char * basedn;
	char * filter;
	char * objectclass;
	char * schemadn;
	int scope;
	int use_sasl;
} LdapFdwOptions;

void initLdapFdwOptions(LdapFdwOptions*);
void free_options(LdapFdwOptions *);

#endif
