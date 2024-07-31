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
	List	   *local_conds;
	List	   *remote_conds;
	AttInMetadata *attinmeta;
	LDAPMessage   *res;
} LdapFdwPlanState;

#endif
