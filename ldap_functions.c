#include <ldap_functions.h>

int common_ldap_bind(LDAP *ld, const char *username, const char *passwd)
{
	_cleanup_berval_ struct berval *berval_password = NULL;
	if(password != NULL) berval_password = ber_bvstrdup(password);
	if(!use_sasl) return ldap_simple_bind_s( ld, username, password );
	else if(use_sasl) return ldap_sasl_bind_s( ld, username, LDAP_SASL_SIMPLE, berval_password , NULL, NULL, NULL);
}

