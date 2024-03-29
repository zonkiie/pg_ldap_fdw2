/*
 * Include all neccessary headers
 * The frame is form fdw_dummy.c and will be reworked step by step
 */

#include "postgres.h"
// #include "postgres_fdw.h"
#include "access/sysattr.h"
#include "nodes/pg_list.h"
#include "nodes/makefuncs.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_type.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "commands/vacuum.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
//#include "optimizer/var.h"
#include "optimizer/optimizer.h"
#include "parser/parsetree.h"
#include "optimizer/restrictinfo.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"


#include <syslog.h>


#include "LdapFdwOptions.h"
#include "helper_functions.h"
#include "ldap_functions.h"

PG_MODULE_MAGIC;

#define MODULE_PREFIX ldap2_fdw

#define LDAP2_FDW_LOGFILE "/dev/shm/ldap2_fdw.log"

#define DEBUGPOINT ereport(LOG, errmsg_internal("ereport Func %s, Line %d\n", __FUNCTION__, __LINE__))

void GetOptionStructr(LdapFdwOptions *, Oid);
void print_list(FILE *, List *);
void initLdap();
void bindLdap();

extern Datum ldap2_fdw_handler(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(ldap2_fdw_handler);

void		_PG_init(void);
void		_PG_fini(void);

static void ldap2_fdw_GetForeignRelSize(PlannerInfo *root,
						   RelOptInfo *baserel,
						   Oid foreigntableid);
static void ldap2_fdw_GetForeignPaths(PlannerInfo *root,
						 RelOptInfo *baserel,
						 Oid foreigntableid);
#if (PG_VERSION_NUM <= 90500)
static ForeignScan *ldap2_fdw_GetForeignPlan(PlannerInfo *root,
						RelOptInfo *baserel,
						Oid foreigntableid,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses);

#else
static ForeignScan *ldap2_fdw_GetForeignPlan(PlannerInfo *root,
						RelOptInfo *baserel,
						Oid foreigntableid,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses,
						Plan *outer_plan);
#endif
static void ldap2_fdw_BeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *ldap2_fdw_IterateForeignScan(ForeignScanState *node);
static void ldap2_fdw_ReScanForeignScan(ForeignScanState *node);
static void ldap2_fdw_EndForeignScan(ForeignScanState *node);


/*
 * FDW callback routines
 */
static void ldap2_fdw_AddForeignUpdateTargets(Query *parsetree,
								RangeTblEntry *target_rte,
								Relation target_relation);
static List *ldap2_fdw_PlanForeignModify(PlannerInfo *root,
						  ModifyTable *plan,
						  Index resultRelation,
						  int subplan_index);
static void ldap2_fdw_BeginForeignModify(ModifyTableState *mtstate,
						   ResultRelInfo *resultRelInfo,
						   List *fdw_private,
						   int subplan_index,
						   int eflags);
static TupleTableSlot *ldap2_fdw_ExecForeignInsert(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot);
static TupleTableSlot *ldap2_fdw_ExecForeignUpdate(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot);
static TupleTableSlot *ldap2_fdw_ExecForeignDelete(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot);
static void ldap2_fdw_EndForeignModify(EState *estate,
						 ResultRelInfo *resultRelInfo);
static int	ldap2_fdw_IsForeignRelUpdatable(Relation rel);
static void ldap2_fdw_ExplainForeignScan(ForeignScanState *node,
						   ExplainState *es);
static void ldap2_fdw_ExplainForeignModify(ModifyTableState *mtstate,
							 ResultRelInfo *rinfo,
							 List *fdw_private,
							 int subplan_index,
							 ExplainState *es);
static bool ldap2_fdw_AnalyzeForeignTable(Relation relation,
							AcquireSampleRowsFunc *func,
							BlockNumber *totalpages);
static int ldap2_fdw_AcquireSampleRowsFunc(Relation relation, int elevel,
							  HeapTuple *rows, int targrows,
							  double *totalrows,
							  double *totaldeadrows);

/*
 * FDW-specific information for RelOptInfo.fdw_private.
 */
typedef struct LdapFdwPlanState
{
	BlockNumber pages;          /* estimate of file's physical size */
	double      ntuples;        /* estimate of number of data rows  */
} LdapFdwPlanState;


/* magic */
enum FdwScanPrivateIndex
{
    /* SQL statement to execute remotely (as a String node) */
    FdwScanPrivateSelectSql,
    /* Integer list of attribute numbers retrieved by the SELECT */
    FdwScanPrivateRetrievedAttrs
};
/*
 * Similarly, this enum describes what's kept in the fdw_private list for
 * a ModifyTable node referencing a postgres_fdw foreign table.  We store:
 *
 * 1) INSERT/UPDATE/DELETE statement text to be sent to the remote server
 * 2) Integer list of target attribute numbers for INSERT/UPDATE
 *    (NIL for a DELETE)
 * 3) Boolean flag showing if there's a RETURNING clause
 * 4) Integer list of attribute numbers retrieved by RETURNING, if any
 */
enum FdwModifyPrivateIndex
{
    /* SQL statement to execute remotely (as a String node) */
    FdwModifyPrivateUpdateSql,
    /* Integer list of target attribute numbers for INSERT/UPDATE */
    FdwModifyPrivateTargetAttnums,
    /* has-returning flag (as an integer Value node) */
    FdwModifyPrivateHasReturning,
    /* Integer list of attribute numbers retrieved by RETURNING */
    FdwModifyPrivateRetrievedAttrs
};

//_cleanup_ldap_ LDAP *ld = NULL;
LDAP *ld = NULL;
//_cleanup_options_ LdapFdwOptions *option_params = (LdapFdwOptions *)calloc(1, sizeof(LdapFdwOptions *));
LdapFdwOptions *option_params = NULL;
LDAPControl **serverctrls = NULL;
LDAPControl **clientctrls = NULL;
char ** attributes_array = NULL;
//_cleanup_ldap_message_ LDAPMessage *res = NULL;
LDAPMessage *res = NULL;
char *dn, *matched_msg = NULL, *error_msg = NULL;
struct timeval timeout_struct = {.tv_sec = 10L, .tv_usec = 0L};
int version, msgid, rc, parse_rc, finished = 0, msgtype, num_entries = 0, num_refs = 0;

void GetOptionStructr(LdapFdwOptions * options, Oid foreignTableId)
{
	ForeignTable *foreignTable = NULL;
	ForeignServer *foreignServer = NULL;
	UserMapping *mapping = NULL;
	ListCell *cell = NULL;
	List * all_options = NULL;
	
	DEBUGPOINT;
	if(options == NULL) {
		ereport(ERROR,
			(errcode(ERRCODE_FDW_INVALID_USE_OF_NULL_POINTER),
			errmsg("options is null!"),
			errhint("options is null - variable is not initialized!"))
		);
		return;
	}
	foreignTable = GetForeignTable(foreignTableId);
	foreignServer = GetForeignServer(foreignTable->serverid);
	mapping = GetUserMapping(GetUserId(), foreignTable->serverid);
	
	all_options = list_copy(foreignTable);
	all_options = list_concat(all_options, foreignServer);
	all_options = list_concat(all_options, mapping);
	
	//foreach(cell, foreignTable->options)
	foreach(cell, all_options)
	{
		DefElem *def = lfirst_node(DefElem, cell);
		
		ereport(LOG, errmsg_internal("ereport Func %s, Line %d, def: %s\n", __FUNCTION__, __LINE__, def->defname));
		
		if (strcmp("uri", def->defname) == 0)
		{
			options->uri = defGetString(def);
		}
		else if(strcmp("hostname", def->defname) == 0)
		{
			char * hostname = defGetString(def);
			options->uri = psprintf("ldap://%s", hostname);
			free(hostname);
		}
		else if (strcmp("username", def->defname) == 0)
		{
			options->username = defGetString(def);
		}
		else if (strcmp("password", def->defname) == 0)
		{
			ereport(LOG, errmsg_internal("GetOptionStructr password %s\n", options->password));
			options->password = defGetString(def);
		}
		else if (strcmp("basedn", def->defname) == 0)
		{
			options->basedn = defGetString(def);
		}
		else if (strcmp("filter", def->defname) == 0)
		{
			options->filter = defGetString(def);
		}
		else if (strcmp("objectclass", def->defname) == 0)
		{
			options->objectclass = defGetString(def);
		}
		else if (strcmp("schemadn", def->defname) == 0)
		{
			options->schemadn = defGetString(def);
		}
		else if(strcmp("scope", def->defname) == 0)
		{
			char *sscope = pstrdup(defGetString(def)); // strdup(def->arg);
			if(!strcasecmp(sscope, "LDAP_SCOPE_BASE")) options->scope = LDAP_SCOPE_BASE;
			else if(!strcasecmp(sscope, "LDAP_SCOPE_ONELEVEL")) options->scope = LDAP_SCOPE_ONELEVEL;
			else if(!strcasecmp(sscope, "LDAP_SCOPE_SUBTREE")) options->scope = LDAP_SCOPE_SUBTREE;
			else if(!strcasecmp(sscope, "LDAP_SCOPE_CHILDREN")) options->scope = LDAP_SCOPE_CHILDREN;
			else ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
				errmsg("invalid value \"%s\" for scope", sscope),
				errhint("Valid values for ldap2_fdw are \"LDAP_SCOPE_BASE\", \"LDAP_SCOPE_ONELEVEL\", \"LDAP_SCOPE_SUBTREE\", \"LDAP_SCOPE_CHILDREN\"."))
			);
			pfree(sscope);
		}
		else
		{
			ereport(LOG, errmsg_internal("%s ereport Line %d\n", __FUNCTION__, __LINE__));
			ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
				errmsg("invalid option \"%s\"", def->defname),
				errhint("Valid table options for ldap2_fdw are \"uri\", \"hostname\", \"username\", \"password\", \"basedn\", \"filter\", \"objectclass\", \"schemadn\", \"scope\""))
			);
		}
	}
	DEBUGPOINT;
	options->use_sasl = 0;
	ereport(LOG, errmsg_internal("GetOptionStructr ereport finished\n"));
}

void print_list(FILE *out_channel, List *list)
{
	for(int i = 0; i < list->length; i++)
	{
		fprintf(out_channel, "%s\n", (char*)list->elements[i].ptr_value);
	}
}

static int estimate_size(LDAP *ldap, const char *basedn, const char *filter, int scope)
{
	DEBUGPOINT;
	int rows = 0;
	finished = 0;
	rc = ldap_search_ext( ld, basedn, scope, filter, (char *[]){"objectClass"}, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
	if ( rc != LDAP_SUCCESS )
	{
		if ( error_msg != NULL && *error_msg != '\0' )
		{

			fprintf( stderr, "%s\n", error_msg );
			ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not exec ldap_search_ext on \"%s\" with filer \"%s\"", basedn, filter),
				errhint("Could not bind to ldap server. Is username and password correct?"))
			);
		}
	}
	while(!finished)
	{
		rc = ldap_result( ld, msgid, LDAP_MSG_ONE, &zerotime, &res );
		switch( rc )
		{
			case LDAP_RES_SEARCH_ENTRY:
				rows++;
				break;
		}
		ldap_msgfree(res);
		res = NULL;
	}
	return rows;
}

static void estimate_costs(PlannerInfo *root, RelOptInfo *baserel,
			   LdapFdwPlanState *fdw_private,
			   Cost *startup_cost, Cost *total_cost)
{
	DEBUGPOINT;
	BlockNumber pages = fdw_private->pages;
	double		ntuples = fdw_private->ntuples;
	Cost		run_cost = 0;
	Cost		cpu_per_tuple;

	/*
	 * We estimate costs almost the same way as cost_seqscan(), thus assuming
	 * that I/O costs are equivalent to a regular table file of the same size.
	 * However, we take per-tuple CPU costs as 10x of a seqscan, to account
	 * for the cost of parsing records.
	 *
	 * In the case of a program source, this calculation is even more divorced
	 * from reality, but we have no good alternative; and it's not clear that
	 * the numbers we produce here matter much anyway, since there's only one
	 * access path for the rel.
	 */
	 /*
	  * Cost Code copied from csv-fdw
	 */
	run_cost += seq_page_cost * pages;

	*startup_cost = baserel->baserestrictcost.startup;
	cpu_per_tuple = cpu_tuple_cost * 10 + baserel->baserestrictcost.per_tuple;
	run_cost += cpu_per_tuple * ntuples;
	*total_cost = *startup_cost + run_cost;
}


void initLdap()
{
	DEBUGPOINT;
	
	if(option_params == NULL)
	{
		DEBUGPOINT;
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
				errmsg("option_params is null!"),
				errhint("ldap option params is not initialized!")));
		return;
	}
	
	ereport(LOG, errmsg_internal("initLdap uri: %s, username: %s, password %s\n", option_params->uri, option_params->username, option_params->password));

	
	int version;
	//option_params = (LdapFdwOptions *)malloc(sizeof(LdapFdwOptions *));
	version = LDAP_VERSION3;
	
	if(option_params->uri == NULL || !strcmp(option_params->uri, ""))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
				errmsg("URI is empty!"),
				errhint("LDAP URI is not given or has empty value!")));
		return;
	}

	if ( ( rc = ldap_initialize( &ld, option_params->uri ) ) != LDAP_SUCCESS )
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				errmsg("Could not establish connection to \"%s\"", option_params->uri),
				errhint("Check that ldap server runs, accept connections and can be reached.")));
		return;
	}

	if ( ( rc = ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version ) ) != LDAP_SUCCESS )
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not set ldap version option!"),
				errhint("Could not set ldap version option. Does ldap server accept the correct ldap version 3?")));
		return;
	}

	// removed ldap bind - call bind from another function
	bindLdap();
}

void bindLdap()
{
	ereport(LOG, errmsg_internal("bindLdap username: %s, password %s\n", option_params->username, option_params->password));
	
	if ( ( rc = common_ldap_bind( ld, option_params->username, option_params->password, option_params->use_sasl) ) != LDAP_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not exec ldap bind to \"%s\" with username \"%s\"!", option_params->uri, option_params->username),
				errhint("Could not bind to ldap server. Is username and password correct?")));
		return;
	}
	
	DEBUGPOINT;
}

void _PG_init()
{
	DEBUGPOINT;
	option_params = (LdapFdwOptions *)palloc(sizeof(LdapFdwOptions *));
}

void _PG_fini()
{
	DEBUGPOINT;
	//free_ldap(&ld);
}

Datum ldap2_fdw_handler(PG_FUNCTION_ARGS)
{
	DEBUGPOINT;
	
    FdwRoutine *fdw_routine = makeNode(FdwRoutine);

    fdw_routine->GetForeignRelSize = ldap2_fdw_GetForeignRelSize;
    fdw_routine->GetForeignPaths = ldap2_fdw_GetForeignPaths;
    fdw_routine->GetForeignPlan = ldap2_fdw_GetForeignPlan;
    fdw_routine->ExplainForeignScan = ldap2_fdw_ExplainForeignScan;
    fdw_routine->ExplainForeignModify = ldap2_fdw_ExplainForeignModify;

    fdw_routine->BeginForeignScan = ldap2_fdw_BeginForeignScan;
    fdw_routine->IterateForeignScan = ldap2_fdw_IterateForeignScan;
    fdw_routine->ReScanForeignScan = ldap2_fdw_ReScanForeignScan;
    fdw_routine->EndForeignScan = ldap2_fdw_EndForeignScan;

    /* insert support */
    fdw_routine->AddForeignUpdateTargets = ldap2_fdw_AddForeignUpdateTargets;

    fdw_routine->PlanForeignModify = ldap2_fdw_PlanForeignModify;
    fdw_routine->BeginForeignModify = ldap2_fdw_BeginForeignModify;
    fdw_routine->ExecForeignInsert = ldap2_fdw_ExecForeignInsert;
    fdw_routine->ExecForeignUpdate = ldap2_fdw_ExecForeignUpdate;
    fdw_routine->ExecForeignDelete = ldap2_fdw_ExecForeignDelete;
    fdw_routine->EndForeignModify = ldap2_fdw_EndForeignModify;
    fdw_routine->IsForeignRelUpdatable = ldap2_fdw_IsForeignRelUpdatable;

    fdw_routine->AnalyzeForeignTable = ldap2_fdw_AnalyzeForeignTable;

	DEBUGPOINT;
    PG_RETURN_POINTER(fdw_routine);
}

/*
 * GetForeignRelSize
 *		set relation size estimates for a foreign table
 */
static void
ldap2_fdw_GetForeignRelSize(PlannerInfo *root,
						   RelOptInfo *baserel,
						   Oid foreigntableid)
{
	DEBUGPOINT;
	GetOptionStructr(option_params, foreigntableid);
	initLdap();
	baserel->rows = estimate_size(ld, option_params->basedn, option_params->filter, option_params->scope);
}

/*
 * GetForeignPaths
 *		create access path for a scan on the foreign table
 */
static void
ldap2_fdw_GetForeignPaths(PlannerInfo *root,
						 RelOptInfo *baserel,
						 Oid foreigntableid)
{
	DEBUGPOINT;
	
	Path	   *path;
	LdapFdwPlanState *fdw_private;
	Cost		startup_cost;
	Cost		total_cost;
	/* Fetch options */
	GetOptionStructr(option_params, foreigntableid);
	initLdap();
#if (PG_VERSION_NUM < 90500)
	path = (Path *) create_foreignscan_path(root, baserel,
						baserel->rows,
						10,
						0,
						NIL,
						NULL,
						NULL);
#else
	path = (Path *) create_foreignscan_path(root, baserel,
#if (PG_VERSION_NUM >= 90600)
						NULL,
#endif
						baserel->rows,
						10,
						0,
						NIL,
						NULL,
						NULL,
						NIL);
#endif
	fdw_private = (LdapFdwPlanState *) baserel->fdw_private;

	/* Estimate costs */
	estimate_costs(root, baserel, fdw_private, &startup_cost, &total_cost);
	/* Create a ForeignPath node and add it as only possible path */
	add_path(baserel, (Path *) create_foreignscan_path(root, baserel,
							NULL,		/* default pathtarget */
							baserel->rows,
							startup_cost,
							total_cost,
							NIL,		/* no pathkeys */
							NULL,		/* no outer rel either */
							NULL,      /* no extra plan */
							NIL));		/* no fdw_private data */
	
	// Eliminate Compiler warning
	if(path)
		;
}

/*
 * GetForeignPlan
 *	create a ForeignScan plan node
 */
#if (PG_VERSION_NUM <= 90500)
static ForeignScan *
ldap2_fdw_GetForeignPlan(PlannerInfo *root,
						RelOptInfo *baserel,
						Oid foreigntableid,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses)
{
	DEBUGPOINT;

	/* Fetch options */
	GetOptionStructr(option_params, foreigntableid);
	initLdap();

	//FILE * log_channel = stderr;
	_cleanup_file_ FILE * log_channel = fopen(LDAP2_FDW_LOGFILE, "a");

	fprintf(log_channel, "tlist\n");
	print_list(log_channel, tlist);

	fprintf(log_channel, "scan_clauses\n");
	print_list(log_channel, scan_clauses);
	free_file(&log_channel);

	Path	   *foreignPath;
	Index		scan_relid = baserel->relid;
	Datum    blob = 0;
	Const    *blob2 = makeConst(INTERNALOID, 0, 0,
                 sizeof(blob),
                 blob,
                 false, false);
	scan_clauses = extract_actual_clauses(scan_clauses, false);
	return make_foreignscan(tlist,
			scan_clauses,
			scan_relid,
			scan_clauses,
			(void *)blob2);
}
#else
static ForeignScan *
ldap2_fdw_GetForeignPlan(PlannerInfo *root,
						RelOptInfo *baserel,
						Oid foreigntableid,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses,
						Plan *outer_plan)
{
	DEBUGPOINT;
	//Path	   *foreignPath;
	Index		scan_relid;
	/* Fetch options */
	GetOptionStructr(option_params, foreigntableid);
	initLdap();

	scan_relid = baserel->relid;
	print_list(stderr, scan_clauses);
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	return make_foreignscan(tlist,
			scan_clauses,
			scan_relid, // baserel->relid,
			scan_clauses,
			NIL, // best_path->fdw_private,
			NIL,
			NIL,
			outer_plan);
}
#endif
/*
 * ExplainForeignScan
 *   no extra info explain plan
 */
/*
static void
ldap2_fdw_ExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
}
*/
/*
 * BeginForeignScan
 *   called during executor startup. perform any initialization
 *   needed, but not start the actual scan.
 */

static void
ldap2_fdw_BeginForeignScan(ForeignScanState *node, int eflags)
{
	DEBUGPOINT;
	ForeignScan *fsplan = (ForeignScan *) node->ss.ps.plan;
	LdapFdwPlanState *fsstate = (LdapFdwPlanState *) node->fdw_state;
	
	fsstate = (LdapFdwPlanState *) palloc0(sizeof(LdapFdwPlanState));
	node->fdw_state = (void *) fsstate;
	//fsstate->query = strVal(list_nth(fsplan->fdw_private, FdwScanPrivateSelectSql));
	//fsstate->retrieved_attrs = list_nth(fsplan->fdw_private, FdwScanPrivateRetrievedAttrs);
	// Todo: Convert plan to ldap filter
	// from:     dynamodb_fdw/dynamodb_impl.cpp line 800
	// LDAP search
	//rc = ldap_search_ext( ld, option_params->basedn, option_params->scope, filter, attributes_array, 0, serverctrls, clientctrls, NULL, LDAP_NO_LIMIT, &msgid );
}



/*
 * IterateForeignScan
 *		Retrieve next row from the result set, or clear tuple slot to indicate
 *		EOF.
 *   Fetch one row from the foreign source, returning it in a tuple table slot
 *    (the node's ScanTupleSlot should be used for this purpose).
 *  Return NULL if no more rows are available.
 */
static TupleTableSlot *
ldap2_fdw_IterateForeignScan(ForeignScanState *node)
{
	DEBUGPOINT;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	/*
	Relation rel;
	AttInMetadata  *attinmeta;
	HeapTuple tuple;
	LdapFdwPlanState *hestate = (LdapFdwPlanState *) node->fdw_state;
	int i;
	int natts;
	char **values;

	// ldap fetch result
	rc = ldap_result( ld, msgid, LDAP_MSG_ONE, &timeout_struct, &res );

	if( hestate->rownum != 0 ){
		ExecClearTuple(slot);
		return slot;
	}
	rel = node->ss.ss_currentRelation;
	attinmeta = TupleDescGetAttInMetadata(rel->rd_att);

	natts = rel->rd_att->natts;
	values = (char **) palloc(sizeof(char *) * natts);

	for(i = 0; i < natts; i++ ){
		values[i] = "Hello,World";
	}

	tuple = BuildTupleFromCStrings(attinmeta, values);
	ExecStoreTuple(tuple, slot, InvalidBuffer, true);

	hestate->rownum++;
	*/
	return slot;

}

/*
 * ReScanForeignScan
 *		Restart the scan from the beginning
 */
static void
ldap2_fdw_ReScanForeignScan(ForeignScanState *node)
{
	DEBUGPOINT;
}

/*
 *EndForeignScan
 *	End the scan and release resources.
 */
static void
ldap2_fdw_EndForeignScan(ForeignScanState *node)
{
	DEBUGPOINT;
	// cleanup
	free_ldap_message(&res);
}


/*
 * postgresAddForeignUpdateTargets
 *    Add resjunk column(s) needed for update/delete on a foreign table
 */
static void
ldap2_fdw_AddForeignUpdateTargets(Query *parsetree,
								RangeTblEntry *target_rte,
								Relation target_relation)
{
	DEBUGPOINT;
 Var      *var;
  const char *attrname;
  TargetEntry *tle;

/*
 * In postgres_fdw, what we need is the ctid, same as for a regular table.
 */

  /* Make a Var representing the desired value */
  var = makeVar(parsetree->resultRelation,
          SelfItemPointerAttributeNumber,
          TIDOID,
          -1,
          InvalidOid,
          0);

  /* Wrap it in a resjunk TLE with the right name ... */
  attrname = "ctid";

  tle = makeTargetEntry((Expr *) var,
              list_length(parsetree->targetList) + 1,
              pstrdup(attrname),
              true);

  /* ... and add it to the query's targetlist */
  parsetree->targetList = lappend(parsetree->targetList, tle);
}




/*
 * ldap2_fdw_PlanForeignModify
 *		Plan an insert/update/delete operation on a foreign table
 *
 * Note: currently, the plan tree generated for UPDATE/DELETE will always
 * include a ForeignScan that retrieves ctids (using SELECT FOR UPDATE)
 * and then the ModifyTable node will have to execute individual remote
 * UPDATE/DELETE commands.  If there are no local conditions or joins
 * needed, it'd be better to let the scan node do UPDATE/DELETE RETURNING
 * and then do nothing at ModifyTable.  Room for future optimization ...
 */
static List *
ldap2_fdw_PlanForeignModify(PlannerInfo *root,
						  ModifyTable *plan,
						  Index resultRelation,
						  int subplan_index)
{
	DEBUGPOINT;
/*
	CmdType		operation = plan->operation;
	RangeTblEntry *rte = planner_rt_fetch(resultRelation, root);
	Relation	rel;
*/
	List	   *targetAttrs = NIL;
	List	   *returningList = NIL;
	List	   *retrieved_attrs = NIL;

	StringInfoData sql;
	initStringInfo(&sql);

	/*
	 * Core code already has some lock on each rel being planned, so we can
	 * use NoLock here.
	 */
//	rel = heap_open(rte->relid, NoLock);

	/*
	 * In an INSERT, we transmit all columns that are defined in the foreign
	 * table.  In an UPDATE, we transmit only columns that were explicitly
	 * targets of the UPDATE, so as to avoid unnecessary data transmission.
	 * (We can't do that for INSERT since we would miss sending default values
	 * for columns not listed in the source statement.)
	 */
/*
	if (operation == CMD_INSERT)
	{
		TupleDesc	tupdesc = RelationGetDescr(rel);
		int			attnum;
		for (attnum = 1; attnum <= tupdesc->natts; attnum++)
		{
			Form_pg_attribute attr = tupdesc->attrs[attnum - 1];
			if (!attr->attisdropped)
				targetAttrs = lappend_int(targetAttrs, attnum);
		}
	}
	else if (operation == CMD_UPDATE)
	{
		Bitmapset  *tmpset = bms_copy(rte->modifiedCols);
		AttrNumber	col;
		while ((col = bms_first_member(tmpset)) >= 0)
		{
			col += FirstLowInvalidHeapAttributeNumber;
			if (col <= InvalidAttrNumber)		// shouldn't happen
				elog(ERROR, "system-column update is not supported");
			targetAttrs = lappend_int(targetAttrs, col);
		}
	}
*/

	/*
	 * Extract the relevant RETURNING list if any.
	 */
/*
	if (plan->returningLists)
		returningList = (List *) list_nth(plan->returningLists, subplan_index);
*/

	/*
	 * Construct the SQL command string.
	 */
/*
	switch (operation)
	{
		case CMD_INSERT:
			deparseInsertSql(&sql, root, resultRelation, rel,
							 targetAttrs, returningList,
							 &retrieved_attrs);
			break;
		case CMD_UPDATE:
			deparseUpdateSql(&sql, root, resultRelation, rel,
							 targetAttrs, returningList,
							 &retrieved_attrs);
			break;
		case CMD_DELETE:
			deparseDeleteSql(&sql, root, resultRelation, rel,
							 returningList,
							 &retrieved_attrs);
			break;
		default:
			elog(ERROR, "unexpected operation: %d", (int) operation);
			break;
	}
	heap_close(rel, NoLock);
*/

	/*
	 * Build the fdw_private list that will be available to the executor.
	 * Items in the list must match enum FdwModifyPrivateIndex, above.
	 */
	return list_make4(makeString(sql.data),
					  targetAttrs,
					  makeInteger((returningList != NIL)),
					  retrieved_attrs);
}

/*
 * ldap2_fdw_BeginForeignModify
 *		Begin an insert/update/delete operation on a foreign table
 */
static void
ldap2_fdw_BeginForeignModify(ModifyTableState *mtstate,
						   ResultRelInfo *resultRelInfo,
						   List *fdw_private,
						   int subplan_index,
						   int eflags)
{
	DEBUGPOINT;
		return;
}

/*
 * ldap2_fdw_ExecForeignInsert
 *		Insert one row into a foreign table
 */
static TupleTableSlot *
ldap2_fdw_ExecForeignInsert(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot)
{
	DEBUGPOINT;
	return NULL;
}

/*
 * ldap2_fdw_ExecForeignUpdate
 *		Update one row in a foreign table
 */
static TupleTableSlot *
ldap2_fdw_ExecForeignUpdate(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot)
{
	DEBUGPOINT;
	return NULL;
}

/*
 * ldap2_fdw_ExecForeignDelete
 *		Delete one row from a foreign table
 */
static TupleTableSlot *
ldap2_fdw_ExecForeignDelete(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot)
{
	DEBUGPOINT;
	return NULL;
}

/*
 * ldap2_fdw_EndForeignModify
 *		Finish an insert/update/delete operation on a foreign table
 */
static void
ldap2_fdw_EndForeignModify(EState *estate,
						 ResultRelInfo *resultRelInfo)
{
	DEBUGPOINT;
	return;
}

/*
 * ldap2_fdw_IsForeignRelUpdatable
 *  Assume table is updatable regardless of settings.
 *		Determine whether a foreign table supports INSERT, UPDATE and/or
 *		DELETE.
 */
static int
ldap2_fdw_IsForeignRelUpdatable(Relation rel)
{
	DEBUGPOINT;
	/* updatable is INSERT, UPDATE and DELETE.
	 */
	return (1 << CMD_INSERT) | (1 << CMD_UPDATE) | (1 << CMD_DELETE) ;
}

/*
 * ldap2_fdw_ExplainForeignScan
 *		Produce extra output for EXPLAIN of a ForeignScan on a foreign table
 */
static void
ldap2_fdw_ExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
/*
	List	   *fdw_private;
	char	   *sql;
	if (es->verbose)
	{
		DEBUGPOINT;
		fdw_private = ((ForeignScan *) node->ss.ps.plan)->fdw_private;
		sql = strVal(list_nth(fdw_private, FdwScanPrivateSelectSql));
		ExplainPropertyText("ldap2_fdw_ SQL", sql, es);
	}
*/

}

/*
 * ldap2_fdw_ExplainForeignModify
 *		Produce extra output for EXPLAIN of a ModifyTable on a foreign table
 */
static void
ldap2_fdw_ExplainForeignModify(ModifyTableState *mtstate,
							 ResultRelInfo *rinfo,
							 List *fdw_private,
							 int subplan_index,
							 ExplainState *es)
{
	DEBUGPOINT;
	if (es->verbose)
	{
		char	   *sql = strVal(list_nth(fdw_private,
										  FdwModifyPrivateUpdateSql));

		ExplainPropertyText("ldap2_fdw_ SQL", sql, es);
	}
}


/*
 * ldap2_fdw_AnalyzeForeignTable
 *		Test whether analyzing this foreign table is supported
 */
static bool
ldap2_fdw_AnalyzeForeignTable(Relation relation,
							AcquireSampleRowsFunc *func,
							BlockNumber *totalpages)
{
	DEBUGPOINT;
  *func = ldap2_fdw_AcquireSampleRowsFunc ;
	return false;
}

/*
 * Acquire a random sample of rows
 */
static int
ldap2_fdw_AcquireSampleRowsFunc(Relation relation, int elevel,
							  HeapTuple *rows, int targrows,
							  double *totalrows,
							  double *totaldeadrows)
{

	DEBUGPOINT;
  totalrows = 0;
  totaldeadrows = 0;
	return 0;
}
