#ifndef __ldapfdwconn__
#define __ldapfdwconn__

#include <unistd.h>
#include <ldap.h>
#include <ldap_schema.h>
#include <sasl/sasl.h>
#include "LdapFdwOptions.h"

typedef struct LdapFdwConn
{
	LDAP *ld;
	LDAPControl **serverctrls;
	LDAPControl **clientctrls;
}

#endif
