#ifndef __ldap_functions__
#define __ldap_functions__

#include <ldap.h>
#include <sasl/sasl.h>

int common_ldap_bind(LDAP *, const char *, const char *, int);
void free_ldap(LDAP **);
void free_ldap_message(LDAPMessage **);
void free_ber(BerElement **);
void free_berval(struct berval **);
size_t fetch_objectclass(char ***, LDAP *, char *);

#define _cleanup_ldap_ __attribute((cleanup(free_ldap)))
#define _cleanup_ldap_message_ __attribute((cleanup(free_ldap_message)))
#define _cleanup_ldap_ber_ __attribute((cleanup(free_ber)))
#define _cleanup_berval_ __attribute((cleanup(free_berval)))

extern struct timeval timeout_struct;

#endif
