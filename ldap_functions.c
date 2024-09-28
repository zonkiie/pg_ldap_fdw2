#include <ldap_functions.h>
#include "helper_functions.h"
#include "utils/elog.h"

#define DEBUGPOINT elog(INFO, "ereport File %s, Func %s, Line %d\n", __FILE__, __FUNCTION__, __LINE__)

int common_ldap_bind(LDAP *ld, const char *username, const char *password, int use_sasl)
{
	//_cleanup_berval_ 
	struct berval *berval_password = NULL;
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

int append_ldap_mod(LDAPMod ***insert_data_all, LDAPMod *insert_data_entry)
{
	int current_count = 0;
	if(insert_data_entry == NULL) return -1;
	if(*insert_data_all == NULL)
	{
		*insert_data_all = (LDAPMod**)calloc(sizeof(LDAPMod*), 2);
	}
	else
	{
		current_count = LDAPMod_count(*insert_data_all);
		*insert_data_all = (LDAPMod**)realloc(*insert_data_all, sizeof(LDAPMod*) * (current_count + 2));
		(*insert_data_all)[current_count] = NULL;
	}
	(*insert_data_all)[current_count] = copy_ldap_mod(insert_data_entry);
	(*insert_data_all)[current_count + 1] = NULL;
	return current_count + 1;
	
}

int LDAPMod_count(LDAPMod ** array)
{
	int count = 0;
	if(array != NULL)
	{
		for(;array[count] != NULL; count++);
	}
	return count;
}

LDAPMod * create_new_ldap_mod()
{
	LDAPMod * new_value = (LDAPMod*)malloc(sizeof(LDAPMod));
	memset(new_value, 0, sizeof(LDAPMod));
	return new_value;
}

LDAPMod * copy_ldap_mod(LDAPMod * ldap_mod)
{
	LDAPMod * new_value = create_new_ldap_mod();
	if(ldap_mod == NULL) return NULL;
	new_value->mod_op = ldap_mod->mod_op;
	new_value->mod_type = strdup(ldap_mod->mod_type);
	//struct berval *value = ber_bvdup(*(ldap_mod->mod_bvalues));
	//new_value->mod_bvalues = &value;
	new_value->mod_values = array_copy(ldap_mod->mod_values);
	return new_value;
}

LDAPMod * construct_new_ldap_mod(int mod_op, char * type, char ** values)
{
	LDAPMod * new_value = create_new_ldap_mod();
	new_value->mod_op = mod_op;
	new_value->mod_type = strdup(type);
	if(values != NULL) new_value->mod_values = array_copy(values);
	else new_value->mod_values = NULL;
	return new_value;
}

void free_ldap_mod(LDAPMod * ldap_mod)
{
	free(ldap_mod->mod_type);
	//ber_bvfree(*(ldap_mod->mod_bvalues));
	if(ldap_mod->mod_values != NULL) 
	{
		free_carr_n(&(ldap_mod->mod_values));
		ldap_mod->mod_values = NULL;
	}
	free(ldap_mod);
}

/**
 * NOTE: This Method does not work, why?
 */
char * ldap_dn2filter(char *dn)
{
	if(dn == NULL) return NULL;
	else
	{
		char *retval = calloc(sizeof(char*), 1);
		strmcat_multi(&retval, "(&");
		char **dn_els = ldap_explode_dn(dn, 0);
		for(int i = 0; dn_els[i] != NULL; i++)
		{
			
			//char **rdn_els = ldap_explode_rdn(dn_els[i], 0);
			char **rdn_els = NULL;
			str_split(&rdn_els, dn_els[i], "=");
			if(rdn_els == NULL) continue;
			strmcat_multi(&retval, "(", rdn_els[0], ":dn:=", rdn_els[1], ")");
			//ldap_value_free(rdn_els);
			free_carr_n(&rdn_els);
		}
		ldap_value_free(dn_els);
		strmcat_multi(&retval, ")");
		return retval;
	}
}

int fetch_attribute_type()
{
	return 0;
}

int fetch_schema(LDAP *ld)
{
	return 0;
}
