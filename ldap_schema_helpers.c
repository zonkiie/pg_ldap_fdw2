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

static char* getPgTypeForLdapType(AttrTypemap map[], char *);

static char* getPgTypeForLdapType(AttrTypemap map[], char *ldapType)
{
	int i = 0;
	while(/*map[i] != NULL &&*/ map[i].ldap_type != NULL)
	{
		if(!strcmp(map[i].ldap_type, ldapType)) return map[i].pg_type;
		i++;
	}
	// if nothing is found, fall back to varchar
	return "varchar";
}

AttrListType ** Create_AttrListType()
{
	AttrListType ** retval = (AttrListType **)malloc(sizeof(AttrListType**) * 2);
	memset(retval, 0, sizeof(AttrListType**) * 2);
	return retval;
}

size_t AttrListTypeCount(AttrListType*** attrlist)
{
	if(attrlist == NULL || *attrlist == NULL) return 0;
	else
	{
		size_t count = 0;
		while((*attrlist)[count] != NULL) count++;
		return count;
	}
}

size_t AttrListTypeAppend(AttrListType*** attrlist, AttrListType *value)
{
	size_t current_size = AttrListTypeCount(attrlist);
	*attrlist = realloc(*attrlist, sizeof(AttrListType**) * (current_size + 2));
	(*attrlist)[current_size] = value;
	(*attrlist)[current_size + 1] = NULL;
	return AttrListTypeCount(attrlist);
}

void AttrListTypeFreeSingle(AttrListType **value)
{
	if((*value)->attr_name != NULL)
	{
		free((*value)->attr_name);
		(*value)->attr_name = NULL;
	}
	if((*value)->ldap_type != NULL)
	{
		free((*value)->ldap_type);
		(*value)->ldap_type = NULL;
	}
	if((*value)->pg_type != NULL)
	{
		free((*value)->pg_type);
		(*value)->pg_type = NULL;
	}
	free(*value);
}

void AttrListTypeFree(AttrListType*** attrlist)
{
	for(int i = 0; (*attrlist)[i] != NULL; i++)
	{
		AttrListTypeFreeSingle(&(*attrlist)[i]);
		(*attrlist)[i] = NULL;
	}
	*attrlist = NULL;
}

/**
 * This is a Prototype for a generic C ldap library, after tests it will be adapted to Postgresql Datatypes like list, Hashmap ...
 */
size_t fetch_ldap_typemap(AttrListType *** attrList, char *** attributes, LDAP *ld, char *object_class, char *schema_dn)
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
	
	for (entry = ldap_first_entry(ld, schema); entry != NULL; entry = ldap_next_entry(ld, entry)) {
// 		schema_entry_str = ldap_get_dn(ld, entry);
// 		elog(INFO, "Schema Str: %s", schema_entry_str);
// 		ldap_memfree(schema_entry_str);
		for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL; a = ldap_next_attribute( ld, entry, ber ) ) {
			struct berval **vals = NULL;
			if ((vals = ldap_get_values_len( ld, entry, a)) != NULL ) {
				for (int i = 0; vals[i] != NULL; i++ ) {
					//elog(INFO, "attr: %s, i: %d, val: %s", a, i, vals[i]->bv_val);
					if(!strcmp(a, "objectClasses")) {
						int oclass_error = 0;
						const char * oclass_error_text;
						LDAPObjectClass *oclass = ldap_str2objectclass(vals[i]->bv_val, &oclass_error, &oclass_error_text, LDAP_SCHEMA_ALLOW_ALL);
						names = oclass->oc_names;
						if(in_array(names, object_class))
						{
							int new_size = 0;
							*attributes = (char**)malloc(sizeof(char*) * 2);
							memset(*attributes, 0, sizeof(char*) * 2);
							if(oclass != NULL && oclass->oc_at_oids_must != NULL)
							{
								for(int attributes_must_size = 0; oclass->oc_at_oids_must[attributes_must_size] != NULL; attributes_must_size++) {
									new_size = array_push(attributes, oclass->oc_at_oids_must[attributes_must_size]);
									size++;
								}
							}
							if(oclass != NULL && oclass->oc_at_oids_may != NULL)
							{
								for(int attributes_may_size = 0; oclass->oc_at_oids_may[attributes_may_size] != NULL; attributes_may_size++) {
									new_size = array_push(attributes, oclass->oc_at_oids_may[attributes_may_size]);
									size++;
								}
							}
						}
						ldap_objectclass_free(oclass);
					}
					else if(!strcmp(a, "attributeTypes")) {
						int * attribute_error;
						const char *  attribute_error_text;
						LDAPAttributeType *attribute_data = ldap_str2attributetype(vals[i]->bv_val, &attribute_error, &attribute_error_text, LDAP_SCHEMA_ALLOW_NONE);
						int new_size = 0;
						names = attribute_data->at_names;
						AttrListType * attrData = (AttrListType *)malloc(sizeof(AttrListType) * 2);
						memset(attrData, 0, sizeof(AttrListType) * 2);
						attrData->attr_name = strdup(names[0]);
						attrData->ldap_type = strdup(attribute_data->at_oid);
						attrData->isarray = (attribute_data->at_single_value == 0);
						ldap_attributetype_free(attribute_data);
						new_size = AttrListTypeAppend(attrList, attrData);
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


size_t fill_AttrListType(AttrListType*** attributes, AttrTypemap map[])
{
	size_t size = 0;
	while((*attributes)[size] != NULL)
	{
		(*attributes)[size]->pg_type = strdup(getPgTypeForLdapType(map, (*attributes)[size]->ldap_type));
		size++;
	}
	return size;
}
