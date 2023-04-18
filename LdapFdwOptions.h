#ifndef __ldapfdwoptions__
#define __ldapfdwoptions__

typedef struct LdapFdwOptions
{
	char * uri;
	char * username;
	char * password;
	char * basedn;
	char * filter;
	int scope;
} LdapFdwOptions;

#endif
