#ifndef __ldapfdwoptions__
#define __ldapfdwoptions__

typedef struct LdapFdwOptions
{
	char * uri;
	char * username;
	char * password;
	char * basedn;
	char * filter;
	char * objectclass;
	int scope;
} LdapFdwOptions;

#endif
