#ifndef __ldapfdwtypes__
#define __ldapfdwtypes__

#include <stdlib.h>
#include <unistd.h>
#include <ldap.h>
#include <ldap_schema.h>
#include <sasl/sasl.h>
#include "helper_functions.h"

typedef struct LdapFdwOptions
{
	char * uri;
	char * username;
	char * password;
	char * basedn;
	char * filter;
	char * objectclass;
	char * schemadn;
	int scope;
	int use_sasl;
} LdapFdwOptions;

typedef struct LdapFdwConn
{
	LDAP *ldap;
	LDAPControl **serverctrls;
	LDAPControl **clientctrls;
	LdapFdwOptions *options;
} LdapFdwConn;

/*
 * FDW-specific information for RelOptInfo.fdw_private.
 */
typedef struct LdapFdwModifyState
{
	Relation	rel;			/* relcache entry for the foreign table */
	List	   *target_attrs;	/* list of target attribute numbers */
	List	   *retrieved_attrs;

	/* Info about parameters for prepared statement */
	int			p_nums;			/* number of parameters to transmit */
	FmgrInfo   *p_flinfo;		/* output conversion functions for them */

	struct HTAB *columnMappingHash;

	LdapFdwConn  *ldapConn;

	AttrNumber	rowidAttno;		/* attnum of resjunk rowid column */

	/* Join/Upper relation information */
	uint32		relType;		/* relation type.  Base, Join, Upper, or Upper
								 * on join */
	char	   *outerRelName;	/* Outer relation name */
	MemoryContext	temp_cxt;
} LdapFdwModifyState;

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
	char	   **column_types;
	Oid		    *column_type_ids;
	AttInMetadata *attinmeta;
	LDAPMessage   *ldap_message_result;
	LDAPMessage	  *msg;
	LdapFdwConn  *ldapConn;
} LdapFdwPlanState;

#endif
