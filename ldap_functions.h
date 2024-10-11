#ifndef __ldap_functions__
#define __ldap_functions__

#include <ldap.h>
#include <ldap_schema.h>
#include <sasl/sasl.h>
#include "LdapFdwOptions.h"

int common_ldap_bind(LDAP *, const char *, const char *, int);
void free_ldap(LDAP **);
void free_ldap_message(LDAPMessage **);
void free_ber(BerElement **);
void free_berval(struct berval **);
int append_ldap_mod(LDAPMod ***, LDAPMod *);
int LDAPMod_count(LDAPMod **);
LDAPMod * create_new_ldap_mod(void);
LDAPMod * copy_ldap_mod(LDAPMod *);
LDAPMod * construct_new_ldap_mod(int, char *, char **);
char * ldap_dn2filter(char *);
void free_ldap_mod(LDAPMod * );
int fetch_attribute_type(void);
int fetch_schema(LDAP *);

#define _cleanup_ldap_ __attribute((cleanup(free_ldap)))
#define _cleanup_ldap_message_ __attribute((cleanup(free_ldap_message)))
#define _cleanup_ldap_ber_ __attribute((cleanup(free_ber)))
#define _cleanup_berval_ __attribute((cleanup(free_berval)))

extern struct timeval timeout_struct;
extern LdapFdwOptions *option_params;

int ldap_simple_bind_s(LDAP *, const char *, const char *);
void ldap_value_free(char **);

#endif
