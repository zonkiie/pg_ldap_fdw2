#ifndef __ldapfdwoptions__
#define __ldapfdwoptions__

#include <unistd.h>


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

#endif
