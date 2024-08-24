/*
 * Include all neccessary headers
 * The frame is form fdw_dummy.c and will be reworked step by step
 */

#include "postgres.h"
// #include "postgres_fdw.h"
#include "access/sysattr.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "nodes/pg_list.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/value.h"
#include "optimizer/appendinfo.h"
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
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

#include "LdapFdwConn.h"
#include "LdapFdwOptions.h"
#include "LdapFdwPlanState.h"
#include "LdapFdwModifyState.h"
#include "helper_functions.h"
#include "ldap_functions.h"
#include "deparse.h"

void GetOptionStructr(LdapFdwOptions *, Oid);
void print_list(FILE *, List *);
void initLdap();
void bindLdap();

PG_MODULE_MAGIC;

#define MODULE_PREFIX ldap2_fdw

#define LDAP2_FDW_LOGFILE "/dev/shm/ldap2_fdw.log"

// LOG: log to postgres log
// INFO: write to stdout
#define DEBUGPOINT ereport(INFO, errmsg_internal("ereport Func %s, Line %d\n", __FUNCTION__, __LINE__))

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
#if PG_VERSION_NUM >= 140000
static void ldap2_fdw_AddForeignUpdateTargets(PlannerInfo *root,
										 Index rtindex,
										 RangeTblEntry *target_rte,
										 Relation target_relation);
#else
static void ldap2_fdw_AddForeignUpdateTargets(Query *parsetree,
										 RangeTblEntry *target_rte,
										 Relation target_relation);
#endif
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

char *dn, *matched_msg = NULL, *error_msg = NULL;
struct timeval timeout_struct = {.tv_sec = 10L, .tv_usec = 0L};
int version, parse_rc, finished = 0, msgtype, num_entries = 0, num_refs = 0;

void GetOptionStructr(LdapFdwOptions * options, Oid foreignTableId)
{
	ForeignTable *foreignTable = NULL;
	ForeignServer *foreignServer = NULL;
	UserMapping *mapping = NULL;
	ListCell *cell = NULL;
	List * all_options = NULL;
	
	if(options == NULL) {
		elog(ERROR, "options is null - variable is not initialized! Line: %d", __LINE__);
		return;
	}

	
	foreignTable = GetForeignTable(foreignTableId);
	foreignServer = GetForeignServer(foreignTable->serverid);
	mapping = GetUserMapping(GetUserId(), foreignTable->serverid);
	
	
	all_options = list_copy(foreignTable->options);
	all_options = list_concat(all_options, foreignServer->options);
	all_options = list_concat(all_options, mapping->options);
	
	//foreach(cell, foreignTable->options)
	foreach(cell, all_options)
	{
		DefElem *def = lfirst_node(DefElem, cell);
		
		//ereport(INFO, errmsg_internal("ereport Func %s, Line %d, def: %s\n", __FUNCTION__, __LINE__, def->defname));
		char * value = NULL;
		if(nodeTag(def->arg) == T_String)
		{
			//DEBUGPOINT;
			value = defGetString(def);
		}
		
		if (strcmp("uri", def->defname) == 0)
		{
			//options->uri = defGetString(def);
			if(value != NULL) options->uri = pstrdup(value);
		}
		else if(strcmp("hostname", def->defname) == 0)
		{
			//char * hostname = defGetString(def);
			//options->uri = psprintf("ldap://%s", hostname);
			//free(hostname);
			if(value != NULL) options->uri = psprintf("ldap://%s", value);
		}
		else if (strcmp("username", def->defname) == 0)
		{
			//options->username = defGetString(def);
			if(value != NULL) options->username = pstrdup(value);
		}
		else if (strcmp("password", def->defname) == 0)
		{
			//options->password = defGetString(def);
			if(value != NULL) options->password = pstrdup(value);
		}
		else if (strcmp("basedn", def->defname) == 0)
		{
			//options->basedn = defGetString(def);
			if(value != NULL) options->basedn = pstrdup(value);
		}
		else if (strcmp("filter", def->defname) == 0)
		{
			//options->filter = defGetString(def);
			//char * filter = defGetString(def);
			//if(filter != NULL) options->filter = pstrdup(filter);
			if(value != NULL) options->filter = pstrdup(value);
		}
		else if (strcmp("objectclass", def->defname) == 0)
		{
			//options->objectclass = defGetString(def);
			if(value != NULL) options->objectclass = pstrdup(value);
		}
		else if (strcmp("schemadn", def->defname) == 0)
		{
			//options->schemadn = defGetString(def);
			if(value != NULL) options->schemadn = pstrdup(value);
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
			ereport(INFO, errmsg_internal("%s ereport Line %d\n", __FUNCTION__, __LINE__));
			ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
				errmsg("invalid option \"%s\"", def->defname),
				errhint("Valid table options for ldap2_fdw are \"uri\", \"hostname\", \"username\", \"password\", \"basedn\", \"filter\", \"objectclass\", \"schemadn\", \"scope\""))
			);
		}
		pfree(value);
	}
	options->use_sasl = 0;
}

void print_list(FILE *out_channel, List *list)
{
	for(int i = 0; i < list->length; i++)
	{
		fprintf(out_channel, "%s\n", (char*)list->elements[i].ptr_value);
	}
}

static int estimate_size(LDAP *ldap, LdapFdwOptions *options)
{
	int rows = 0, rc = 0, msgid = 0;
	finished = 0;
	LDAPMessage   *res;
	if(options == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_ERROR),
			errmsg("options is null!"),
			errhint("options is null!"))
		);
	}
	
	if(options->basedn == NULL || !strcmp(options->basedn, ""))
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_ERROR),
			errmsg("Basedn is null or empty!"),
			errhint("Basedn is null or empty!"))
		);
	}
	
	ereport(INFO, errmsg_internal("%s ereport Line %d : basedn: %s\n", __FUNCTION__, __LINE__, options->basedn));
	if(options->filter != NULL)
	{
		ereport(INFO, errmsg_internal("%s ereport Line %d : filter: %s\n", __FUNCTION__, __LINE__, options->filter));
	}
	//rc = ldap_search_ext( ld, options->basedn, options->scope, options->filter, (char *[]){"objectClass"}, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
	rc = ldap_search_ext( ld, options->basedn, options->scope, NULL, (char *[]){NULL}, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
	if ( rc != LDAP_SUCCESS )
	{
		if ( error_msg != NULL && *error_msg != '\0' )
		{

			ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not exec ldap_search_ext on \"%s\" with filer \"%s\"", options->basedn, options->filter),
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
			default:
				finished = 1;
				break;
		}
		ldap_msgfree(res);
		res = NULL;
	}
	ereport(INFO, errmsg_internal("%s ereport Line %d : rows: %d\n", __FUNCTION__, __LINE__, rows));
	return rows;
}

static void estimate_costs(PlannerInfo *root, RelOptInfo *baserel,
			   LdapFdwPlanState *fdw_private,
			   Cost *startup_cost, Cost *total_cost)
{
	if(fdw_private == NULL) {
		*total_cost = 1;
		return;
	}
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

/**
 * @see https://www.postgresql.org/message-id/bv3rrk$2uv1$1@news.hub.org
 */
/*static char* getTupleDescDatum(Oid oid)
{
	TupleDesc tupleDesc = TypeGetTupleDesc(oid, NIL);
	TupleTableSlot* slot = TupleDescGetSlot(tupleDesc);
	return TupleGetDatum(slot, tuple);
}*/


/**
 * FROM mongo_query.c, function mongo_is_foreign_expr
 */
static bool ldap_fdw_is_foreign_expr(PlannerInfo *root, RelOptInfo *baserel, Expr *expression, bool is_having_cond)
{
	return true;
}


void initLdap()
{
	int version = LDAP_VERSION3, rc = 0;
	//int debug_level = LDAP_DEBUG_TRACE;
	
	if(option_params == NULL)
	{
		DEBUGPOINT;
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
				errmsg("option_params is null!"),
				errhint("ldap option params is not initialized!")));
		return;
	}
	
	//ereport(INFO, errmsg_internal("initLdap uri: %s, username: %s, password %s\n", option_params->uri, option_params->username, option_params->password));

	
	//option_params = (LdapFdwOptions *)malloc(sizeof(LdapFdwOptions *));
	
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

	/*if ( ( rc = ldap_set_option( ld, LDAP_OPT_DEBUG_LEVEL, &debug_level ) ) != LDAP_SUCCESS )
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not set ldap debug level option!"),
				errhint("Could not set ldap debug level option.")));
		return;
	}*/
	
	// removed ldap bind - call bind from another function
	bindLdap();
}

void bindLdap()
{
	int rc;
	//ereport(INFO, errmsg_internal("bindLdap username: %s, password %s\n", option_params->username, option_params->password));
	
	if ( ( rc = common_ldap_bind( ld, option_params->username, option_params->password, option_params->use_sasl) ) != LDAP_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not exec ldap bind to \"%s\" with username \"%s\"!", option_params->uri, option_params->username),
				errhint("Could not bind to ldap server. Is username and password correct?")));
		return;
	}
	
}

void _PG_init()
{
	option_params = (LdapFdwOptions *)palloc0(sizeof(LdapFdwOptions *));
	initLdapFdwOptions(option_params);
	DEBUGPOINT;
}

void _PG_fini()
{
	DEBUGPOINT;
	//free_ldap(&ld);
}

Datum ldap2_fdw_handler(PG_FUNCTION_ARGS)
{
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
	LdapFdwPlanState *fpinfo;
	fpinfo = (LdapFdwPlanState *) palloc0(sizeof(LdapFdwPlanState));
	baserel->fdw_private = (void *) fpinfo;
	
	
	
	GetOptionStructr(option_params, foreigntableid);
	initLdap();

	//baserel->rows = estimate_size(ld, option_params->basedn, option_params->filter, option_params->scope);
	baserel->rows = estimate_size(ld, option_params);
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
	
	Path	   *path;
	LdapFdwPlanState *fdw_private;
	
	fdw_private = (LdapFdwPlanState *) baserel->fdw_private;
	
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
	LdapFdwPlanState *fdw_private;
	
	fdw_private = (LdapFdwPlanState *) baserel->fdw_private;
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
	//Path	   *foreignPath;
	Index		scan_relid = baserel->relid;
	
	ListCell *cell = NULL;
	List *remote_exprs = NIL;
	List *local_exprs = NIL;
	LdapFdwPlanState *fdw_private;
	
	fdw_private = (LdapFdwPlanState *) baserel->fdw_private;
	
	
	//DEBUGPOINT;
	/* Fetch options */
	GetOptionStructr(option_params, foreigntableid);
	initLdap();
	//DEBUGPOINT;

	scan_relid = baserel->relid;
	//DEBUGPOINT;
	/*foreach(cell, scan_clauses) {
		DefElem *def = lfirst_node(DefElem, cell);
		ereport(INFO, errmsg_internal("%s ereport Line %d : name: %s\n", __FUNCTION__, __LINE__, def->defname));
		char * value = NULL;
		DEBUGPOINT;
		value = defGetString(def);
		ereport(INFO, errmsg_internal("%s ereport Line %d : name: %s, value: %s\n", __FUNCTION__, __LINE__, def->defname, value));
	}*/
	
	/*
	 * From MongoDB FDW
	 * 
	 * Separate the restrictionClauses into those that can be executed
	 * remotely and those that can't.  baserestrictinfo clauses that were
	 * previously determined to be safe or unsafe are shown in
	 * fdw_private->remote_conds and fdw_private->local_conds.  Anything else in the
	 * restrictionClauses list will be a join clause, which we have to check
	 * for remote-safety.  Only the OpExpr clauses are sent to the remote
	 * server.
	 */
	foreach(cell, scan_clauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(cell);

		Assert(IsA(rinfo, RestrictInfo));
		
		//ereport(INFO, errmsg_internal("%s ereport Line %d : List Cell ptr: %s\n", __FUNCTION__, __LINE__, (char*)cell->ptr_value));

		/* Ignore pseudoconstants, they are dealt with elsewhere */
		if (rinfo->pseudoconstant)
			continue;

		if (list_member_ptr(fdw_private->remote_conds, rinfo))
			remote_exprs = lappend(remote_exprs, rinfo->clause);
		else if (list_member_ptr(fdw_private->local_conds, rinfo))
			local_exprs = lappend(local_exprs, rinfo->clause);
		else if (IsA(rinfo->clause, OpExpr) && ldap_fdw_is_foreign_expr(root, baserel, rinfo->clause, false))
			remote_exprs = lappend(remote_exprs, rinfo->clause);
		else
			local_exprs = lappend(local_exprs, rinfo->clause);
	}
	
	
	//print_list(stderr, scan_clauses);
	//DEBUGPOINT;
	if(scan_clauses == NULL)
	{
		//DEBUGPOINT;
	}
	else
	{
		//DEBUGPOINT;
	}
	
	//ereport(INFO, errmsg_internal("%s ereport Line %d : List length: %d\n", __FUNCTION__, __LINE__, list_length(scan_clauses)));
	
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	//ereport(INFO, errmsg_internal("%s ereport Line %d : List length: %d\n", __FUNCTION__, __LINE__, list_length(scan_clauses)));
	
	//DEBUGPOINT;
	
	//char * values = NameListToString(scan_clauses);
	//char * values = ListToString(scan_clauses, ", ");
	
	//ereport(INFO, errmsg_internal("%s ereport Line %d : Raw Values: %s\n", __FUNCTION__, __LINE__, values));
	//DEBUGPOINT;

	
	/*
	foreach(cell, scan_clauses) {
		DefElem *def = lfirst_node(DefElem, cell);
		ereport(INFO, errmsg_internal("%s ereport Line %d : name: %s\n", __FUNCTION__, __LINE__, def->defname));
		char * value = NULL;
		DEBUGPOINT;
		if(def == NULL)
		{
			DEBUGPOINT;
		}
		else if (def->arg == NULL)
		{
			DEBUGPOINT;
		}
		else
		{
			DEBUGPOINT;
			switch(nodeTag(def->arg))
			{
				case T_String:
					DEBUGPOINT;
					value = defGetString(def);
					break;
				case T_Integer:
				case T_Float:
					DEBUGPOINT;
					break;
				case T_A_Star:
					DEBUGPOINT;
					break;
				case T_List:
					DEBUGPOINT;
					break;
				default:
					DEBUGPOINT;
					break;
				
			}
			DEBUGPOINT;
			ereport(INFO, errmsg_internal("%s ereport Line %d : name: %s, value: %s\n", __FUNCTION__, __LINE__, def->defname, value));
		}
	}*/
	
	//DEBUGPOINT;
	
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
	ForeignScan *fsplan = (ForeignScan *) node->ss.ps.plan;
	LdapFdwPlanState *fsstate = (LdapFdwPlanState *) node->fdw_state;
	Relation rel;
	TupleDesc tupdesc;
	int attnum;

	fsstate = (LdapFdwPlanState *) palloc0(sizeof(LdapFdwPlanState));
	//fsstate->ntuples = 3;
	fsstate->row = 0;

	fsstate->attinmeta = TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att);

	rel = node->ss.ss_currentRelation;
	
	tupdesc = RelationGetDescr(rel);
	
	fsstate->num_attrs = tupdesc->natts;
	
	fsstate->columns = (char**)palloc(sizeof(char*) * tupdesc->natts);
	
	// Beispiel für die Schleife über die Spalten
	for (attnum = 1; attnum <= tupdesc->natts; attnum++)
	{
		// Ignoriere eventuell ausgeblendete Spalten
		//if (!att_is_hidden(tupdesc, attnum))
		if(!tupdesc->attrs[attnum - 1].attisdropped)
		{
			// Hole den Spaltennamen
			char *colname = NameStr(tupdesc->attrs[attnum - 1].attname);
			fsstate->columns[attnum - 1] = pstrdup(colname);
		}
	}
	fsstate->columns[tupdesc->natts] = NULL;

	node->fdw_state = (void *) fsstate;
	
	elog(INFO, "basedn: %s",  option_params->basedn);
	elog(INFO, "scope: %d",  option_params->scope);
	elog(INFO, "filter: %s",  option_params->filter);
	
	//fsstate->query = strVal(list_nth(fsplan->fdw_private, FdwScanPrivateSelectSql));
	//fsstate->retrieved_attrs = list_nth(fsplan->fdw_private, FdwScanPrivateRetrievedAttrs);
	// Todo: Convert plan to ldap filter
	// from:     dynamodb_fdw/dynamodb_impl.cpp line 800
	// LDAP search
	//rc = ldap_search_ext( ld, option_params->basedn, option_params->scope, filter, attributes_array, 0, serverctrls, clientctrls, NULL, LDAP_NO_LIMIT, &msgid );
	fsstate->rc = ldap_search_ext( ld, option_params->basedn, option_params->scope, option_params->filter, fsstate->columns, 0, serverctrls, clientctrls, &timeout_struct, LDAP_NO_LIMIT, &(fsstate->msgid) );
	if ( fsstate->rc != LDAP_SUCCESS ) {

		elog(INFO, "ldap_search_ext_s: %s\n", ldap_err2string( fsstate->rc ) );
	}
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
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	
	AttInMetadata  *attinmeta;
	HeapTuple tuple;
	LdapFdwPlanState *fsstate = (LdapFdwPlanState *) node->fdw_state;
	int i;
	int vi;
	int natts;
	int err;
	char **s_values;
	//char ** a = NULL;
	BerElement *ber;
	char *entrydn = NULL;
	fsstate->ldap_message_result = NULL;
	struct berval **vals = NULL;
	bool first_in_array = true;
	char array_delimiter = '|';
	char *tmp_str = NULL;
	LDAPMessage *tmpmsg = NULL;
	
	s_values = (char **) palloc(sizeof(char *) * fsstate->num_attrs + 1);
	memset(s_values, 0, (fsstate->num_attrs + 1 ) * sizeof(char*));
	
	ExecClearTuple(slot);
	
	fsstate->rc = ldap_result( ld, fsstate->msgid, LDAP_MSG_ONE, &timeout_struct, &(fsstate->ldap_message_result) );
	switch(fsstate->rc)
	{
		case -1:
		case LDAP_RES_SEARCH_RESULT:
			//elog(INFO, "LDAP_RES_SEARCH_RESULT");
			return slot;
		case LDAP_RES_SEARCH_ENTRY:
			//elog(INFO, "LDAP_RES_SEARCH_ENTRY");
			entrydn = ldap_get_dn(ld, fsstate->ldap_message_result);
			i = 0;
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
			
			for(char **a = fsstate->columns; *a != NULL; *a++) {
				if(!strcasecmp(*a, "dn"))
				{
					s_values[i++] = pstrdup(entrydn);
					continue;
				}
				if((vals = ldap_get_values_len(ld, fsstate->ldap_message_result, *a)) != NULL)
				{
					if(ldap_count_values_len(vals) == 0)
					{
						s_values[i] = NULL;
					}
					else
					{
						tmp_str = strdup("");
						first_in_array = true;
						for ( vi = 0; vals[ vi ] != NULL; vi++ ) {
							if(first_in_array == true) first_in_array = false;
							else{
								tmp_str = realloc(tmp_str, strlen(tmp_str) + 1);
								tmp_str[strlen(tmp_str)] = array_delimiter;
							}
							tmp_str = realloc(tmp_str, strlen(tmp_str) + strlen(vals[ vi ]->bv_val) + 1);
							strcat(tmp_str, vals[ vi ]->bv_val);
						}
						s_values[i] = pstrdup(tmp_str);
						free(tmp_str);
					}
					ber_bvecfree(vals);
				}
				else
				{
				}
				i++;
			}
			if(entrydn != NULL) {
				ldap_memfree(entrydn);
				entrydn = NULL;
			}
			break;
		case LDAP_RES_SEARCH_REFERENCE:
			//elog(INFO, "LDAP_RES_SEARCH_REFERENCE");
			return slot;
	}
	
	
	fsstate->row++;
	
	tuple = BuildTupleFromCStrings(fsstate->attinmeta, s_values);
	ExecStoreHeapTuple(tuple, slot, false);
		
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
	// cleanup
	LdapFdwPlanState *fsstate = (LdapFdwPlanState *) node->fdw_state;

	/* if festate is NULL, we are in EXPLAIN; nothing to do */
	if (fsstate)
	{
		free_ldap_message(&(fsstate->ldap_message_result));
		pfree(fsstate);
		node->fdw_state = NULL;
	}
}


/*
 * From mongdo db fdw
 *    Add resjunk column(s) needed for update/delete on a foreign table
 */

#if PG_VERSION_NUM >= 140000
static void
ldap2_fdw_AddForeignUpdateTargets(PlannerInfo *root,
							 Index rtindex,
							 RangeTblEntry *target_rte,
							 Relation target_relation)
#else
static void
ldap2_fdw_AddForeignUpdateTargets(Query *parsetree,
							 RangeTblEntry *target_rte,
							 Relation target_relation)
#endif
{
	Var      *var;
	const char *attrname;
#if PG_VERSION_NUM < 140000
	TargetEntry *tle;
#endif

	/* assumes that this isn't attisdropped */
	Form_pg_attribute attr = TupleDescAttr(RelationGetDescr(target_relation), 0);

	/* Make a Var representing the desired value */
#if PG_VERSION_NUM >= 140000
	var = makeVar(rtindex,
#else
	var = makeVar(parsetree->resultRelation,
#endif
				  1,
				  attr->atttypid,
				  attr->atttypmod,
				  InvalidOid,
				  0);


	/* Get name of the row identifier column */
	//attrname = NameStr(attr->attname);
	attrname = pstrdup(NameStr(attr->attname));

#if PG_VERSION_NUM >= 140000
	/* Register it as a row-identity column needed by this target rel */
	add_row_identity_var(root, var, rtindex, attrname);
#else
	/* Wrap it in a TLE with the right name ... */
	tle = makeTargetEntry((Expr *) var,
						  list_length(parsetree->targetList) + 1,
						  pstrdup(attrname),
						  true);

	/* ... And add it to the query's targetlist */
	parsetree->targetList = lappend(parsetree->targetList, tle);
#endif
	DEBUGPOINT;
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
 * @source from https://github.com/heimir-sverrisson/jdbc2_fdw
 */
static List *
ldap2_fdw_PlanForeignModify(PlannerInfo *root,
						  ModifyTable *plan,
						  Index resultRelation,
						  int subplan_index)
{
	DEBUGPOINT;
	CmdType		operation = plan->operation;
	RangeTblEntry *rte = planner_rt_fetch(resultRelation, root);
	List	   *targetAttrs = NIL;
	List	   *returningList = NIL;
	List	   *retrieved_attrs = NIL;
	TupleDesc	tupdesc;
	Oid			array_element_type = InvalidOid;
	Oid			foreignTableId;
	StringInfoData sql;
	Relation	rel = NULL;
	DEBUGPOINT;
#if PG_VERSION_NUM < 130000
	rel = heap_open(rte->relid, NoLock);
#else
	rel = table_open(rte->relid, NoLock);
#endif
	
	
	initStringInfo(&sql);
	
	elog(INFO, "SQL String: %s", sql.data);

	/*
	 * Core code already has some lock on each rel being planned, so we can
	 * use NoLock here.
	 */
	foreignTableId = RelationGetRelid(rel);
	tupdesc = RelationGetDescr(rel);

	/*
	 * In an INSERT, we transmit all columns that are defined in the foreign
	 * table.  In an UPDATE, we transmit only columns that were explicitly
	 * targets of the UPDATE, so as to avoid unnecessary data transmission.
	 * (We can't do that for INSERT since we would miss sending default values
	 * for columns not listed in the source statement.)
	 */

	if (operation == CMD_INSERT)
	{
		int			attnum;
		for(attnum = 0; attnum < tupdesc->natts; attnum++)
		{
			Form_pg_attribute attr = TupleDescAttr(tupdesc, attnum);
			if (!attr->attisdropped)
				targetAttrs = lappend_int(targetAttrs, attnum);
		}
		
	}
	else if (operation == CMD_UPDATE)
	{
		/*Bitmapset  *tmpset = bms_copy(rte->modifiedCols);
		AttrNumber	col;
		while ((col = bms_first_member(tmpset)) >= 0)
		{
			col += FirstLowInvalidHeapAttributeNumber;
			if (col <= InvalidAttrNumber)		// shouldn't happen
				elog(ERROR, "system-column update is not supported");
			targetAttrs = lappend_int(targetAttrs, col);
		}*/
		
		Bitmapset  *tmpset;
#if PG_VERSION_NUM >= 160000
		RTEPermissionInfo *perminfo;
		int			attidx;
#endif
		AttrNumber	col;

#if PG_VERSION_NUM >= 160000
		perminfo = getRTEPermissionInfo(root->parse->rteperminfos, rte);
		tmpset = bms_copy(perminfo->updatedCols);
		attidx = -1;
#else
		tmpset = bms_copy(rte->updatedCols);
#endif

#if PG_VERSION_NUM >= 160000
		while ((attidx = bms_next_member(tmpset, attidx)) >= 0)
#else
		while ((col = bms_first_member(tmpset)) >= 0)
#endif
		{
#if PG_VERSION_NUM >= 160000
			col = attidx + FirstLowInvalidHeapAttributeNumber;
#else
			col += FirstLowInvalidHeapAttributeNumber;
#endif
			if (col <= InvalidAttrNumber)	/* Shouldn't happen */
				elog(ERROR, "system-column update is not supported");

			/*
			 * We also disallow updates to the first column which happens to
			 * be the row identifier in MongoDb (_id)
			 */
			if (col == 1)		/* Shouldn't happen */
				elog(ERROR, "row identifier column update is not supported");

			targetAttrs = lappend_int(targetAttrs, col);
		}
		/* We also want the rowid column to be available for the update */
		targetAttrs = lcons_int(1, targetAttrs);
		
	}

	/*
	 * Extract the relevant RETURNING list if any.
	 */
	if (plan->returningLists)
		returningList = (List *) list_nth(plan->returningLists, subplan_index);

	/*
	 * Construct the SQL command string.
	 */
	switch (operation)
	{
		case CMD_INSERT:
/*			deparseInsertSql(&sql, root, resultRelation, rel,
							 targetAttrs, returningList,
							 &retrieved_attrs);*/
			break;
		case CMD_UPDATE:
			/*deparseUpdateSql(&sql, root, resultRelation, rel,
							 targetAttrs, returningList,
							 &retrieved_attrs);*/
			break;
		case CMD_DELETE:
			DEBUGPOINT;
			deparseDeleteSql(&sql, root, resultRelation, rel,
							 returningList,
							 &retrieved_attrs);
			break;
#if PG_VERSION_NUM >= 150000
		case CMD_MERGE:
			elog(ERROR, "CMD_MERGE not supported");
			break;
#endif
		case CMD_UTILITY:
			elog(ERROR, "CMD_UTILITY not supported");
			break;
		case CMD_NOTHING:
			elog(ERROR, "CMD_NOTHING not supported");
			break;
		case CMD_UNKNOWN:
			elog(ERROR, "CMD_UNKNOWN not supported");
			break;
		default:
			elog(ERROR, "unexpected operation: %d", (int) operation);
			break;
	}
#if PG_VERSION_NUM < 130000
	heap_close(rel, NoLock);
#else
	table_close(rel, NoLock);
#endif
	
	DEBUGPOINT;

	/*
	 * Build the fdw_private list that will be available to the executor.
	 * Items in the list must match enum FdwModifyPrivateIndex, above.
	 */
	/*return list_make4(makeString(sql.data),
					  targetAttrs,
					  makeInteger((returningList != NIL)),
					  retrieved_attrs);*/
	return list_make1(targetAttrs);
}

/*
 * ldap2_fdw_BeginForeignModify
 *		Begin an insert/update/delete operation on a foreign table
 *		From Mongo fdw
 */
static void
ldap2_fdw_BeginForeignModify(ModifyTableState *mtstate,
						   ResultRelInfo *resultRelInfo,
						   List *fdw_private,
						   int subplan_index,
						   int eflags)
{
	DEBUGPOINT;
	LdapFdwModifyState *fmstate;
	Relation	rel = resultRelInfo->ri_RelationDesc;
	AttrNumber	n_params;
	Form_pg_attribute attr;
	Oid			typefnoid = InvalidOid;
	bool		isvarlena = false;
	List		*attrs_list;
	ListCell   *lc;
	Oid			foreignTableId;
	Oid			userid;
	ForeignServer *server;
	UserMapping *user;
	ForeignTable *table;
	int nrattrs = 0;
#if PG_VERSION_NUM >= 160000
	ForeignScan *fsplan = (ForeignScan *) mtstate->ps.plan;
#else
	EState	   *estate = mtstate->ps.state;
	RangeTblEntry *rte;
#endif

	DEBUGPOINT;
	/*
	 * Do nothing in EXPLAIN (no ANALYZE) case.  resultRelInfo->ri_FdwState
	 * stays NULL.
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

#if PG_VERSION_NUM >= 160000
	userid = fsplan->checkAsUser ? fsplan->checkAsUser : GetUserId();
#else
	rte = rt_fetch(resultRelInfo->ri_RangeTableIndex, estate->es_range_table);
	userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();
#endif

	DEBUGPOINT;
	foreignTableId = RelationGetRelid(rel);

	/* Get info about foreign table. */
	table = GetForeignTable(foreignTableId);
	server = GetForeignServer(table->serverid);
	user = GetUserMapping(userid, server->serverid);
	DEBUGPOINT;

	/* Begin constructing LdapFdwModifyState. */
	fmstate = (LdapFdwModifyState *) palloc0(sizeof(LdapFdwModifyState));

	fmstate->rel = rel;
	//GetOptionStructr((fmstate->options), foreignTableId);
	GetOptionStructr(option_params, foreignTableId);

	/*
	 * Get connection to the foreign server.  Connection manager will
	 * establish new connection if necessary.
	 */
	fmstate->ldap = ld;
	DEBUGPOINT;
	
	fmstate->target_attrs = (List *) list_nth(fdw_private, 0);
	fmstate->options = option_params;
	
	//fmstate->retrieved_attrs = (List *) list_nth(fdw_private, 3);

	//n_params = list_length(fmstate->target_attrs) + 1;
	//elog(INFO, "n_params: %d", n_params);
	fmstate->p_flinfo = (FmgrInfo *) palloc(sizeof(FmgrInfo) * n_params);
	fmstate->p_nums = 0;

	DEBUGPOINT;
	
	if (mtstate->operation == CMD_UPDATE)
	{
#if PG_VERSION_NUM >= 140000
		Plan	   *subplan = outerPlanState(mtstate)->plan;
#else
		Plan	   *subplan = mtstate->mt_plans[subplan_index]->plan;
#endif

		Assert(subplan != NULL);
		
		DEBUGPOINT;

		attr = TupleDescAttr(RelationGetDescr(rel), 0);
		elog(INFO, "Function: %s, Attribute Name: %s, Line: %d", __FUNCTION__, NameStr(attr->attname), __LINE__);
		
		fmstate->rowidAttno = ExecFindJunkAttributeInTlist(subplan->targetlist, NameStr(attr->attname));
		DEBUGPOINT;
	// 
// 		/* Find the rowid resjunk column in the subplan's result */
// 		fmstate->rowidAttno = ExecFindJunkAttributeInTlist(subplan->targetlist, NameStr(attr->attname));
// 		
//         getTypeOutputInfo(TIDOID, &typefnoid, &isvarlena);
//         fmgr_info(typefnoid, &fmstate->p_flinfo[fmstate->p_nums]);
//         fmstate->p_nums++;
// 		
// 		if (!AttributeNumberIsValid(fmstate->rowidAttno))
// 			elog(ERROR, "could not find junk row identifier column");
	}
	
	/* Set up for remaining transmittable parameters */
	foreach(lc, fmstate->target_attrs)
	{
		int			attnum = lfirst_int(lc);
		DEBUGPOINT;
		attr = TupleDescAttr(RelationGetDescr(rel), attnum);
		DEBUGPOINT;
		elog(INFO, "Function: %s, Attribute Name: %s", __FUNCTION__, NameStr(attr->attname));
		DEBUGPOINT;

		elog(INFO, "Function: %s, attr->atttypid: %d, attr->attisdropped: %d", __FUNCTION__, attr->atttypid, attr->attisdropped);
		Assert(!attr->attisdropped);
		DEBUGPOINT;
		if(attr->atttypid != 0 && !attr->attisdropped) {
			getTypeOutputInfo(attr->atttypid, &typefnoid, &isvarlena);
			DEBUGPOINT;
			fmgr_info(typefnoid, &fmstate->p_flinfo[fmstate->p_nums]);
			DEBUGPOINT;
			fmstate->p_nums++;
		}
	}
		DEBUGPOINT;
	Assert(fmstate->p_nums <= n_params);
		DEBUGPOINT;

	resultRelInfo->ri_FdwState = fmstate;
	DEBUGPOINT;
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
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Oid			foreignTableId;
	Oid			userid;
    const char **p_values;
	char	   *columnName = NULL;
	char *dn = NULL;
	UserMapping *user;
	LDAPMod		**insert_data = NULL;
	LDAPMod		* single_ldap_mod = NULL;
	TupleDesc	tupdesc;
    int         n_rows;
	int 		i;
	int 		j;
	int			p_index;
	int			rc;
	Form_pg_attribute attr;
	Relation rel = resultRelInfo->ri_RelationDesc;
	tupdesc = RelationGetDescr(rel);
    Datum       attr_value;
	ForeignServer *server;
	ForeignTable *table;
    bool        isnull;
#if PG_VERSION_NUM >= 160000
	ForeignScan *fsplan = (ForeignScan *) mtstate->ps.plan;
#else
	//EState	   *estate = mtstate->ps.state;
	RangeTblEntry *rte;
#endif

	foreignTableId = RelationGetRelid(rel);
	
#if PG_VERSION_NUM >= 160000
	userid = fsplan->checkAsUser ? fsplan->checkAsUser : GetUserId();
#else
	rte = rt_fetch(resultRelInfo->ri_RangeTableIndex, estate->es_range_table);
	userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();
#endif

	/* Get info about foreign table. */
	table = GetForeignTable(foreignTableId);
	server = GetForeignServer(table->serverid);
	user = GetUserMapping(userid, server->serverid);

	/* Begin constructing LdapFdwModifyState. */
	fmstate = (LdapFdwModifyState *) palloc0(sizeof(LdapFdwModifyState));

	fmstate->rel = rel;
	//GetOptionStructr((fmstate->options), foreignTableId);
	GetOptionStructr(option_params, foreignTableId);
	initLdap();
	
	//p_values = (const char **) palloc(sizeof(char *) * fmstate->p_nums);
	//insert_data = ( LDAPMod ** ) palloc(( fmstate->p_nums + 1 ) * sizeof( LDAPMod * ));

// 	insert_data = ( LDAPMod ** ) palloc(( tupdesc->natts + 2 ) * sizeof( LDAPMod * ));
// 	memset(insert_data, 0, sizeof(LDAPMod **)*(tupdesc->natts + 2));
// 	
// 	for ( i = 0; i < tupdesc->natts + 1; i++ ) {
// 
// 		if (( insert_data[ i ] = ( LDAPMod * ) palloc( sizeof( LDAPMod ))) == NULL ) {
// 			elog(ERROR, "Could not allocate Memory for accocating ldap mods!");
// 		}
// 		insert_data[i]->mod_op = LDAP_MOD_ADD;
// 	}
// 	
// 	elog(INFO, "fmstate->p_nums: %d, tupdesc->natts:%d", fmstate->p_nums, tupdesc->natts);
// 
// 	j = 0;
// 	
// 	// Attribute für objectClass erstellen
	char *objectclass_values[] = { "inetOrgPerson", NULL };
// 	
//     insert_data[j]->mod_type = "objectClass";
//     insert_data[j]->mod_values = objectclass_values;
	
	j++;
	
	single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_ADD, "objectClass", objectclass_values);
	append_ldap_mod(&insert_data, single_ldap_mod);
	free_ldap_mod(single_ldap_mod);
	single_ldap_mod = NULL;
	
    // Durchlaufe alle Attribute des Tuples
    for (i = 0; i < tupdesc->natts; i++) {
        if (slot->tts_isnull[i]) {
            // Attribut ist NULL
            elog(INFO, "Attribut %s ist NULL", NameStr(tupdesc->attrs[i].attname));
            continue;
        }

        // Hole den Wert des Attributs
        attr_value = slot_getattr(slot, i + 1, &isnull);

        if (!isnull) {
            // Eindeutigen Namen des Attributs erhalten
            char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
            // Wert des Attributs formatieren (z.B. für Logging)
            char *value_str = DatumGetCString(DirectFunctionCall1(textout, attr_value));
			if(!strcmp(att_name, "dn")) dn = pstrdup(value_str);
			else
			{
				elog(INFO, "i: %d, j: %d, Attribut: %s, Wert: %s", i, j, att_name, value_str);
				//p_values[i] = pstrdup(value_str);
				/*
				insert_data[i]->mod_type = pstrdup(att_name);
				insert_data[i]->mod_values = (char**)palloc( sizeof(char*)*2);
				memset(insert_data[i]->mod_values, 0, sizeof(char*)*2);
				insert_data[i]->mod_values[0] = pstrdup(value_str);
				insert_data[i]->mod_values[1] = NULL;
				*/
				
				/*
				insert_data[j]->mod_type = pstrdup(att_name);
				insert_data[j]->mod_values = (char**)palloc( sizeof(char*)*2);
				memset(insert_data[j]->mod_values, 0, sizeof(char*)*2);
				insert_data[j]->mod_values[0] = pstrdup(value_str);
				insert_data[j]->mod_values[1] = NULL;
				j++;
				*/
				
				single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_ADD, att_name, (char*[]){value_str, NULL});
				append_ldap_mod(&insert_data, single_ldap_mod);
				//ldap_mods_free(&single_ldap_mod, true);
				free_ldap_mod(single_ldap_mod);
				single_ldap_mod = NULL;

			}
            pfree(value_str);  // Freigeben des String-Puffers
			pfree(att_name);
        }
    }
	/*if (slot != NULL && fmstate->target_attrs != NIL)
	{
		ListCell   *lc;
		foreach(lc, fmstate->target_attrs)
		{
			int			attnum = lfirst_int(lc);
			bool isnull;
			attr = TupleDescAttr(RelationGetDescr(rel), attnum);
			Datum value = slot_getattr(slot, attnum + 1, &isnull);
			char * c_name = NameStr(attr->attname);
			//char * c_value = DatumGetCString(value);
			//char * c_value = OutputFunctionCall(&fmstate->p_flinfo[p_index], value);
			//char * c_value = OutputFunctionCall(textout, value);
			char *c_value = DatumGetCString(DirectFunctionCall1(textout, value));
			
			elog(INFO, "%s AttrNum: %u, Name: %s, Value: %s, Null: %d", __FUNCTION__, attnum, c_name, c_value, isnull);
			p_values[p_index] = c_value;
			p_index++;
		}
		//ldap_add_ext(ld, dn, NULL, NULL, NULL);
	}*/
	
	elog(INFO, "ldap add: dn: %s", dn);
	rc = ldap_add_ext_s( ld, dn, insert_data, NULL, NULL );

	if ( rc != LDAP_SUCCESS ) {
		elog( ERROR, "ldap_add_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );

	}
	
	for ( i = 0; i < fmstate->p_nums; i++ ) {

		free( insert_data[ i ] );

	}
	
	pfree(dn);
	free( insert_data );
	return slot;
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
	LdapFdwModifyState *fmstate = fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Datum       attr_value, datum;
	bool		isNull = false;
	Oid			foreignTableId;
	Oid			typoid;
	char *dn = NULL;
	char *columnName = NULL;
	int rc = 0;
	int i = 0, j = 0;
	LDAPMod		**modify_data = NULL;
	LDAPMod		* single_ldap_mod = NULL;
	ForeignTable *table;
	Form_pg_attribute attr;
	Relation rel = resultRelInfo->ri_RelationDesc;
	TupleDesc tupdesc = RelationGetDescr(rel);
	ListCell   *lc = NULL;
	foreignTableId = RelationGetRelid(resultRelInfo->ri_RelationDesc);
	datum = ExecGetJunkAttribute(planSlot, fmstate->rowidAttno, &isNull);
	typoid = get_atttype(foreignTableId, 1);
	// Durchlaufe alle Attribute des Tuples
    for (i = 0; i < tupdesc->natts; i++) {
        if (slot->tts_isnull[i]) {
            // Attribut ist NULL
            elog(INFO, "Attribut %s ist NULL", NameStr(tupdesc->attrs[i].attname));
			single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){value_str, NULL});
			append_ldap_mod(&modify_data, single_ldap_mod);
			free_ldap_mod(single_ldap_mod);
			single_ldap_mod = NULL;
            continue;
        }
        attr_value = slot_getattr(slot, i + 1, &isNull);

        if (!isNull) {
            // Eindeutigen Namen des Attributs erhalten
            char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
            // Wert des Attributs formatieren (z.B. für Logging)
            char *value_str = DatumGetCString(DirectFunctionCall1(textout, attr_value));
			if(!strcmp(att_name, "dn")) dn = pstrdup(value_str);
			else
			{
				elog(INFO, "i: %d, j: %d, Attribut: %s, Wert: %s", i, j, att_name, value_str);
				if(value_str == NULL || !strcmp(value_str, "")) {
					single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){value_str, NULL});
				} else {
					single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_REPLACE, att_name, (char*[]){value_str, NULL});
				}
				append_ldap_mod(&modify_data, single_ldap_mod);
				//ldap_mods_free(&single_ldap_mod, true);
				free_ldap_mod(single_ldap_mod);
				single_ldap_mod = NULL;
				
			}
            pfree(value_str);  // Freigeben des String-Puffers
			pfree(att_name);
		}
	}
	
	elog(INFO, "ldap mod: dn: %s", dn);
	rc = ldap_modify_ext_s( ld, dn, modify_data, NULL, NULL );
	rc = LDAP_SUCCESS;

	if ( rc != LDAP_SUCCESS ) {
		elog( ERROR, "ldap_modify_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );

	}
	
	for ( i = 0; i < fmstate->p_nums; i++ ) {

		free( modify_data[ i ] );

	}
	
	pfree(dn);
	free( modify_data );
	
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
	LdapFdwModifyState *fmstate = fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Datum       attr_value, datum;
	bool		isNull = false;
	Oid			foreignTableId;
	Oid			typoid;
	char *dn = NULL;
	char *columnName = NULL;
	int rc = 0;
	int i = 0;
	ForeignTable *table;
	Form_pg_attribute attr;
	Relation rel = resultRelInfo->ri_RelationDesc;
	TupleDesc tupdesc = RelationGetDescr(rel);
	foreignTableId = RelationGetRelid(resultRelInfo->ri_RelationDesc);
	
	/* Get the id that was passed up as a resjunk column */
	datum = ExecGetJunkAttribute(planSlot, 1, &isNull);
	char *value_str = DatumGetCString(DirectFunctionCall1(textout, datum));
	elog(INFO, "Value: %s", value_str);

	columnName = get_attname(foreignTableId, 1, false);
	elog(INFO, "Column Name: %s\n", columnName);
	elog(INFO, "Base DN to delete from: %s\n", fmstate->options->basedn);
	asprintf(&dn, "%s=%s,%s", columnName, value_str, fmstate->options->basedn);
	rc = ldap_delete_s(ld, dn);
	free(dn);
	
	if(rc != LDAP_SUCCESS) return NULL;
	
	/*
	typoid = get_atttype(foreignTableId, 1);
	table = GetForeignTable(foreignTableId);
	DEBUGPOINT;
	for (i = 0; i < tupdesc->natts; i++) {
		if (slot->tts_isnull[i]) {
			// Attribut ist NULL
			continue;
		}
		DEBUGPOINT;

		// Hole den Wert des Attributs
		attr_value = slot_getattr(slot, i + 1, &isNull);
		char *value_str = DatumGetCString(DirectFunctionCall1(textout, attr_value));
		elog(ERROR, "Attr Value: %s", value_str);
		DEBUGPOINT;

		if (!isNull) {
			DEBUGPOINT;
			// Eindeutigen Namen des Attributs erhalten
			char *att_name = NameStr(tupdesc->attrs[i].attname);
			DEBUGPOINT;
			// Wert des Attributs formatieren (z.B. für Logging)
			if(strcmp(att_name, "dn")) continue;
			DEBUGPOINT;
			char *dn = DatumGetCString(DirectFunctionCall1(textout, attr_value));
			DEBUGPOINT;
			elog(INFO, "Attribut: %s, Wert: %s", att_name, dn);
			rc = ldap_delete_s(ld, dn);
			DEBUGPOINT;
			pfree(dn);  // Freigeben des String-Puffers
		}
	}
	*/


	/* The type of first column of MongoDB's foreign table must be NAME */
	/*if (typoid != NAMEOID)
		elog(ERROR, "type of first column of MongoDB's foreign table must be \"NAME\"");*/
	
	
	
	
	return slot;
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
	DEBUGPOINT;
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



