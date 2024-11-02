#warning under work
size_t fetch_ldap_typemap_new(List* attrList, List* attributes, LDAP *ld, List* object_class, char *schema_dn)
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
	lappend(attributes, makeString("dn"));
	lappend(attributes, makeString("objectclass"));
	
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
						List *lnames = NIL;
						LDAPObjectClass *oclass = ldap_str2objectclass(vals[i]->bv_val, &oclass_error, &oclass_error_text, LDAP_SCHEMA_ALLOW_ALL);
						for(int ni = 0; oclass->oc_names[ni] != NULL; ni++)
						{
							lappend(names, makeString(oclass->oc_names[ni]));
						}
							
						if((intersect = list_intersection(names, object_class)) != NIL)
						{
							int new_size = 0;
							if(oclass != NULL && oclass->oc_at_oids_must != NULL)
							{
								for(int attributes_must_size = 0; oclass->oc_at_oids_must[attributes_must_size] != NULL; attributes_must_size++) {
									ListCell *lc;
									lappend(attributes, makeString(oclass->oc_at_oids_must[attributes_must_size]));
									foreach(lc, attrList)
									{
										
									}
									size++;
								}
							}
							if(oclass != NULL && oclass->oc_at_oids_may != NULL)
							{
								for(int attributes_may_size = 0; oclass->oc_at_oids_may[attributes_may_size] != NULL; attributes_may_size++) {
									lappend(attributes, makeString(oclass->oc_at_oids_may[attributes_may_size]));
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
							lappend(attrList, PointerGetDatum(attrData));
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

