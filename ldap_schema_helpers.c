#include "ldap_schema_helpers.h"

// Handle ldap types
// https://www.openldap.org/doc/admin22/schema.html

AttrTypemap typemap[] = {
	{"boolean", 			"1.3.6.1.4.1.1466.115.121.1.7", 	"boolean"}, //"bool"
	{"directoryString",		"1.3.6.1.4.1.1466.115.121.1.15",	"varchar"}, //"Unicode (UTF-8) string"
	{"distinguishedName",	"1.3.6.1.4.1.1466.115.121.1.12",	"varchar"}, //"LDAP DN"
	{"integer",				"1.3.6.1.4.1.1466.115.121.1.27",	"integer"}, //"integer"
	{"jpeg",				"1.3.6.1.4.1.1466.115.121.1.28",	"bytea"},	//"JPEG Image"
	{"numericString",		"1.3.6.1.4.1.1466.115.121.1.36",	"numeric"}, //"numeric string"
	{"OID",					"1.3.6.1.4.1.1466.115.121.1.38",	"varchar"}, //"object identifier"
	{"octetString",			"1.3.6.1.4.1.1466.115.121.1.40",	"varchar"}, //"arbitary octets"
	{NULL, NULL, NULL},
};

AttrTypemap ** Create_AttrTypemap()
{
	
}


size_t fetch_ldap_typemap(AttrListLdap*** attrlist, LDAP *ld, char *object_class, char *schema_dn)
{
	size_t size = 0;
	char *a = NULL, * schema_entry_str = NULL;
	char **names = NULL;
	int rc = 0, oclass_error = 0;
	const char * oclass_error_text;
	struct berval **vals = NULL;
	LDAPMessage *schema = NULL, *entry = NULL;
	BerElement *ber;
	rc = ldap_search_ext_s(
		ld,
		schema_dn,
		LDAP_SCOPE_BASE, //LDAP_SCOPE_SUBTREE, //LDAP_SCOPE_BASE,
		/*schema_filter, */ "(objectClass=*)",
		(char*[]){ "attributeTypes", "objectClasses", NULL }, //(char*[]){ NULL },
		0,
		NULL,
		NULL,
		&timeout_struct,
		1000000,
		&schema );
	if (rc != LDAP_SUCCESS) {
		return -1;
	}
	
	elog(INFO, "ObjectClass: %s", object_class);
	
	for (entry = ldap_first_entry(ld, schema); entry != NULL; entry = ldap_next_entry(ld, entry)) {
		schema_entry_str = ldap_get_dn(ld, entry);
		elog(INFO, "Schema Str: %s", schema_entry_str);
		ldap_memfree(schema_entry_str);
		for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL; a = ldap_next_attribute( ld, entry, ber ) ) {
			DEBUGPOINT;
			struct berval **vals = NULL;
			if ((vals = ldap_get_values_len( ld, entry, a)) != NULL ) {
				DEBUGPOINT;
				for (int i = 0; vals[i] != NULL; i++ ) {
					DEBUGPOINT;
					elog(INFO, "attr: %s, i: %d, val: %s", a, i, vals[i]->bv_val);
					if(!strcmp(a, "objectClasses")) {
						int oclass_error = 0;
						const char * oclass_error_text;
						LDAPObjectClass *oclass = ldap_str2objectclass(vals[i]->bv_val, &oclass_error, &oclass_error_text, LDAP_SCHEMA_ALLOW_ALL);
						names = oclass->oc_names;
						if(in_array(names, object_class)) {
							
							
							//*attributes = (char**)malloc(1);

							for(int attributes_must_size = 0; oclass->oc_at_oids_must[attributes_must_size] != NULL; attributes_must_size++) {
								//*attributes = realloc(*attributes, size + 1);
								//*attributes[size] = strdup(oclass->oc_at_oids_must[attributes_must_size]);
								elog(INFO, "Must: %s", oclass->oc_at_oids_must[attributes_must_size]);
								size++;
							}

							for(int attributes_may_size = 0; oclass->oc_at_oids_may[attributes_may_size] != NULL; attributes_may_size++) {
								//*attributes = realloc(*attributes, size + 1);
								//*attributes[size] = strdup(oclass->oc_at_oids_may[attributes_may_size]);
								elog(INFO, "May: %s", oclass->oc_at_oids_may[attributes_may_size]);
								size++;
							}
						}
						ldap_objectclass_free(oclass);
					}
					else if(!strcmp(a, "attributeTypes")) {
						int * attribute_error;
						const char *  attribute_error_text;
						LDAPAttributeType *attribute_data = ldap_str2attributetype(vals[i]->bv_val, &attribute_error, &attribute_error_text, LDAP_SCHEMA_ALLOW_NONE);
						names = attribute_data->at_names;
						AttrListLdap * attrData = (AttrListLdap *)malloc(sizeof(AttrListLdap)+1);
						attrData->attr_name = strdup(names[0]);
						attrData->ldap_type = strdup(attribute_data->at_oid);
						attrData->isarray = (attribute_data->at_single_value == 0);
						ldap_attributetype_free(attribute_data);
						elog(INFO, "Attribute %s: %s", attrData->attr_name, attrData->ldap_type);
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


size_t translate_AttrListLdap(AttrListPg*** attributesPg, AttrListLdap** attributesLd)
{
	size_t size = 0;
	return size;
}

size_t fetch_objectclass(char ***attributes, LDAP *ld, char * object_class, char * base_dn)
{
	size_t size = 0;
	char *a = NULL;
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

