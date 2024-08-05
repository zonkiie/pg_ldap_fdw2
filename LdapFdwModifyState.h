#ifndef __ldapfdwmodifystate__
#define __ldapfdwmodifystate__

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

	LDAP *ldap;	/* MongoDB connection */

	LdapFdwOptions *options;
	AttrNumber	rowidAttno;		/* attnum of resjunk rowid column */

	/* Join/Upper relation information */
	uint32		relType;		/* relation type.  Base, Join, Upper, or Upper
								 * on join */
	char	   *outerRelName;	/* Outer relation name */
} LdapFdwModifyState;

#endif
