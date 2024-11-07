static AttrListType * Build_SingleAttrListType(const char *, const char *, const char *, bool, bool);

AttrListType * Create_SingleAttrListType(void)
{
	AttrListType * attrData = (AttrListType *)malloc(sizeof(AttrListType) * 2);
	memset(attrData, 0, sizeof(AttrListType) * 2);
	return attrData;
}

AttrListType ** Create_AttrListType(void)
{
	AttrListType ** retval = (AttrListType **)malloc(sizeof(AttrListType*) * 2);
	memset(retval, 0, sizeof(AttrListType**) * 2);
	return retval;
}

AttrListType * Build_SingleAttrListType(const char * attr_name, const char * ldap_type, const char * pg_type, bool isarray, bool nullable)
{
	AttrListType * attrData = Create_SingleAttrListType();
	if(attr_name) attrData->attr_name = strdup(attr_name);
	if(ldap_type) attrData->ldap_type = strdup(ldap_type);
	if(pg_type) attrData->pg_type = strdup(pg_type);
	attrData->isarray = isarray;
	attrData->nullable = nullable;
	return attrData;
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
	free(*attrlist);
	*attrlist = NULL;
}

char * getAttrTypeByAttrName(AttrListType ***attrList, char * attrName)
{
	for(int i = 0; (*attrList)[i] != NULL; i++)
	{
		if(!strcmp((*attrList)[i]->attr_name, attrName)) return (*attrList)[i]->pg_type;
	}
	return "varchar";
}


/**
 * This is a Prototype for a generic C ldap library, after tests it will be adapted to Postgresql Datatypes like list, Hashmap ...
 */
size_t fetch_ldap_typemap(AttrListType *** attrList, char *** attributes, LDAP *ld, char **object_class, char *schema_dn)
{
	size_t size = 0;
	char *a = NULL;
	char **names = NULL;
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
	*attributes = (char**)malloc(sizeof(char*) * 2);
	memset(*attributes, 0, sizeof(char*) * 2);
	// Put the default columns into attributes array
	array_push(attributes, "dn");
	array_push(attributes, "objectclass");
	
	for (entry = ldap_first_entry(ld, schema); entry != NULL; entry = ldap_next_entry(ld, entry)) {
//		char * schema_entry_str = NULL;
// 		schema_entry_str = ldap_get_dn(ld, entry);
// 		elog(INFO, "Schema Str: %s", schema_entry_str);
// 		ldap_memfree(schema_entry_str);
		for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL; a = ldap_next_attribute( ld, entry, ber ) ) {
			struct berval **vals = NULL;
			if ((vals = ldap_get_values_len( ld, entry, a)) != NULL ) {
				for (int i = 0; vals[i] != NULL; i++ ) {
// 					elog(INFO, "attr: %s, i: %d, val: %s", a, i, vals[i]->bv_val);
					if(!strcmp(a, "objectClasses")) {
						int oclass_error = 0;
						const char * oclass_error_text;
						LDAPObjectClass *oclass = ldap_str2objectclass(vals[i]->bv_val, &oclass_error, &oclass_error_text, LDAP_SCHEMA_ALLOW_ALL);
						names = oclass->oc_names;
						//if(in_array(names, object_class))
						if(array_has_intersect(names, object_class))
						{
							int new_size = 0;
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
						int attribute_error;
						const char *  attribute_error_text;
						LDAPAttributeType *attribute_data = ldap_str2attributetype(vals[i]->bv_val, &attribute_error, &attribute_error_text, LDAP_SCHEMA_ALLOW_NONE);
						int new_size = 0;
						names = attribute_data->at_names;
						
						for(int attrI = 0; names[attrI] != NULL; attrI++)
						{
							AttrListType * attrData = Build_SingleAttrListType (
								names[attrI],
								attribute_data->at_oid,
								NULL,
								(attribute_data->at_single_value == 0),
								true
							);
							new_size = AttrListTypeAppend(attrList, attrData);
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


size_t fill_AttrListType(AttrListType*** attributes, AttrTypemap map[])
{
	size_t size = 0;
	if(*attributes == 0) return 0;
	while((*attributes)[size] != NULL)
	{
		(*attributes)[size]->pg_type = strdup(getPgTypeForLdapType(map, (*attributes)[size]->ldap_type));
		if((*attributes)[size]->isarray)
		{
			(*attributes)[size]->pg_type = realloc(((*attributes)[size]->pg_type), strlen((*attributes)[size]->pg_type) + 3);
			strcat((*attributes)[size]->pg_type, "[]");
		}
//		elog(INFO, "pg_type: %s", (*attributes)[size]->pg_type);
		size++;
	}
	return size;
}


