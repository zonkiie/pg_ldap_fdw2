#include "LdapFdwOptions.h"

LdapFdwOptions * createLdapFdwOptions()
{
	return (LdapFdwOptions *)palloc0(sizeof(LdapFdwOptions));
}

void initLdapFdwOptions(LdapFdwOptions* ldapFdwOptions)
{
	ldapFdwOptions->uri = NULL;
	ldapFdwOptions->username = NULL;
	ldapFdwOptions->password = NULL;
	ldapFdwOptions->basedn = NULL;
	ldapFdwOptions->filter = NULL;
	//ldapFdwOptions->objectclasses = NULL;
	ldapFdwOptions->schemadn = NULL;
}

void free_options(LdapFdwOptions * options)
{
	free_pstr(&(options->uri));
	free_pstr(&(options->username));
	free_pstr(&(options->password));
	free_pstr(&(options->basedn));
	free_pstr(&(options->filter));
	//free_pstr(&(options->objectclass));
	//free_pstr_array(&(options->objectclasses));
	//free_carr_n(&(options->objectclasses));
	free_pstr(&(options->schemadn));

}

