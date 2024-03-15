#include <ldap_functions.h>
#include "helper_functions.h"

int common_ldap_bind(LDAP *ld, const char *username, const char *password, int use_sasl)
{
	_cleanup_berval_ struct berval *berval_password = NULL;
	if(password != NULL) berval_password = ber_bvstrdup(password);
	if(!use_sasl) return ldap_simple_bind_s( ld, username, password);
	else if(use_sasl) return ldap_sasl_bind_s( ld, username, LDAP_SASL_SIMPLE, berval_password , NULL, NULL, NULL);
	return -1;
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

size_t fetch_objectclass(char ***attributes, LDAP *ld, char * object_class)
{
	size_t size = 0;
	char *a = NULL;
	char *base_dn = option_params->basedn;
	int rc = 0;
	LDAPMessage *schema = NULL, *entry = NULL;
	BerElement *ber;
	rc = ldap_search_ext_s(
		ld,
		base_dn,
		LDAP_SCOPE_BASE,
		/*schema_filter, */ "(objectClass=*)",
		(char*[]){ "objectClasses", NULL }, // (char*[]){ "attributeTypes", "objectClasses", NULL },   //(char*[]){ NULL },
		0,
		NULL,
		NULL,
		&timeout_struct,
		1000000,
		&schema );
	
	if (rc != LDAP_SUCCESS) {
		if (rc == LDAP_NO_SUCH_OBJECT) {
            fprintf(stderr, "Das Schema-Objekt '%s' wurde nicht gefunden.\n", base_dn);
        }
		fprintf(stderr, "Fehler beim Suchen des Schemas: %s\n", ldap_err2string(rc));
		return -1;
	}

    for (entry = ldap_first_entry(ld, schema); entry != NULL; entry = ldap_next_entry(ld, entry)) {
		char* schema_entry_str = ldap_get_dn(ld, entry);
		ldap_memfree(schema_entry_str);
		for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL; a = ldap_next_attribute( ld, entry, ber ) ) {
			struct berval **vals = NULL;
			if ((vals = ldap_get_values_len( ld, entry, a)) != NULL ) {
				for (int i = 0; vals[i] != NULL; i++ ) {
					if(!strcmp(a, "objectClasses")) {
						int oclass_error = 0;
						const char * oclass_error_text;
						LDAPObjectClass *oclass = ldap_str2objectclass(vals[i]->bv_val, &oclass_error, &oclass_error_text, LDAP_SCHEMA_ALLOW_ALL);
						char **names = oclass->oc_names;
						if(in_array(names, object_class)) {
							
							*attributes = (char**)malloc(1);

							for(int attributes_must_size = 0; oclass->oc_at_oids_must[attributes_must_size] != NULL; attributes_must_size++) {
								*attributes = realloc(*attributes, size + 1);
								*attributes[size] = strdup(oclass->oc_at_oids_must[attributes_must_size]);
								size++;
							}

							for(int attributes_may_size = 0; oclass->oc_at_oids_may[attributes_may_size] != NULL; attributes_may_size++) {
								*attributes = realloc(*attributes, size + 1);
								*attributes[size] = strdup(oclass->oc_at_oids_may[attributes_may_size]);
								size++;
							}
						}
						ldap_objectclass_free(oclass);
					}
				}

				ber_bvecfree(vals);
			}
			ldap_memfree( a );
		}
		if ( ber != NULL ) {

			ber_free( ber, 0 );

		}
    }
	// Aufräumen
	ldap_msgfree(schema);
	return size;
}

int fetch_attribute_type()
{
	return 0;
}

int fetch_schema(LDAP *ld)
{
	return 0;
}
