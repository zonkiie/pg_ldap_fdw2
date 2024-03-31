#include "LdapFdwOptions.h"

void initLdapFdwOptions(LdapFdwOptions* ldapFdwOptions)
{
	ldapFdwOptions->uri = NULL;
	ldapFdwOptions->username = NULL;
	ldapFdwOptions->password = NULL;
	ldapFdwOptions->basedn = NULL;
	ldapFdwOptions->filter = NULL;
	ldapFdwOptions->objectclass = NULL;
	ldapFdwOptions->schemadn = NULL;
}
