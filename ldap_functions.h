#ifndef __ldap_functions__
#define __ldap_functions__

#include <ldap.h>
#include <sasl/sasl.h>

void free_ldap(LDAP **);
void free_ldap_message(LDAPMessage **);
void free_ber(BerElement **);
void free_berval(struct berval **);


#endif
