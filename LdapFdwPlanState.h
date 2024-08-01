#ifndef __ldapfdwplanstate__
#define __ldapfdwplanstate__

/*
 * FDW-specific information for RelOptInfo.fdw_private.
 */
typedef struct LdapFdwPlanState
{
	BlockNumber pages;          /* estimate of file's physical size */
	int         ntuples;        /* estimate of number of data rows  */
	int         row;
	int         rc;
	int         msgid;
	int         num_attrs;
	List	   *local_conds;
	List	   *remote_conds;
	char       **columns;
	AttInMetadata *attinmeta;
	LDAPMessage   *ldap_message_result;
	LDAPMessage	  *msg;
} LdapFdwPlanState;

#endif
