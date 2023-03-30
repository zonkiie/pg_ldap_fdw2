#include <ldap_functions.h>

int common_ldap_bind(LDAP *ld, const char *username, const char *password, int use_sasl)
{
	_cleanup_berval_ struct berval *berval_password = NULL;
	if(password != NULL) berval_password = ber_bvstrdup(password);
	if(!use_sasl) return ldap_simple_bind_s( ld, username, password);
	else if(use_sasl) return ldap_sasl_bind_s( ld, username, LDAP_SASL_SIMPLE, berval_password , NULL, NULL, NULL);
}

void free_ldap(LDAP **ldap)
{
	if(*ldap == NULL) return;
	ldap_unbind_ext_s( *ldap , NULL, NULL);
	*ldap = NULL;
	sasl_done();
}

void free_ldap_message(LDAPMessage **message)
{
	if(*message == NULL || message == NULL) return;
	ldap_msgfree(*message);
	*message = NULL;
}

void free_ber(BerElement **ber)
{
	if(ber == NULL || *ber == NULL) return;
	ber_free(*ber, 0);
	*ber = NULL;
}

void free_berval(struct berval **bval)
{
	if(bval == NULL || *bval == NULL) return;
	ber_bvfree(*bval);
	*bval = NULL;
}


