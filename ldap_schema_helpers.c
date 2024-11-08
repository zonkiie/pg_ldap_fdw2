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
static AttrListType * Build_SingleAttrListType(const char *, const char *, const char *, bool, bool);

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


/**
 * Check wether list2 values are contained in list1
 */
List * list_str_value_intersection(List * list1, List * list2)
{
	List * result = NIL;
	ListCell * lc1 = NULL, * lc2 = NULL;
	foreach(lc1, list1)
	{
		char * value1 = lfirst(lc1);
		foreach(lc2, list2)
		{
			char * value2 = lfirst(lc2);
			if(!strcmp(value1, value2)) result = lappend(result, pstrdup(value1));
		}
	}
	return result;
}

List* Create_AttrListType(void)
{
	List * retval = NIL;
	return retval;
}

AttrListType * Create_SingleAttrListType(void)
{
	AttrListType * attrData = (AttrListType *)palloc(sizeof(AttrListType) * 2);
	memset(attrData, 0, sizeof(AttrListType) * 2);
	return attrData;
}

AttrListType * Build_SingleAttrListType(const char * attr_name, const char * ldap_type, const char * pg_type, bool isarray, bool nullable)
{
	AttrListType * attrData = Create_SingleAttrListType();
	if(attr_name) attrData->attr_name = pstrdup(attr_name);
	if(ldap_type) attrData->ldap_type = pstrdup(ldap_type);
	if(pg_type) attrData->pg_type = pstrdup(pg_type);
	attrData->isarray = isarray;
	attrData->nullable = nullable;
	return attrData;
}

size_t AttrListTypeAppend(List* attrlist, AttrListType *value)
{
	lappend(attrlist, value);
}

void AttrListTypeFreeSingle(AttrListType **value)
{
	if((*value)->attr_name != NULL)
	{
		pfree((*value)->attr_name);
		(*value)->attr_name = NULL;
	}
	if((*value)->ldap_type != NULL)
	{
		pfree((*value)->ldap_type);
		(*value)->ldap_type = NULL;
	}
	if((*value)->pg_type != NULL)
	{
		pfree((*value)->pg_type);
		(*value)->pg_type = NULL;
	}
	pfree(*value);
}

void AttrListTypeFree(List * attrlist)
{
	ListCell *lc;
	if(attrlist == NULL || attrlist == NIL) return;
	foreach(lc, attrlist)
	{
		AttrListType *value = (AttrListType*)lfirst(lc);
		AttrListTypeFreeSingle(&value);
	}
}

char * getAttrTypeByAttrName(List *attrList, char * attrName)
{
	ListCell *lc;
	if(attrList == NULL || attrList == NIL) return NULL;
	foreach(lc, attrList)
	{
		AttrListType *value = (AttrListType*)lfirst(lc);
		if(!strcmp(value->attr_name, attrName)) return value->pg_type;
	}
	return "varchar";
}

bool getAttrNullableByAttrName(List *attrList, char * attrName)
{
	ListCell *lc;
	if(attrList == NULL || attrList == NIL) return false;
	foreach(lc, attrList)
	{
		AttrListType *value = (AttrListType*)lfirst(lc);
		if(!strcmp(value->attr_name, attrName)) return value->nullable;
	}
	return true;
}


#warning under work
size_t fetch_ldap_typemap(List** attrList, List** attributes, LDAP *ld, List* object_class, char *schema_dn)
{
	size_t size = 0;
	char *a = NULL;
	int rc = 0;
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
	
	// Put the default columns into attributes list
	*attributes = lappend(*attributes, pstrdup("dn"));
	*attributes = lappend(*attributes, pstrdup("objectclass"));
	
	for (entry = ldap_first_entry(ld, schema); entry != NULL; entry = ldap_next_entry(ld, entry))
	{
		for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL; a = ldap_next_attribute( ld, entry, ber ) )
		{
			struct berval **vals = NULL;
			if ((vals = ldap_get_values_len( ld, entry, a)) != NULL )
			{
				for (int i = 0; vals[i] != NULL; i++ )
				{
					if(!strcmp(a, "objectClasses"))
					{
						int oclass_error = 0;
						const char * oclass_error_text;
						List *intersect = NIL;
						List *names = NIL;
						ListCell * lc = NULL;
						LDAPObjectClass *oclass = ldap_str2objectclass(vals[i]->bv_val, &oclass_error, &oclass_error_text, LDAP_SCHEMA_ALLOW_ALL);
						for(int ni = 0; oclass->oc_names[ni] != NULL; ni++) names = lappend(names, pstrdup(oclass->oc_names[ni]));
						
						//if((intersect = list_intersection(names, object_class)) != NIL)
						if((intersect = list_str_value_intersection(names, object_class)) != NIL)
						{
							int new_size = 0;
							if(oclass != NULL && oclass->oc_at_oids_must != NULL)
							{
								for(int attributes_must_size = 0; oclass->oc_at_oids_must[attributes_must_size] != NULL; attributes_must_size++) {
									ListCell *lc;
									*attributes = lappend(*attributes, pstrdup(oclass->oc_at_oids_must[attributes_must_size]));
									foreach(lc, *attrList)
									{
										AttrListType *value = (AttrListType*)lfirst(lc);
										if(!strcmp(value->attr_name, oclass->oc_at_oids_must[attributes_must_size]))
										{
											value->nullable = false;
										}
									}
									size++;
								}
							}
							if(oclass != NULL && oclass->oc_at_oids_may != NULL)
							{
								for(int attributes_may_size = 0; oclass->oc_at_oids_may[attributes_may_size] != NULL; attributes_may_size++) {
									*attributes = lappend(*attributes, pstrdup(oclass->oc_at_oids_may[attributes_may_size]));
									size++;
								}
							}
						}
						ldap_objectclass_free(oclass);
					}
					else if(!strcmp(a, "attributeTypes")) {
						int attribute_error;
						const char *  attribute_error_text;
						LDAPAttributeType *attribute_data = ldap_str2attributetype(vals[i]->bv_val, &attribute_error, &attribute_error_text, LDAP_SCHEMA_ALLOW_NONE);
						int new_size = 0;
						char ** names = attribute_data->at_names;
						
						for(int attrI = 0; names[attrI] != NULL; attrI++)
						{
							AttrListType * attrData = Build_SingleAttrListType (
								names[attrI],
								attribute_data->at_oid,
								NULL,
								(attribute_data->at_single_value == 0),
								true
							);
							*attrList = lappend(*attrList, attrData);
						}
										
						ldap_attributetype_free(attribute_data);
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
    
	// Cleanup
	ldap_msgfree(schema);
	
	return size;
}

size_t fill_AttrListType(List* attributes, AttrTypemap map[])
{
	size_t size = 0;
	ListCell *lc;
	if(attributes == NULL || attributes == NIL) return 0;
	foreach(lc, attributes)
	{
		AttrListType *value = (AttrListType*)lfirst(lc);
		value->pg_type = pstrdup(getPgTypeForLdapType(map, value->ldap_type));
		if(value->isarray) pstrmcat(&(value->pg_type), "[]"); 
		size++;
	}
	return size;
}
