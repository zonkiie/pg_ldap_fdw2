/*
 * Include all neccessary headers
 * The frame is form fdw_dummy.c and will be reworked step by step
 */

#include "postgres.h"
// #include "postgres_fdw.h"
#include "access/sysattr.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/tableam.h"
#include "catalog/namespace.h"
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
#include "nodes/pg_list.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/execnodes.h"
#include "nodes/nodes.h"
#include "nodes/value.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
//#include "optimizer/var.h"
#include "optimizer/optimizer.h"
#include "parser/parse_func.h"
#include "parser/parsetree.h"
#if PG_VERSION_NUM >= 160000
#include "parser/parse_relation.h"
#endif
#include "optimizer/restrictinfo.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

#include "LdapFdwOptions.h"
#include "LdapFdwTypes.h"
#include "helper_functions.h"
#include "ldap_functions.h"
#include "ldap_schema_helpers.h"
#include "deparse.h"

static void GetOptionStructr(LdapFdwOptions *, Oid);
static LdapFdwConn * create_LdapFdwConn(void);
static LdapFdwOptions * create_LdapFdwOptions(void);
static void initLdapConnectionStruct(LdapFdwConn *);
static void bindLdapStruct(LdapFdwConn *);
static TupleTableSlot * fetchLdapEntryByDnIntoSlot(LdapFdwConn *, Oid, char *);


PG_MODULE_MAGIC;

#define MODULE_PREFIX ldap2_fdw

#define LDAP2_FDW_LOGFILE "/dev/shm/ldap2_fdw.log"

/** @see https://jdbc.postgresql.org/documentation/publicapi/constant-values.html */
#define VARCHARARRAYOID 1015

// LOG: log to postgres log
// INFO: write to stdout
#define DEBUGPOINT elog(INFO, "ereport File %s, Func %s, Line %d\n", __FILE__, __FUNCTION__, __LINE__)

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
static void ldap2_fdw_add_foreign_final_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *final_rel, FinalPathExtraData *extra);

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
static void ldap2_fdw_GetForeignUpperPaths(PlannerInfo *root,
									  UpperRelationKind stage,
									  RelOptInfo *input_rel,
									  RelOptInfo *output_rel,
									  void *extra);
static List * ldap2_fdw_ImportForeignSchema(ImportForeignSchemaStmt *stmt,
							Oid serverOid);

//static bool ldap2_fdw_ScrollResultSet();


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

/**
 * @from mysql_fdw
 */
// enum FdwPathPrivateIndex
// {
// 	/* has-final-sort flag (as an integer Value node) */
// 	FdwPathPrivateHasFinalSort,
// 	/* has-limit flag (as an integer Value node) */
// 	FdwPathPrivateHasLimit
// };
enum FdwPathPrivateIndex
{
	/* has-limit flag (as an integer Value node) */
	FdwPathPrivateHasLimit,
	/* has-offset flag (as an integer Value node) */
	FdwPathPrivateHasOffset
};

struct timeval timeout_struct = {.tv_sec = 10L, .tv_usec = 0L};

static void GetOptionStructr(LdapFdwOptions * options, Oid foreignTableId)
{
	ForeignTable *foreignTable = NULL;
	ForeignServer *foreignServer = NULL;
	UserMapping *mapping = NULL;
	ListCell *cell = NULL;
	List * all_options = NIL;
	
	if(options == NULL) {
		elog(ERROR, "options is null - variable is not initialized! Line: %d", __LINE__);
		return;
	}

	
	foreignTable = GetForeignTable(foreignTableId);
	foreignServer = GetForeignServer(foreignTable->serverid);
	mapping = GetUserMapping(GetUserId(), foreignTable->serverid);
	
	all_options = list_concat(all_options, foreignTable->options);
	all_options = list_concat(all_options, foreignServer->options);
	all_options = list_concat(all_options, mapping->options);
	
	foreach(cell, all_options)
	{
		DefElem *def = lfirst_node(DefElem, cell);
		
		char * value = NULL;
		if(nodeTag(def->arg) == T_String)
		{
			value = defGetString(def);
		}
		
		if (strcmp("uri", def->defname) == 0)
		{
			if(value != NULL) options->uri = pstrdup(value);
		}
		else if(strcmp("hostname", def->defname) == 0)
		{
			if(value != NULL) options->uri = psprintf("ldap://%s", value);
		}
		else if (strcmp("username", def->defname) == 0)
		{
			if(value != NULL) options->username = pstrdup(value);
		}
		else if (strcmp("password", def->defname) == 0)
		{
			if(value != NULL) options->password = pstrdup(value);
		}
		else if (strcmp("basedn", def->defname) == 0)
		{
			if(value != NULL) options->basedn = pstrdup(value);
		}
		else if (strcmp("filter", def->defname) == 0)
		{
			if(value != NULL) options->filter = pstrdup(value);
		}
		else if (strcmp("schemadn", def->defname) == 0)
		{
			if(value != NULL) options->schemadn = pstrdup(value);
		}
		else if(strcmp("use_remotefiltering", def->defname) == 0)
		{
			if(value != NULL)
			{
				if(!strcasecmp(value, "true") || !strcasecmp(value, "t")) options->use_remotefiltering = true;
				else if(!strcasecmp(value, "false") || !strcasecmp(value, "f")) options->use_remotefiltering = false;
				else parse_int(value, &(options->use_remotefiltering), 0, NULL);
			}
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
			sscope = NULL;
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

		/* Why does this work on ARM but not on x64?
		pfree(value);
		value = NULL;*/
	}
	options->use_sasl = 0;
}

static void get_column_type(char ** type, int * is_array, Form_pg_attribute att)
{
	// Zunächst den Typ der Spalte abfragen
	Oid typid = att->atttypid;

	HeapTuple tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tup))
	{
		Form_pg_type typeform = (Form_pg_type) GETSTRUCT(tup);
		
		*type = pstrdup(NameStr(typeform->typname));
		*is_array = (*type[0]) == '_';
		// Column Name: NameStr(att->attname)
		ReleaseSysCache(tup);
	}	
}

static Oid get_type_oid(char * typename)
{
	//Datum relnamespace = PG_PUBLIC_NAMESPACE;
	/* @see https://github.com/pgspider/jdbc_fdw/blob/main/jdbc_fdw.c , method jdbc_convert_type_name */
	Oid type_oid = DatumGetObjectId(DirectFunctionCall1(regtypein, CStringGetDatum(typename)));
	return type_oid;
}

/**
 * @see https://github.com/EnterpriseDB/mongo_fdw/blob/master/mongo_query.c , function append_mongo_value
 */
static char ** extract_array_from_datum(Datum datum, Oid oid)
{
	char ** retval = NULL;
	switch(oid)
	{
		case VARCHARARRAYOID:
		case TEXTARRAYOID:
		{
			ArrayType  *array;
			Oid			elmtype;
			int16		elmlen;
			bool		elmbyval;
			char		elmalign;
			int			num_elems;
			Datum	   *elem_values;
			bool	   *elem_nulls;
			int			i;
			array = DatumGetArrayTypeP(datum);
			elmtype = ARR_ELEMTYPE(array);
			get_typlenbyvalalign(elmtype, &elmlen, &elmbyval, &elmalign);

			deconstruct_array(array, elmtype, elmlen, elmbyval, elmalign, &elem_values, &elem_nulls, &num_elems);
			
			retval = (char**)palloc(sizeof(char*) * (num_elems + 1));
			memset(retval, 0, sizeof(char*) * (num_elems + 1));
			
			for (i = 0; i < num_elems; i++)
			{
				char	   *valueString;
				Oid			outputFunctionId;
				bool		typeVarLength;
				getTypeOutputInfo(TEXTOID, &outputFunctionId, &typeVarLength);
				valueString = OidOutputFunctionCall(outputFunctionId, elem_values[i]);
				retval[i] = pstrdup(valueString);
			}
		}
			break;
		default:
		{
			retval = (char**)palloc(sizeof(char*) * (2));
			memset(retval, 0, sizeof(char*) * (2));
			retval[0] = DatumGetCString(DirectFunctionCall1(textout, datum));
		}
			break;
	}
	
	return retval;
}

static int estimate_size(LDAP *ldap, LdapFdwOptions *options)
{
	int rows = 0, rc = 0, msgid = 0;
	char *error_msg = NULL;
	LDAPMessage   *res;
	
	if(options == NULL)
	{
		elog(ERROR, "options is null!");
	}
	
	if(options->basedn == NULL || !strcmp(options->basedn, ""))
	{
		elog(ERROR, "Basedn is null or empty!");
	}
	
	/*
	if(options->filter != NULL)
	{
		elog(INFO, "%s Line %d : filter: %s\n", __FUNCTION__, __LINE__, options->filter);
	}*/
	
	//rc = ldap_search_ext( ld, options->basedn, options->scope, options->filter, (char *[]){"objectClass"}, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
	rc = ldap_search_ext( ldap, options->basedn, options->scope, NULL, (char *[]){NULL}, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
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
	
	while((rc = ldap_result( ldap, msgid, LDAP_MSG_ONE, &zerotime, &res )) == LDAP_RES_SEARCH_ENTRY)
	{
		rows++;
		ldap_msgfree(res);
		res = NULL;
	}
	
	return rows;
}

static void estimate_costs(PlannerInfo *root, RelOptInfo *baserel,
			   LdapFdwPlanState *fdw_private,
			   Cost *startup_cost, Cost *total_cost)
{
	BlockNumber pages = fdw_private->pages;
	double		ntuples = fdw_private->ntuples;
	Cost		run_cost = 0.0;
	Cost		cpu_per_tuple = 0.0;

	if(fdw_private == NULL) {
		*total_cost = 1;
		return;
	}
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


static LdapFdwConn * create_LdapFdwConn(void)
{
	LdapFdwConn* conn = (LdapFdwConn *)palloc0(sizeof(LdapFdwConn));
	//LDAPControl	*tmpc = (LDAPControl*)palloc( sizeof( LDAPControl ));
	
	conn->options = create_LdapFdwOptions();
	
	/*
	//tmpc->ldctl_oid = LDAP_CONTROL_SUBENTRIES;	
	tmpc->ldctl_oid = LDAP_CONTROL_X_WHATFAILED;
	tmpc->ldctl_iscritical = 1;
	ber = ber_alloc_t(LBER_USE_DER);
	ber_printf( ber, "b", 1);
	ber_flatten2( ber, &(tmpc->ldctl_value), 1 );
	ber_free( ber, 1 );
	
	conn->serverctrls = (LDAPControl**)malloc( sizeof( LDAPControl* ) * 2);
	conn->serverctrls[0] = tmpc;
	conn->serverctrls[1] = NULL;
	*/

	conn->serverctrls = NULL;
	conn->clientctrls = NULL;
	return conn;
}

static LdapFdwOptions * create_LdapFdwOptions(void)
{
	LdapFdwOptions * options = (LdapFdwOptions *)palloc0(sizeof(LdapFdwOptions));
	initLdapFdwOptions(options);
	return options;
}

static LdapFdwOptions * create_LdapFdwConnForFt(Oid foreignTableId)
{
	LdapFdwOptions * options = NULL;
	return options;
}

static void initLdapConnectionStruct(LdapFdwConn * ldap_fdw_connection)
{
	int version = LDAP_VERSION3, restart = 1, rc = 0;
	//int debug_level = LDAP_DEBUG_TRACE;
	
	if(ldap_fdw_connection->options == NULL)
	{
		DEBUGPOINT;
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
				errmsg("options is null!"),
				errhint("ldap option params is not initialized!")));
		return;
	}
	
	if(ldap_fdw_connection->options->uri == NULL || !strcmp(ldap_fdw_connection->options->uri, ""))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
				errmsg("URI is empty!"),
				errhint("LDAP URI is not given or has empty value!")));
		return;
	}

	if ( ( rc = ldap_initialize( &(ldap_fdw_connection->ldap), ldap_fdw_connection->options->uri ) ) != LDAP_SUCCESS )
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				errmsg("Could not establish connection to \"%s\"", ldap_fdw_connection->options->uri),
				errhint("Check that ldap server runs, accept connections and can be reached.")));
		return;
	}

	if ( ( rc = ldap_set_option( ldap_fdw_connection->ldap, LDAP_OPT_PROTOCOL_VERSION, &version ) ) != LDAP_SUCCESS )
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not set ldap version option!"),
				errhint("Could not set ldap version option. Does ldap server accept the correct ldap version 3?")));
		return;
	}
	
#warning why is LDAP_OPT_RESTART here neccessary?	
	if ( ( rc = ldap_set_option( ldap_fdw_connection->ldap, LDAP_OPT_RESTART, &restart) ) != LDAP_SUCCESS )
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not set ldap restart option!"),
				errhint("Could not set ldap restart option. Does ldap server accept the correct ldap restart option?")));
		return;
	}
	

	// removed ldap bind - call bind from another function
	bindLdapStruct(ldap_fdw_connection);
}

static void bindLdapStruct(LdapFdwConn * ldap_fdw_connection)
{
	int rc;
	
	if ( ( rc = common_ldap_bind( ldap_fdw_connection->ldap, ldap_fdw_connection->options->username, ldap_fdw_connection->options->password, ldap_fdw_connection->options->use_sasl) ) != LDAP_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_ERROR),
				errmsg("Could not exec ldap bind to \"%s\" with username \"%s\"!", ldap_fdw_connection->options->uri, ldap_fdw_connection->options->username),
				errhint("Could not bind to ldap server. Is username and password correct?")));
		return;
	}
	
}

/**
 * @see https://gist.github.com/syzdek/1459007/31c0561747bdbb88e7c3bbcb7d205e0f6d1972e7
 * @see https://postgrespro.com/list/thread-id/2545516
 */
static TupleTableSlot * fetchLdapEntryByDnIntoSlot(LdapFdwConn * ldapConn, Oid foreignTableId, char * dn)
{
#warning Remove unused vars or rewrite code to use it
	//ForeignTable *foreignTable = GetForeignTable(foreignTableId);
	//ForeignServer *foreignServer = GetForeignServer(foreignTable->serverid);
	//UserMapping *mapping = GetUserMapping(GetUserId(), foreignTable->serverid);
	HeapTuple	tuple;
	Relation rel = RelationIdGetRelation(foreignTableId);
	TupleTableSlot *slot = table_slot_create(rel, NULL);
	TupleDesc tupdesc = RelationGetDescr(rel);
	int msgId = 0;
	LDAPMessage    * entry = NULL;
	Oid varchar_oid = get_type_oid("character varying");
	struct berval **vals = NULL;
	int attnum, num_attrs = tupdesc->natts, rc, i;
	bool		typeByValue;
	char		typeAlignment;
	int16		typeLength;
	bool found = false;
	bool *null_values = (bool*)palloc(sizeof(bool) * (num_attrs + 1));
	Datum *d_values = (Datum*)palloc(sizeof(Datum) * (num_attrs + 1));
	char       **columns = (char**)palloc(sizeof(char*) * tupdesc->natts);
	char	   **column_types = (char**)palloc(sizeof(char*) * tupdesc->natts);
	Oid		    *column_type_ids = (Oid*)palloc(sizeof(Oid) * tupdesc->natts);
	char		*entrydn = NULL, *filter = NULL;
	memset(columns, 0, (tupdesc->natts) * sizeof(char*));
	memset(column_types, 0, (tupdesc->natts) * sizeof(char*));
	memset(column_type_ids, 0, (tupdesc->natts) * sizeof(Oid));
	memset(null_values, 0, (num_attrs + 1 ) * sizeof(bool));
	memset(d_values, 0, (num_attrs + 1 ) * sizeof(Datum*));
	
	get_typlenbyvalalign(varchar_oid, &typeLength, &typeByValue, &typeAlignment);
	
	//ExecClearTuple(slot);
	for (attnum = 1; attnum <= tupdesc->natts; attnum++)
	{
		// Ignoriere eventuell ausgeblendete Spalten
		//if (!att_is_hidden(tupdesc, attnum))
		if(!tupdesc->attrs[attnum - 1].attisdropped)
		{
			// Hole den Spaltennamen
			char *colname = NameStr(tupdesc->attrs[attnum - 1].attname);
			char * type = NULL;
			int is_array = -1;
			
			columns[attnum - 1] = pstrdup(colname);
			
			//Oid typid = tupdesc->attrs[attnum - 1].atttypid;
			get_column_type(&type, &is_array, &(tupdesc->attrs[attnum - 1]));
			column_types[attnum - 1] = pstrdup(type);
			column_type_ids[attnum - 1] = tupdesc->attrs[attnum - 1].atttypid;
		}
	}
	columns[tupdesc->natts] = NULL;
	
	// Own Scope to supress warnings, will be later changed
	{
		//filter = psprintf("(&(objectClass:=*)(dn:=%s)", dn);
		char* filter2 = ldap_dn2filter(dn);
		filter = pstrdup(filter2);
		free(filter2);
	}
	
	rc = ldap_search_ext( ldapConn->ldap, ldapConn->options->basedn, ldapConn->options->scope, filter /*ldapConn->options->filter*/ , columns, 0, ldapConn->serverctrls, ldapConn->clientctrls, &timeout_struct, LDAP_NO_LIMIT, &msgId );
	
	//if(rc != LDAP_SUCCESS) return slot;
	if(rc != LDAP_SUCCESS)  goto fetchLdapEntryByDnIntoSlotFinish;
	
	while((rc = ldap_result( ldapConn->ldap, msgId, LDAP_MSG_ONE, &timeout_struct, &entry )) == LDAP_RES_SEARCH_ENTRY)
	{
		i = 0;
		entrydn = ldap_get_dn(ldapConn->ldap, entry);
		if(strcmp(entrydn, dn)) // dn does not equal entrydn, so continue
		{
			ldap_memfree(entrydn);
			continue;
		}
		for(char **a = columns; *a != NULL; a++)
		{
			bool column_type_is_array = (column_types[i])[0] == '_';
			if(!strcasecmp(*a, "dn"))
			{
				null_values[i] = 0;
				d_values[i] = DirectFunctionCall1(textin, CStringGetDatum(entrydn));
				i++;
				continue;
			}
			if((vals = ldap_get_values_len(ldapConn->ldap, entry, *a)) != NULL)
			{
				int values_length = ldap_count_values_len(vals);
				//if(ldap_count_values_len(vals) == 0)
				if(values_length == 0)
				{
					null_values[i] = true;
					d_values[i] = PointerGetDatum(NULL);
				}
				else
				{
					if(!column_type_is_array) {
						null_values[i] = (vals[0]->bv_val == NULL || strlen(vals[0]->bv_val) == 0);
						d_values[i] = DirectFunctionCall1(textin, CStringGetDatum(vals[0]->bv_val));
					} else {
						ArrayType* array_values;
						Datum* item_values = (Datum*)palloc(sizeof(Datum) * (values_length + 2));
						memset(item_values, 0, (values_length + 1 ) * sizeof(Datum));

						for (int vi = 0; vals[ vi ] != NULL; vi++ ) {
							null_values[i] = 0;
							item_values[vi] = DirectFunctionCall1(textin, CStringGetDatum(vals[ vi ]->bv_val));
						}
						array_values = construct_array(item_values, values_length, varchar_oid, typeLength, typeByValue, typeAlignment);
						d_values[i] = PointerGetDatum(array_values);
					}
					
				}
				ber_bvecfree(vals);
			}
			else
			{
				null_values[i] = true;
				d_values[i] = PointerGetDatum(NULL);
			}
			i++;
		}
		
		found = true;
		
		ldap_memfree(entrydn);
		entrydn = NULL;
		
		ldap_msgfree(entry);
		entry = NULL;
	}

	if(found)
	{
		tuple = heap_form_tuple(tupdesc, d_values, null_values);
		ExecStoreHeapTuple(tuple, slot, false);
	}
	
fetchLdapEntryByDnIntoSlotFinish:
	pfree(filter);
	filter = NULL;
#if PG_VERSION_NUM < 130000
	heap_close(rel, NoLock);
#else
	table_close(rel, NoLock);
#endif
	ReleaseTupleDesc(tupdesc);
	return slot;
}

void _PG_init()
{
}

void _PG_fini()
{
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
	fdw_routine->ImportForeignSchema = ldap2_fdw_ImportForeignSchema;
	
	/* Support functions for upper relation push-down */
	fdw_routine->GetForeignUpperPaths = ldap2_fdw_GetForeignUpperPaths;

    PG_RETURN_POINTER(fdw_routine);
}

/*
 * GetForeignRelSize
 *		set relation size estimates for a foreign table
 */
static void
ldap2_fdw_GetForeignRelSize(PlannerInfo *root,
						   RelOptInfo *baserel,
						   Oid foreignTableId)
{
#warning remove unused vars or rewrite code
	//ListCell   *lc;
	//Bitmapset  *attrs_used = NULL;
	LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) palloc(sizeof(LdapFdwPlanState));
	memset(fdw_private, 0, sizeof(LdapFdwPlanState));
	
	//initLdapFdwOptions(option_params);
	fdw_private->ldapConn = create_LdapFdwConn();
	GetOptionStructr(fdw_private->ldapConn->options, foreignTableId);
	initLdapConnectionStruct(fdw_private->ldapConn);
	
	
	/*
	pull_varattnos((Node *) baserel->reltarget->exprs, baserel->relid,
				   &attrs_used);

	foreach(lc, baserel->baserestrictinfo)
	{
		RestrictInfo *ri = (RestrictInfo *) lfirst(lc);

		if (ldap_fdw_is_foreign_expr(root, baserel, ri->clause, false))
			fdw_private->remote_conds = lappend(fdw_private->remote_conds, ri);
		else
			fdw_private->local_conds = lappend(fdw_private->local_conds, ri);
	}

	pull_varattnos((Node *) baserel->reltarget->exprs, baserel->relid,
				   &fdw_private->attrs_used);

	foreach(lc, fdw_private->local_conds)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

		pull_varattnos((Node *) rinfo->clause, baserel->relid,
					   &fdw_private->attrs_used);
	}
	*/
	
	
	baserel->rows = estimate_size(fdw_private->ldapConn->ldap, fdw_private->ldapConn->options);
	fdw_private->row = 0;
	fdw_private->pushdown_safe = true;
	//elog(INFO, "Rows: %f", baserel->rows);
	baserel->fdw_private = (void *) fdw_private;
	if(baserel->fdw_private == NULL) elog(ERROR, "fdw_private is NULL! Line: %d", __LINE__);
	ldap_unbind_ext_s(fdw_private->ldapConn->ldap, NULL, NULL);
	free_options(fdw_private->ldapConn->options);
}

/*
 * GetForeignPaths
 *		create access path for a scan on the foreign table
 */
static void
ldap2_fdw_GetForeignPaths(PlannerInfo *root,
						 RelOptInfo *baserel,
						 Oid foreignTableId)
{
	
	Path	   *path;
	LdapFdwPlanState *fdw_private;
	//Cost		startupCost = 0.0;
	Cost		startup_cost = 10.0;
	Cost		total_cost = 0.0;
	
	fdw_private = (LdapFdwPlanState *) baserel->fdw_private;
	
	if(fdw_private == NULL) elog(ERROR, "fdw_private is NULL! Line: %d", __LINE__);
	
	/* Fetch options */
	//GetOptionStructr(fdw_private->ldapConn->options, foreignTableId);
	//initLdapWithOptions(fdw_private->ldapConn);
	//initLdapConnectionStruct(fdw_private->ldapConn);
	
#if (PG_VERSION_NUM >= 170000)
	path = (Path *) create_foreignscan_path(root, baserel,
												   NULL,	/* default pathtarget */
												   baserel->rows,
												   startup_cost,
												   total_cost,
												   NIL, /* no pathkeys */
												   baserel->lateral_relids,
												   NULL,	/* no extra plan */
												   NIL, /* no fdw_restrictinfo list */
												   NIL);	/* no fdw_private data */
#elif(PG_VERSION_NUM >= 90500 && PG_VERSION_NUM < 170000)
	path = (Path *) create_foreignscan_path(root, baserel,
#if (PG_VERSION_NUM >= 90600)
												   NULL,	/* default pathtarget */
#endif
												   baserel->rows,
												   startup_cost,
												   total_cost,
												   NIL, /* no pathkeys */
												   baserel->lateral_relids,
												   NULL,	/* no extra plan */
												   NIL);	/* no fdw_private list */
#elif (PG_VERSION_NUM < 90500)
	path = (Path *) create_foreignscan_path(root, baserel,
						baserel->rows,
						startup_cost,
						totalCost,
						NIL,
						NULL,
						NULL);

#endif

	/* Estimate costs */
	estimate_costs(root, baserel, fdw_private, &startup_cost, &total_cost);
	/* Create a ForeignPath node and add it as only possible path */
	
	
		/* Add foreign path as the only possible path */
	add_path(baserel, path);
}

/*
 * GetForeignPlan
 *	create a ForeignScan plan node
 */
#if (PG_VERSION_NUM <= 90500)
static ForeignScan *
ldap2_fdw_GetForeignPlan(PlannerInfo *root,
						RelOptInfo *baserel,
						Oid foreignTableId,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses)
{
	LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) baserel->fdw_private;
	/* Fetch options */
	fdw_private->ldapConn = create_LdapFdwConn();
	GetOptionStructr(fdw_private->ldapConn->options, foreignTableId);
	//initLdapWithOptions(fdw_private->ldapConn);
	initLdapConnectionStruct(fpinfo->ldapConn);

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
						Oid foreignTableId,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses,
						Plan *outer_plan)
{
	//Path	   *foreignPath;
	Index		scan_relid = baserel->relid;
	LdapFdwPlanState *fdw_private = NULL;
	
	ListCell	*cell = NULL;
	List 		*fdw_private_list = NIL;
#warning remove unused vars or rewrite code
	/*
	List 		*remote_exprs = NIL;
	List		*local_exprs = NIL;
	List		*scan_var_list;
	List		*fdw_scan_tlist = NIL;
	List		*whole_row_lists = NIL;
	*/
	char		*dn_clauses = NULL;
	StringInfoData sql_buf;
	bool		has_final_sort = false;
	bool		has_limit = false;
	bool		has_offset = false;
	
    initStringInfo(&sql_buf);
	fdw_private = (LdapFdwPlanState *) baserel->fdw_private;
	
	if (best_path->fdw_private)
	{
		//has_final_sort = intVal(list_nth(best_path->fdw_private, FdwPathPrivateHasFinalSort));
		has_limit = intVal(list_nth(best_path->fdw_private, FdwPathPrivateHasLimit));
		has_offset = intVal(list_nth(best_path->fdw_private, FdwPathPrivateHasOffset));
	}
	
	
	elog(INFO, "has_final_sort: %d, has_limit: %d, has_offset: %d", has_final_sort, has_limit, has_offset);
	
	//LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) palloc(sizeof(LdapFdwPlanState));
	
	//if(fdw_private == NULL) elog(ERROR, "fdw_private is NULL! Line: %d", __LINE__);
	
	/* Fetch options */
	//fdw_private->ldapConn = create_LdapFdwConn();
	//GetOptionStructr(fdw_private->ldapConn->options, foreignTableId);
	////initLdapWithOptions(fdw_private->ldapConn);
	//initLdapConnectionStruct(fdw_private->ldapConn);
	
	if(fdw_private->ldapConn->options->use_remotefiltering)
	{
	//baserel->fdw_private = (void*)fdw_private;
	//ldap2_fdw_log_nodeTags();
	// call ldap2_fdw_extract_dn(List *scan_clauses) here
	
	// Working on ldap filtering
	
	//dn_clauses = ldap2_fdw_extract_dn(root, foreignTableId, scan_clauses);
		dn_clauses = ldap2_fdw_extract_dn_value(root, foreignTableId, scan_clauses);
		fdw_private_list = list_make1(makeString(dn_clauses));
	}
	
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
		
		//ereport(INFO, errmsg_internal("%s ereport Line %d : List Cell ptr: %s\n", __FUNCTION__, __LINE__, (char*)cell->ptr_value));

		// Ignore pseudoconstants, they are dealt with elsewhere
		if (rinfo->pseudoconstant)
			continue;
/*
		if (list_member_ptr(fdw_private->remote_conds, rinfo))
			remote_exprs = lappend(remote_exprs, rinfo->clause);
		else if (list_member_ptr(fdw_private->local_conds, rinfo))
			local_exprs = lappend(local_exprs, rinfo->clause);
		else if (IsA(rinfo->clause, OpExpr) && ldap_fdw_is_foreign_expr(root, baserel, rinfo->clause, false))
			remote_exprs = lappend(remote_exprs, rinfo->clause);
		else
			local_exprs = lappend(local_exprs, rinfo->clause);
*/
	}
	
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

	/*
	foreach(cell, scan_clauses) {
		Node *clause = (Node *) lfirst(cell);
		
		//if (IsA(clause, Const))
		//{
		if (nodeTag(clause) == T_Const)
		{
			Const *constNode = (Const *) clause;
			
			Datum *value = constNode->constvalue;
			
			char *clause_sql = DatumGetCString(DirectFunctionCall1(textout, value));
			
			// Convert the expression to string
			//char *clause_sql = nodeToString(clause);
			
			appendStringInfoString(&sql_buf, clause_sql);
			
			pfree(clause_sql);
			
			// Optionally add a separator (like a space or comma) if desired
			if (lnext(scan_clauses, cell) != NULL)
			{
				appendStringInfoString(&sql_buf, " \nAND\n "); // or whatever separator is appropriate
			}
		}
		
	}
	
	elog(INFO, "sql: %s", sql_buf.data);
	*/



	//ereport(INFO, errmsg_internal("%s ereport Line %d : List length: %d\n", __FUNCTION__, __LINE__, list_length(scan_clauses)));
	
	//char * values = NameListToString(scan_clauses);
	//char * values = ListToString(scan_clauses, ", ");
	
	//ereport(INFO, errmsg_internal("%s ereport Line %d : Raw Values: %s\n", __FUNCTION__, __LINE__, values));
	
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
			ereport(INFO, errmsg_internal("%s ereport Line %d : name: %s, value: %s\n", __FUNCTION__, __LINE__, def->defname, value));
		}
	}*/
	
	//ldap_unbind_ext_s(fdw_private->ldapConn->ldap, NULL, NULL);
	//free_options(fdw_private->ldapConn->options);
	
	return make_foreignscan(tlist,
			scan_clauses,
			scan_relid, // baserel->relid,
			scan_clauses,
			fdw_private_list, //NIL, // best_path->fdw_private,
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
	//Relation *frel = node->ss.ss_currentRelation;
	// Oid foreignTableId = fsplan->scanRel->relid;
	// Oid foreignTableId = fsplan->scan.scanrelid;
	Oid foreignTableId = node->ss.ss_currentRelation->rd_id;
	//LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) node->fdw_state;
	LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) palloc0(sizeof(LdapFdwPlanState));
	Relation rel;
	TupleDesc tupdesc;
#warning remove unused vars or rewrite code
	/*ForeignTable *table;
	ForeignServer *server;
	UserMapping *user;*/
	char * dn_clauses = NULL;
	char * filter = NULL;
	
	int attnum;
	
	List	   *fdw_private_list = fsplan->fdw_private;
	if (list_length(fdw_private_list) > 0) dn_clauses = strVal(list_nth(fsplan->fdw_private, 0));

	
	if(fdw_private == NULL) elog(ERROR, "fdw_private is NULL! Line: %d", __LINE__);
	
	//fdw_private = (LdapFdwPlanState *) palloc0(sizeof(LdapFdwPlanState));
	fdw_private->ldapConn = create_LdapFdwConn();
	GetOptionStructr(fdw_private->ldapConn->options, foreignTableId);
	////initLdapWithOptions(fdw_private->ldapConn);
	initLdapConnectionStruct(fdw_private->ldapConn);
	//fdw_private->ntuples = 3;
	fdw_private->row = 0;

	fdw_private->attinmeta = TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att);

	rel = node->ss.ss_currentRelation;
	
	tupdesc = RelationGetDescr(rel);
	
	fdw_private->num_attrs = tupdesc->natts;
	
	fdw_private->columns = (char**)palloc(sizeof(char*) * tupdesc->natts);
	memset(fdw_private->columns, 0, (tupdesc->natts) * sizeof(char*));
	fdw_private->column_types = (char**)palloc(sizeof(char*) * tupdesc->natts);
	memset(fdw_private->column_types, 0, (tupdesc->natts) * sizeof(char*));
	fdw_private->column_type_ids = (Oid*)palloc(sizeof(Oid) * tupdesc->natts);
	memset(fdw_private->column_type_ids, 0, (tupdesc->natts) * sizeof(Oid));
	
	// Beispiel für die Schleife über die Spalten
	for (attnum = 1; attnum <= tupdesc->natts; attnum++)
	{
		// Ignoriere eventuell ausgeblendete Spalten
		//if (!att_is_hidden(tupdesc, attnum))
		if(!tupdesc->attrs[attnum - 1].attisdropped)
		{
			// Hole den Spaltennamen
			char *colname = NameStr(tupdesc->attrs[attnum - 1].attname);
			char * type;
			int is_array;
			
			fdw_private->columns[attnum - 1] = pstrdup(colname);
			
			//Oid typid = tupdesc->attrs[attnum - 1].atttypid;
			get_column_type(&type, &is_array, &(tupdesc->attrs[attnum - 1]));
			fdw_private->column_types[attnum - 1] = pstrdup(type);
			fdw_private->column_type_ids[attnum - 1] = tupdesc->attrs[attnum - 1].atttypid;
		}
	}
	fdw_private->columns[tupdesc->natts] = NULL;

	node->fdw_state = (void *) fdw_private;
	
	//fdw_private->query = strVal(list_nth(fsplan->fdw_private, FdwScanPrivateSelectSql));
	//fdw_private->retrieved_attrs = list_nth(fsplan->fdw_private, FdwScanPrivateRetrievedAttrs);
	// Todo: Convert plan to ldap filter
	// from:     dynamodb_fdw/dynamodb_impl.cpp line 800
	// LDAP search
	//rc = ldap_search_ext( ld, option_params->basedn, option_params->scope, filter, attributes_array, 0, serverctrls, clientctrls, NULL, LDAP_NO_LIMIT, &msgid );
	
	if(fdw_private->ldapConn->options->use_remotefiltering && dn_clauses != NULL)
	{
	
		//elog(INFO, "Use remote Filtering. Remote DN Clauses: %s", dn_clauses);
	
	/* defined filter has always priority */
	//if(dn_clauses != NULL && fdw_private->ldapConn->options->filter == NULL) filter = dn_clauses;
	//else if(fdw_private->ldapConn->options->filter != NULL) filter = fdw_private->ldapConn->options->filter;
	//else filter = fdw_private->ldapConn->options->filter;
		//filter = dn_clauses;
		//elog(INFO, "dn clauses: %s", dn_clauses);
		char* filter2 = ldap_dn2filter(dn_clauses);
		if(filter2 != NULL) {
			filter = pstrdup(filter2);
			free(filter2);
		}
		else filter = fdw_private->ldapConn->options->filter;
	}
	else filter = fdw_private->ldapConn->options->filter;
	elog(INFO, "Applying filters: %s", filter);
	
	//fdw_private->rc = ldap_search_ext( fdw_private->ldapConn->ldap, fdw_private->ldapConn->options->basedn, fdw_private->ldapConn->options->scope, fdw_private->ldapConn->options->filter, fdw_private->columns, 0, fdw_private->ldapConn->serverctrls, fdw_private->ldapConn->clientctrls, &timeout_struct, LDAP_NO_LIMIT, &(fdw_private->msgid) );
	fdw_private->rc = ldap_search_ext( fdw_private->ldapConn->ldap, fdw_private->ldapConn->options->basedn, fdw_private->ldapConn->options->scope, filter, fdw_private->columns, 0, fdw_private->ldapConn->serverctrls, fdw_private->ldapConn->clientctrls, &timeout_struct, LDAP_NO_LIMIT, &(fdw_private->msgid) );
	if ( fdw_private->rc != LDAP_SUCCESS ) {

		elog(INFO, "ldap_search_ext_s: %s\n", ldap_err2string( fdw_private->rc ) );
	}
}

/*
 * IterateForeignScan
 *		Retrieve next row from the result set, or clear tuple slot to indicate
 *		EOF.
 *   Fetch one row from the foreign source, returning it in a tuple table slot
 *    (the node's ScanTupleSlot should be used for this purpose).
 *  Return NULL if no more rows are available.
 * @see https://stackoverflow.com/questions/51127189/how-to-return-array-into-array-with-custom-type-in-postgres-c-function
 * @see https://stackoverflow.com/questions/61604650/create-integer-array-in-postgresql-c-function
 * @see https://github.com/postgres/postgres/blob/master/src/backend/utils/adt/arrayfuncs.c
 * @see https://stackoverflow.com/questions/39870443/whats-the-correct-oid-for-an-array-of-composite-type-in-postgresql
 * @see https://github.com/EnterpriseDB/mongo_fdw/blob/master/mongo_fdw.c, method column_value_array
 */
static TupleTableSlot *
ldap2_fdw_IterateForeignScan(ForeignScanState *node)
{
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	
	//AttInMetadata  *attinmeta;
	HeapTuple tuple;
	Relation rel = node->ss.ss_currentRelation;
	TupleDesc tupdesc;
	LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) node->fdw_state;
	int i;
	int vi;
	//int natts;
	int err;
	bool		typeByValue;
	char		typeAlignment;
	int16		typeLength;
	
	bool column_type_is_array;
	Datum *d_values;
	bool *null_values;
	//BerElement *ber;
	Oid varchar_oid = get_type_oid("character varying");
	char *entrydn = NULL;
	struct berval **vals = NULL;
	//Datum relnamespace = rel->rd_rel->relnamespace;
	fdw_private->ldap_message_result = NULL;
	//bool first_in_array = true;
#define array_delimiter ','
	//LDAPMessage *tmpmsg = NULL;
	
	get_typlenbyvalalign(varchar_oid, &typeLength, &typeByValue, &typeAlignment);
	
	tupdesc = RelationGetDescr(rel);
	
	null_values = (bool*)palloc(sizeof(bool) * (fdw_private->num_attrs + 1));
	memset(null_values, 0, (fdw_private->num_attrs + 1 ) * sizeof(bool));
	
	d_values = (Datum*)palloc(sizeof(Datum) * (fdw_private->num_attrs + 1));
	memset(d_values, 0, (fdw_private->num_attrs + 1 ) * sizeof(Datum*));
	
	ExecClearTuple(slot);
	
	fdw_private->rc = ldap_result( fdw_private->ldapConn->ldap, fdw_private->msgid, LDAP_MSG_ONE, &timeout_struct, &(fdw_private->ldap_message_result) );
	switch(fdw_private->rc)
	{
		case -1:
		case LDAP_RES_SEARCH_RESULT:
			//elog(INFO, "LDAP_RES_SEARCH_RESULT");
		case LDAP_RES_SEARCH_REFERENCE:
			//elog(INFO, "LDAP_RES_SEARCH_REFERENCE");
			return slot;
		case LDAP_RES_SEARCH_ENTRY:
			//elog(INFO, "LDAP_RES_SEARCH_ENTRY");
			entrydn = ldap_get_dn(fdw_private->ldapConn->ldap, fdw_private->ldap_message_result);
			i = 0;
			ldap_get_option(fdw_private->ldapConn->ldap, LDAP_OPT_ERROR_NUMBER, &err);
			
			// former: *a++
			for(char **a = fdw_private->columns; *a != NULL; a++) {
				
				column_type_is_array = (fdw_private->column_types[i])[0] == '_';
				
				if(!strcasecmp(*a, "dn"))
				{
					null_values[i] = 0;
					d_values[i] = DirectFunctionCall1(textin, CStringGetDatum(entrydn));
					i++;
					continue;
				}
				if((vals = ldap_get_values_len(fdw_private->ldapConn->ldap, fdw_private->ldap_message_result, *a)) != NULL)
				{
					int values_length = ldap_count_values_len(vals);
					//if(ldap_count_values_len(vals) == 0)
					if(values_length == 0)
					{
						null_values[i] = true;
						d_values[i] = PointerGetDatum(NULL);
					}
					else
					{
						if(!column_type_is_array) {
							null_values[i] = (vals[0]->bv_val == NULL || strlen(vals[0]->bv_val) == 0);
							d_values[i] = DirectFunctionCall1(textin, CStringGetDatum(vals[0]->bv_val));
						} else {
							ArrayType* array_values;
							Datum* item_values = (Datum*)palloc(sizeof(Datum) * values_length + 2);
							memset(item_values, 0, (values_length + 1 ) * sizeof(Datum));
	
							for ( vi = 0; vals[ vi ] != NULL; vi++ ) {
								item_values[vi] = DirectFunctionCall1(textin, CStringGetDatum(vals[ vi ]->bv_val));
							}
							array_values = construct_array(item_values, values_length, varchar_oid, typeLength, typeByValue, typeAlignment);
							d_values[i] = PointerGetDatum(array_values);
						}
						
					}
					ber_bvecfree(vals);
				}
				else
				{
					null_values[i] = true;
					d_values[i] = PointerGetDatum(NULL);
				}
				i++;
			}
			if(entrydn != NULL) {
				ldap_memfree(entrydn);
				entrydn = NULL;
			}
			break;
	}
	
	
	fdw_private->row++;
	
	tuple = heap_form_tuple(tupdesc, d_values, null_values);
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
	LdapFdwPlanState *fdw_private = (LdapFdwPlanState *) node->fdw_state;

	/* if festate is NULL, we are in EXPLAIN; nothing to do */
	if (fdw_private)
	{
		ldap_unbind_ext_s(fdw_private->ldapConn->ldap, NULL, NULL);
		free_options(fdw_private->ldapConn->options);
		free_ldap_message(&(fdw_private->ldap_message_result));
		pfree(fdw_private);
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
	CmdType		operation = plan->operation;
	RangeTblEntry *rte = planner_rt_fetch(resultRelation, root);
	List	   *targetAttrs = NIL;
	TupleDesc	tupdesc;
	List	   *returningList = NIL;
#warning remove unused vars or rewrite code
	/*
	List	   *retrieved_attrs = NIL;
	Oid			array_element_type = InvalidOid;*/
	Relation	rel = NULL;
	//Oid			foreignTableId;
	StringInfoData sql;
#if PG_VERSION_NUM < 130000
	rel = heap_open(rte->relid, NoLock);
#else
	rel = table_open(rte->relid, NoLock);
#endif
	
	initStringInfo(&sql);
	
	/*
	 * Core code already has some lock on each rel being planned, so we can
	 * use NoLock here.
	 */
	//foreignTableId = RelationGetRelid(rel);
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
			/*deparseDeleteSql(&sql, root, resultRelation, rel,
							 returningList,
							 &retrieved_attrs);*/
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
	
	/*
	 * Build the fdw_private list that will be available to the executor.
	 * Items in the list must match enum FdwModifyPrivateIndex, above.
	 */
	/*return list_make4(makeString(sql.data),
					  targetAttrs,
					  makeInteger((returningList != NIL)),
					  retrieved_attrs);*/
	//return list_make1(targetAttrs);
	return list_make2(targetAttrs,
					  makeInteger((returningList != NIL)));
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
	/* Begin constructing LdapFdwModifyState. */
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) palloc0(sizeof(LdapFdwModifyState));
	Relation	rel = resultRelInfo->ri_RelationDesc;
	AttrNumber	n_params;
	Form_pg_attribute attr;
	Oid			typefnoid = InvalidOid;
	bool		isvarlena = false;
	//List		*attrs_list;
	ListCell   *lc;
	Oid			foreignTableId = 0;
	Oid			userid = 0;
	ForeignServer *server = NULL;
	ForeignTable *table;
#warning remove unused vars or rewrite code
	/*
	UserMapping *user;
	*/
#if PG_VERSION_NUM >= 160000
	ForeignScan *fsplan = (ForeignScan *) mtstate->ps.plan;
#else
	EState	   *estate = mtstate->ps.state;
	RangeTblEntry *rte;
#endif

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

	foreignTableId = RelationGetRelid(rel);

	/* Get info about foreign table. */
	table = GetForeignTable(foreignTableId);
	server = GetForeignServer(table->serverid);
#warning work on user-mapping
	//user = GetUserMapping(userid, server->serverid);
	
	fmstate->rel = rel;
	//GetOptionStructr((fmstate->options), foreignTableId);
	fmstate->ldapConn = create_LdapFdwConn();
	GetOptionStructr(fmstate->ldapConn->options, foreignTableId);
	//initLdapWithOptions(fmstate->ldapConn);
	initLdapConnectionStruct(fmstate->ldapConn);

	fmstate->target_attrs = (List *) list_nth(fdw_private, 0);
	fmstate->has_returning = (bool)intVal(list_nth(fdw_private, 1));
	
	//fmstate->retrieved_attrs = (List *) list_nth(fdw_private, 3);

	//n_params = list_length(fmstate->target_attrs) + 1;
	//elog(INFO, "n_params: %d", n_params);
	fmstate->p_flinfo = (FmgrInfo *) palloc(sizeof(FmgrInfo) * n_params);
	fmstate->p_nums = 0;

	if (mtstate->operation == CMD_UPDATE)
	{
#if PG_VERSION_NUM >= 140000
		Plan	   *subplan = outerPlanState(mtstate)->plan;
#else
		Plan	   *subplan = mtstate->mt_plans[subplan_index]->plan;
#endif

		Assert(subplan != NULL);
		
		attr = TupleDescAttr(RelationGetDescr(rel), 0);
		
		fmstate->rowidAttno = ExecFindJunkAttributeInTlist(subplan->targetlist, NameStr(attr->attname));
		
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
		attr = TupleDescAttr(RelationGetDescr(rel), attnum);
		Assert(!attr->attisdropped);
		if(attr->atttypid != 0 && !attr->attisdropped) {
			getTypeOutputInfo(attr->atttypid, &typefnoid, &isvarlena);
			fmgr_info(typefnoid, &fmstate->p_flinfo[fmstate->p_nums]);
			fmstate->p_nums++;
		}
	}
	Assert(fmstate->p_nums <= n_params);

	resultRelInfo->ri_FdwState = fmstate;
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
    Datum       attr_value;
	char 		*dn = NULL;
    bool        isnull;
	LDAPMod		**insert_data = NULL;
	LDAPMod		* single_ldap_mod = NULL;
	TupleDesc	tupdesc;
    int         i = 0, j = 0, rc = 0;
#warning remove unused vars or rewrite code
	/*
	char		*columnName = NULL;
    const char **p_values;
	int			n_rows = 0, p_index = 0;
	Oid			userid;
	UserMapping *user;
	ForeignServer *server;
	ForeignTable *table;
	*/
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Relation rel = resultRelInfo->ri_RelationDesc;
	Oid			foreignTableId = RelationGetRelid(rel);
	tupdesc = RelationGetDescr(rel);
	
//#if PG_VERSION_NUM >= 160000
//	//ForeignScan *fsplan = (ForeignScan *) mstate->ps.plan;
//	ForeignScan *fsplan = (ForeignScan *) estate->ps.state;
//#else
//	//EState	   *estate = mtstate->ps.state;
//	RangeTblEntry *rte;
//#endif

	
//#if PG_VERSION_NUM >= 160000
//	userid = fsplan->checkAsUser ? fsplan->checkAsUser : GetUserId();
//#else
//	rte = rt_fetch(resultRelInfo->ri_RangeTableIndex, estate->es_range_table);
//	userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();
//#endif

//	/* Get info about foreign table. */
//	table = GetForeignTable(foreignTableId);
//	server = GetForeignServer(table->serverid);
//	user = GetUserMapping(userid, server->serverid);*/

	/* Begin constructing LdapFdwModifyState. */
	//fmstate = (LdapFdwModifyState *) palloc0(sizeof(LdapFdwModifyState));

	fmstate->rel = rel;
	/*
	//GetOptionStructr((fmstate->options), foreignTableId);
	fmstate->ldapConn = create_LdapFdwConn();
	GetOptionStructr(fmstate->ldapConn->options, foreignTableId);
	//initLdapWithOptions(fmstate->ldapConn);
	initLdapConnectionStruct(fmstate->ldapConn);
	*/
	j++;
	
    // Iterate all attribute of the tuple
    for (i = 0; i < tupdesc->natts; i++) {
        if (slot->tts_isnull[i]) {
            // Attribute is NULL
            //elog(INFO, "Attribut %s ist NULL", NameStr(tupdesc->attrs[i].attname));
            continue;
        }

        // Hole den Wert des Attributs
        attr_value = slot_getattr(slot, i + 1, &isnull);

        if (!isnull) {
			// Retrieve unique name of attribute
            char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
			// Format the value of the attribute
            char *value_str = DatumGetCString(DirectFunctionCall1(textout, attr_value));
			//if(!strcmp(att_name, "dn")) dn = pstrdup(value_str);
			if(!strcmp(att_name, "dn")) dn = pstrdup(DatumGetCString(DirectFunctionCall1(textout, attr_value)));
			else
			{
				char ** values_array = extract_array_from_datum(attr_value, tupdesc->attrs[i].atttypid);
				
				//single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_ADD, att_name, (char*[]){value_str, NULL});
				single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_ADD, att_name, values_array);
				append_ldap_mod(&insert_data, single_ldap_mod);
				//ldap_mods_free(&single_ldap_mod, true);
				free_ldap_mod(single_ldap_mod);
				single_ldap_mod = NULL;

			}
            pfree(value_str);
			pfree(att_name);
        }
    }
	/*if (slot != NULL && fmstate->target_attrs != NIL)
	{
		Form_pg_attribute attr;
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
	
#warning Throw error when dn is NULL
	if(dn == NULL || !strcmp(dn, ""))
	{
		elog(ERROR, "dn is %s!", dn);
		return NULL;
	}
	
	rc = ldap_add_ext_s( fmstate->ldapConn->ldap, dn, insert_data, NULL, NULL );

	if ( rc != LDAP_SUCCESS ) {
		elog( ERROR, "ldap_add_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );

	}
	
	/*for ( i = 0; i < fmstate->p_nums; i++ ) {

		free( insert_data[ i ] );

	}*/
	for(i = 0; insert_data[i] != NULL; i++) free(insert_data[i]);
	
	if(fmstate->has_returning)
	{
		slot = fetchLdapEntryByDnIntoSlot(fmstate->ldapConn, foreignTableId, dn);
	}
	
	pfree(dn);
	free( insert_data );
	
	
	return slot;
}

/*
 * ldap2_fdw_ExecForeignUpdate
 *		Update one row in a foreign table
 */
/*static TupleTableSlot *
ldap2_fdw_ExecForeignUpdate(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot)
{
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Datum       attr_value, datum;
	bool		isNull = false;
	Oid			foreignTableId;
	Oid			typoid;
	char *dn = NULL;
	char *columnName = NULL;
	int rc = 0;
	int i = 0, j = 0;
	LDAPMod		**modify_data_remove = NULL;
	LDAPMod		**modify_data_add = NULL;
	LDAPMod		* single_ldap_mod = NULL;
	LDAPMod		* single_ldap_mod_remove = NULL, * single_ldap_mod_add = NULL;
	ForeignTable *table;
	Form_pg_attribute attr;
	Relation rel = resultRelInfo->ri_RelationDesc;
	TupleDesc tupdesc = RelationGetDescr(rel);
	ListCell   *lc = NULL;
	foreignTableId = RelationGetRelid(resultRelInfo->ri_RelationDesc);
	datum = ExecGetJunkAttribute(planSlot, fmstate->rowidAttno, &isNull);
	typoid = get_atttype(foreignTableId, 1);
	DEBUGPOINT;
	// Durchlaufe alle Attribute des Tuples
    for (i = 0; i < tupdesc->natts; i++) {
        if (slot->tts_isnull[i]) {
            // Attribut ist NULL
			elog(INFO, "Attribut %s ist NULL in Line %d", NameStr(tupdesc->attrs[i].attname), __LINE__);
			char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
			single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){NULL});
			append_ldap_mod(&modify_data_remove, single_ldap_mod);
			free_ldap_mod(single_ldap_mod);
			single_ldap_mod = NULL;
			DEBUGPOINT;
            continue;
        }
		DEBUGPOINT;
        attr_value = slot_getattr(slot, i + 1, &isNull);

        if (!isNull)
		{
            // Eindeutigen Namen des Attributs erhalten
            char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
            // Wert des Attributs formatieren (z.B. für Logging)
            //char *value_str = pstrdup(DatumGetCString(DirectFunctionCall1(textout, attr_value)));
			//if(!strcmp(att_name, "dn")) dn = pstrdup(value_str);
			DEBUGPOINT;
			if(!strcmp(att_name, "dn")) dn = pstrdup(DatumGetCString(DirectFunctionCall1(textout, attr_value)));
			else
			{
				DEBUGPOINT;
				//elog(INFO, "i: %d, j: %d, Attribut: %s, Wert: %s", i, j, att_name, value_str);
				single_ldap_mod_remove = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, NULL);
				append_ldap_mod(&modify_data_remove, single_ldap_mod_remove);
				
				single_ldap_mod_add = NULL;
				char ** values_array = extract_array_from_datum(attr_value, tupdesc->attrs[i].atttypid);
				//if(value_str == NULL || !strcmp(value_str, "")) {
				if(values_array == NULL)
				{
					elog(INFO, "Function %s, Line: %d: values array = NULL!", __FUNCTION__, __LINE__);
					//single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){value_str, NULL});
					//single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){NULL});
				} else {
					//single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_REPLACE, att_name, (char*[]){value_str, NULL});
					//char ** values_array = extract_array_from_datum(attr_value, tupdesc->attrs[i].atttypid);
					//single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_REPLACE, att_name, values_array);
					single_ldap_mod_add = construct_new_ldap_mod(LDAP_MOD_REPLACE, att_name, values_array);
				}
				elog(INFO, "attname: %s", att_name);
				elog(INFO, "Value: %s", values_array[0]);
				//append_ldap_mod(&modify_data, single_ldap_mod);
				if(single_ldap_mod_add) append_ldap_mod(&modify_data_add, single_ldap_mod_add);
				//ldap_mods_free(&single_ldap_mod, true);
				//free_ldap_mod(single_ldap_mod);
				free_ldap_mod(single_ldap_mod_remove);
				if(single_ldap_mod_add) free_ldap_mod(single_ldap_mod_add);
				//single_ldap_mod = NULL;
				single_ldap_mod_remove = NULL;
				single_ldap_mod_add = NULL;
				
			}
            //pfree(value_str);  // Freigeben des String-Puffers
			pfree(att_name);
		}
		else
		{
			elog(INFO, "Attribut %s ist NULL in Line %d", NameStr(tupdesc->attrs[i].attname), __LINE__);
			char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
			single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){NULL});
			append_ldap_mod(&modify_data_remove, single_ldap_mod);
			free_ldap_mod(single_ldap_mod);
			single_ldap_mod = NULL;
			DEBUGPOINT;
			
		}
	}
	
	if(modify_data_remove != NULL)
	{
		elog(INFO, "ldap mod: Line: %d, dn: %s", __LINE__, dn);
		
		rc = ldap_modify_ext_s( fmstate->ldapConn->ldap, dn, modify_data_remove, NULL, NULL );

		if ( rc != LDAP_SUCCESS ) {
			elog( LOG, "ldap_modify_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );

		}
		else elog(INFO, "ldap_modify success (remove)!");
		
		for( i = 0; modify_data_remove[i] != NULL; i++)
		{
			free( modify_data_remove[ i ] );
		}
		
		free(modify_data_remove);
		modify_data_remove = NULL;
	}
	
	
	if(modify_data_add != NULL)
	{
		elog(INFO, "ldap mod: Line: %d, dn: %s", __LINE__, dn);
		rc = ldap_modify_ext_s( fmstate->ldapConn->ldap, dn, modify_data_add, NULL, NULL );

		if ( rc != LDAP_SUCCESS ) {
			elog( LOG, "ldap_modify_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );

		}
		else elog(INFO, "ldap_modify success (add)!");

		for( i = 0; modify_data_add[i] != NULL; i++)
		{
			free( modify_data_add[ i ] );
		}
		
		pfree(dn);
		free( modify_data_add );
		modify_data_add = NULL;
	}
	
	
	return NULL;
}
*/

/**
 * ldap2_fdw_ExecForeignUpdate
 *		Update one row in a foreign table
 * TODO: LDAP Rename/move on dn change
 */
static TupleTableSlot *
ldap2_fdw_ExecForeignUpdate(EState *estate,
						  ResultRelInfo *resultRelInfo,
						  TupleTableSlot *slot,
						  TupleTableSlot *planSlot)
{
	Datum       attr_value, id_value;
	bool		isNull = false;
	Oid			foreignTableId;
	Oid			typoid;
	char *dn = NULL, *old_dn = NULL;
	int rc = 0;
	int i = 0, j = 0, op_success = 0;
	LDAPMod		**modify_data = NULL;
	LDAPMod		**single_mod_array = NULL;
	LDAPMod		* single_ldap_mod = NULL;
	
	/*
	char *columnName = NULL;
	ForeignTable *table;
	Form_pg_attribute attr;
	ListCell   *lc = NULL;
	*/
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Relation rel = resultRelInfo->ri_RelationDesc;
	TupleDesc tupdesc = RelationGetDescr(rel);
	foreignTableId = RelationGetRelid(resultRelInfo->ri_RelationDesc);
	id_value = ExecGetJunkAttribute(planSlot, fmstate->rowidAttno, &isNull);
	if(!isNull) old_dn = pstrdup(DatumGetCString(DirectFunctionCall1(textout, id_value)));
	
	typoid = get_atttype(foreignTableId, 1);
	
	// Durchlaufe alle Attribute des Tuples
    for (i = 0; i < tupdesc->natts; i++) {
        attr_value = slot_getattr(slot, i + 1, &isNull);
		
		char *att_name = pstrdup(NameStr(tupdesc->attrs[i].attname));
		single_ldap_mod = NULL;
		
        if (slot->tts_isnull[i] || isNull) {
            // Attribut ist NULL
			single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_DELETE, att_name, (char*[]){NULL});
			append_ldap_mod(&modify_data, single_ldap_mod);
			free_ldap_mod(single_ldap_mod);
			single_ldap_mod = NULL;
            continue;
        }
        else if(!isNull)
		{
			if(!strcmp(att_name, "dn")) dn = pstrdup(DatumGetCString(DirectFunctionCall1(textout, attr_value)));
			else
			{
				char ** values_array = extract_array_from_datum(attr_value, tupdesc->attrs[i].atttypid);
				if(values_array == NULL)
				{
					single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_REPLACE, att_name, (char*[]){NULL});
				} else {
					single_ldap_mod = construct_new_ldap_mod(LDAP_MOD_REPLACE, att_name, values_array);
				}
				if(single_ldap_mod) append_ldap_mod(&modify_data, single_ldap_mod);
				if(single_ldap_mod) free_ldap_mod(single_ldap_mod);
				single_ldap_mod = NULL;
				
			}
			pfree(att_name);
		}
	}
	
	/*
	 *	NOTE: Dirty workaround: Every change is committed individually because there are errors if fields are not existent and other fields are changed.
	 *	This may be changed in the future.
	 *  TODO: Improve updates
	 */
	
	if(dn == NULL || !strcmp(dn, ""))
	{
		elog(ERROR, "dn is %s!", dn);
		return NULL;
	}
	
	
	/*
	 * Change of dn requested?
	 */
	if(old_dn != NULL && strcasecmp(dn, old_dn))
	{
		char * comma = index(dn, ','), * parent_rdn = NULL, * child_rdn = NULL;
		if(comma != NULL) 
		{
			parent_rdn = pstrdup(comma + 1);
			child_rdn = pnstrdup(dn, comma - dn);
		}
		else
		{
			child_rdn = pstrdup(dn);
		}
		
		rc = ldap_rename_s(fmstate->ldapConn->ldap, old_dn, child_rdn, parent_rdn, 1, fmstate->ldapConn->serverctrls, fmstate->ldapConn->clientctrls);
		if(rc == LDAP_SUCCESS) op_success++;
		pfree(parent_rdn);
		pfree(child_rdn);
	}
	
	if(modify_data != NULL)
	{
		single_mod_array = (LDAPMod**)palloc(sizeof(LDAPMod*) * 2);
		memset(single_mod_array, 0, sizeof(LDAPMod*) * 2);
		
		for( i = 0; modify_data[i] != NULL; i++)
		{
			single_mod_array[0] = modify_data[i];
			rc = ldap_modify_ext_s( fmstate->ldapConn->ldap, dn, single_mod_array, fmstate->ldapConn->serverctrls, fmstate->ldapConn->clientctrls);

			if( rc == LDAP_SUCCESS) op_success++;
			
			//if ( rc != LDAP_SUCCESS ) elog( INFO, "ldap_modify_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );
			//else elog(INFO, "ldap_modify success!");
		}
		
		/*rc = ldap_modify_ext_s( fmstate->ldapConn->ldap, dn, modify_data, fmstate->ldapConn->serverctrls, fmstate->ldapConn->clientctrls);

		if ( rc != LDAP_SUCCESS ) {
			elog( LOG, "ldap_modify_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) );

		}
		else elog(INFO, "ldap_modify success!");
		*/
		
		for( i = 0; modify_data[i] != NULL; i++)
		{
			free( modify_data[ i ] );
		}
		
		free(modify_data);
		modify_data = NULL;
	}
	
	if(fmstate->has_returning)
	{
		slot = fetchLdapEntryByDnIntoSlot(fmstate->ldapConn, foreignTableId, dn);
	}
	
	return slot;
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
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;;
	Datum       datum;
	bool		isNull = false;
	Oid			foreignTableId;
	char *dn = NULL;
	char *columnName = NULL;
	int rc = 0;
	int i = 0;
	int dn_index = -1;
	Relation rel = resultRelInfo->ri_RelationDesc;
	//TupleDesc tupdesc = RelationGetDescr(rel);
	foreignTableId = RelationGetRelid(resultRelInfo->ri_RelationDesc);
	
	for(i = 0; i < RelationGetDescr(rel)->natts; i++)
	{
		columnName = get_attname(foreignTableId, i + 1, false);
		if(!strcmp(columnName, "dn"))
		{
			dn_index = i;
			break;
		}
	}
	
	//columnName = get_attname(foreignTableId, 1, false);
	/* Get the id that was passed up as a resjunk column */
	if(dn_index < 0) elog(ERROR, "No dn found!");
	datum = ExecGetJunkAttribute(planSlot, dn_index + 1, &isNull);
	//datum = ExecGetJunkAttribute(planSlot, 1, &isNull);
	char *value_str = DatumGetCString(DirectFunctionCall1(textout, datum));
	
	if((dn = pstrdup(value_str)))
	{
		if(fmstate->has_returning)
		{
			slot = fetchLdapEntryByDnIntoSlot(fmstate->ldapConn, foreignTableId, dn);
		}
		
		rc = ldap_delete_ext_s(fmstate->ldapConn->ldap, dn, fmstate->ldapConn->serverctrls, fmstate->ldapConn->clientctrls);

		pfree(dn);
	
		if(rc != LDAP_SUCCESS)
		{
			elog( LOG, "ldap_delete_ext_s: (%d) %s\n", rc, ldap_err2string( rc ) ); 
			return NULL;
		}
	}
	
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
	// cleanup
	LdapFdwModifyState *fmstate = (LdapFdwModifyState *) resultRelInfo->ri_FdwState;

	/* if festate is NULL, we are in EXPLAIN; nothing to do */
	if (fmstate)
	{
		ldap_unbind_ext_s( fmstate->ldapConn->ldap , NULL, NULL);
		fmstate->ldapConn->ldap = NULL;
		pfree(fmstate);
		resultRelInfo->ri_FdwState = NULL;
	}
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

/*
 * from: mysqlGetForeignUpperPaths
 */
static void
ldap2_fdw_GetForeignUpperPaths(PlannerInfo *root, UpperRelationKind stage,
						  RelOptInfo *input_rel, RelOptInfo *output_rel,
						  void *extra)
{
	LdapFdwPlanState *fpinfo;
	Query	   *parse = root->parse;
	
	elog(INFO, "Function: %s", __FUNCTION__);
	
	
	if (!input_rel->fdw_private)
		elog(INFO, "input_rel->fdw_private is not set!");
	if (input_rel->fdw_private && !((LdapFdwPlanState *) input_rel->fdw_private)->pushdown_safe)
		elog(INFO, "input_rel->fdw_private->pushdown_safe is not true!");
	
	/*
	 * If input rel is not safe to pushdown, then simply return as we cannot
	 * perform any post-join operations on the foreign server.
	 */
	if (!input_rel->fdw_private ||
		!((LdapFdwPlanState *) input_rel->fdw_private)->pushdown_safe)
		return;

	elog(INFO, "Function: %s, Line: %d", __FUNCTION__, __LINE__);

	/* Ignore stages we don't support; and skip any duplicate calls. */
	if ((stage != UPPERREL_FINAL) || output_rel->fdw_private)
		return;
	
	elog(INFO, "Function: %s, Line: %d", __FUNCTION__, __LINE__);

	fpinfo = (LdapFdwPlanState *) palloc0(sizeof(LdapFdwPlanState));
	fpinfo->pushdown_safe = false;
	fpinfo->stage = stage;
	output_rel->fdw_private = fpinfo;

	switch (stage)
	{
		case UPPERREL_FINAL:
			ldap2_fdw_add_foreign_final_paths(root, input_rel, output_rel,
										  (FinalPathExtraData *) extra);
			break;
		default:
			elog(ERROR, "unexpected upper relation: %d", (int) stage);
			break;
	}
}


/*
 * ldap2_fdw_add_foreign_final_paths from mysql_add_foreign_final_paths
 *		Add foreign paths for performing the final processing remotely.
 *
 * Given input_rel contains the source-data Paths.  The paths are added to the
 * given final_rel.
 */
static void
ldap2_fdw_add_foreign_final_paths(PlannerInfo *root, RelOptInfo *input_rel,
							  RelOptInfo *final_rel, FinalPathExtraData *extra)
{
	Query	   *parse = root->parse;
	LdapFdwPlanState *ifpinfo = (LdapFdwPlanState *) input_rel->fdw_private;
	LdapFdwPlanState *fpinfo = (LdapFdwPlanState *) final_rel->fdw_private;
	bool		has_final_sort = false;
	List	   *pathkeys = NIL;
	double		rows;
	Cost		startup_cost;
	Cost		total_cost;
	List	   *fdw_private;
	ForeignPath *final_path;
	
	if (parse->commandType != CMD_SELECT)
		return;

	fpinfo->outerrel = input_rel;
	
	elog(INFO, "Has_Limit old Value: %d", fpinfo->has_limit);
	elog(INFO, "Has_Offset old Value: %d", fpinfo->has_offset);
	elog(INFO, "Rows: %d, ntuples: %d", fpinfo->row, fpinfo->ntuples);
	
	if (parse->limitCount)
	{
		elog(INFO, "Line %d: Limit found!", __LINE__);
		fpinfo->limit_count = -1;
		if (IsA(parse->limitCount, Const)) {
				Const *constNode = (Const *)parse->limitCount;

			// Check if the value is not NULL and it is of integer type
			if ((constNode->consttype == INT2OID || constNode->consttype == INT4OID || constNode->consttype == INT8OID) && !constNode->constisnull) {
				fpinfo->has_limit = true;
				// Extract the integer value
				fpinfo->limit_count = DatumGetInt32(constNode->constvalue);
			}
		}
		elog(INFO, "Limit Value: %d", fpinfo->limit_count);
	}
	
	if (parse->limitOffset)
	{
		elog(INFO, "Line %d: Offset found!", __LINE__);
		fpinfo->limit_offset = -1;
		if (IsA(parse->limitOffset, Const)) {
				Const *constNode = (Const *)parse->limitOffset;

			// Check if the value is not NULL and it is of integer type
			if ((constNode->consttype == INT2OID || constNode->consttype == INT4OID || constNode->consttype == INT8OID) && !constNode->constisnull) {
				fpinfo->has_offset = true;
				// Extract the integer value
				fpinfo->limit_offset = DatumGetInt32(constNode->constvalue);
			}
		}
		elog(INFO, "Offset Value: %d", fpinfo->limit_offset);
	}
	
	
	startup_cost = 1;
	total_cost = 1 + startup_cost;
	rows = fpinfo->limit_count;
	
	/*
	 * Build the fdw_private list that will be used by mysqlGetForeignPlan.
	 * Items in the list must match order in enum FdwPathPrivateIndex.
	 */
	/*fdw_private = list_make2(makeInteger(has_final_sort),
							 makeInteger(extra->limit_needed));*/
	fdw_private = list_make2(makeInteger(fpinfo->has_limit), makeInteger(fpinfo->has_offset));

	/*
	 * Create foreign final path; this gets rid of a no-longer-needed outer
	 * plan (if any), which makes the EXPLAIN output look cleaner
	 */
#if PG_VERSION_NUM >= 170000
	final_path = create_foreign_upper_path(root,
										   input_rel,
										   root->upper_targets[UPPERREL_FINAL],
										   rows,
										   startup_cost,
										   total_cost,
										   pathkeys,
										   NULL,	/* no extra plan */
										   NIL,		/* no fdw_restrictinfo list */
										   fdw_private);
#else
	final_path = create_foreign_upper_path(root,
										   input_rel,
										   root->upper_targets[UPPERREL_FINAL],
										   rows,
										   startup_cost,
										   total_cost,
										   pathkeys,
										   NULL,	/* no extra plan */
										   fdw_private);
#endif

	/* and add it to the final_rel */
	add_path(final_rel, (Path *) final_path);
	
}

/**
 * @see https://github.com/pgspider/jdbc_fdw/blob/main/jdbc_fdw.c function jdbcImportForeignSchema
 * Method to fetch Schema from foreign Server 
 *
 */
static List *
ldap2_fdw_ImportForeignSchema(ImportForeignSchemaStmt *stmt, Oid serverOid)
{
	List			*commands = NIL;
	List			*options = NIL;
	List			*columns = NIL;
	List			*attr_typemap = NIL;
	ListCell		*lc, *oc_lc;
	//AttrListType	**attr_typemap = Create_AttrListType();
	bool			recreate = false;
	//char			**columns = NULL;
	char			*tablename = NULL;
	char			*schemaname = NULL;
	char			*dropStr = NULL;
	char			*createStr = NULL;
	char			*scope = NULL;
	char			*use_remotefiltering = NULL;
	//char			**objectClasses = NULL;
	List			*objectClasses = NIL;
	ForeignServer	*server;
	UserMapping		*user;
	size_t			num_attrs = 0;
	int				len = 0;
	server = GetForeignServer(serverOid);
	user = GetUserMapping(GetUserId(), server->serverid);
	LdapFdwConn *ldapConn = create_LdapFdwConn();
	
	options = list_concat(options, server->options);
	options = list_concat(options, user->options);
	
	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);
		if(!strcmp(def->defname, "uri")) ldapConn->options->uri = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "username")) ldapConn->options->username = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "password")) ldapConn->options->password = pstrdup(defGetString(def));
	}
	
	foreach(lc, stmt->options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (!strcmp(def->defname, "recreate")) recreate = defGetBoolean(def);
		else if(!strcmp(def->defname, "basedn")) ldapConn->options->basedn = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "filter")) ldapConn->options->filter = pstrdup(defGetString(def));
		//else if(!strcmp(def->defname, "objectclass")) array_push(&objectClasses, defGetString(def));
		else if(!strcmp(def->defname, "objectclass")) objectClasses = lappend(objectClasses, pstrdup(defGetString(def)));
		else if(!strcmp(def->defname, "schemadn")) ldapConn->options->schemadn = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "scope")) scope = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "tablename")) tablename = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "schemaname")) schemaname = pstrdup(defGetString(def));
		else if(!strcmp(def->defname, "use_remotefiltering"))
		{
			char * rf_value = defGetString(def);
			if(!strcasecmp(rf_value, "t") || !strcasecmp(rf_value, "true")) use_remotefiltering = pstrdup("1");
			else if(!strcasecmp(rf_value, "f") || !strcasecmp(rf_value, "false")) use_remotefiltering = pstrdup("0");
			else use_remotefiltering = pstrdup(rf_value);
			parse_int(use_remotefiltering, &(ldapConn->options->use_remotefiltering), 0, NULL);
		}
		else
			ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					errmsg("invalid option \"%s\"", def->defname)));
	}
	
	if(ldapConn->options->schemadn == NULL || !strcmp(ldapConn->options->schemadn, ""))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					errmsg("Schemadn empty! Read your ldap server manual. usually it's cn=schema or cn=subschema.")));
	}
	
	if(tablename == NULL)
	{
		elog(ERROR, "tablename is NULL!");
	}
	
	if(schemaname == NULL) schemaname = pstrdup("public");

	initLdapConnectionStruct(ldapConn);
	
	//num_attrs = fetch_ldap_typemap(&attr_typemap, &columns, ldapConn->ldap, ldapConn->options->objectclasses, ldapConn->options->schemadn);
	num_attrs = fetch_ldap_typemap(&attr_typemap, &columns, ldapConn->ldap, objectClasses, ldapConn->options->schemadn);
	
	fill_AttrListType(attr_typemap, typemap);
	
	/*for(int i = 0; attr_typemap[i] != NULL; i++)
	{
		elog(INFO, "attr_typemap[%d] {attr_name: %s, ldap_type: %s, pg_type: %s, isarray: %d", i, attr_typemap[i]->attr_name, attr_typemap[i]->ldap_type, attr_typemap[i]->pg_type, attr_typemap[i]->isarray);
	}*/
	
	//len = asprintf(&dropStr, "DROP FOREIGN TABLE IF EXISTS \"%s\";", tablename);
	//len = asprintf(&dropStr, "DROP FOREIGN TABLE IF EXISTS \"%s\".\"%s\";", schemaname, tablename);
	dropStr = psprintf("DROP FOREIGN TABLE IF EXISTS \"%s\".\"%s\";", schemaname, tablename);
	if(dropStr == NULL) elog(ERROR, "Cound not create drop string!");
	//if(len == 0) elog(ERROR, "Cound not create drop string!");
	
	createStr = pstrdup("");
	//strmcat_multi(&createStr, "CREATE FOREIGN TABLE IF NOT EXISTS \"", tablename, "\" (");
	pstrmcat_multi(&createStr, "CREATE FOREIGN TABLE IF NOT EXISTS \"", schemaname, "\".\"", tablename, "\" (");
	
//	for(int i = 0; columns[i] != NULL; i++)
	
	foreach(lc, columns)
	{
		char * columnName = lfirst(lc);
		char * attrType = getAttrTypeByAttrName(attr_typemap, columnName);
		bool nullable = getAttrNullableByAttrName(attr_typemap, columnName);
		char * defaultValue = pstrdup("");
		char * nullable_string = pstrdup(nullable?"NULL":"NOT NULL");
		if(!strcasecmp(columnName, "uid")) pstrmcat_multi(&defaultValue, "DEFAULT array[uuid_generate_v4()]");
		else if(!strcasecmp(columnName, "objectclass"))
		{
			if(objectClasses != NULL)
			{
				bool first = true;
				attrType = "varchar[]";
				//for(int obj_i = 0; objectClasses[obj_i] != NULL; obj_i++)
				foreach(oc_lc, objectClasses)
				{
					char *singleObjectClass = lfirst(oc_lc);
					if(first) pstrmcat_multi(&defaultValue, "DEFAULT array[");
					pstrmcat_multi(&defaultValue, "'", singleObjectClass, "',");
					first = false;
				}
				if(defaultValue[strlen(defaultValue) - 1] == ',') defaultValue[strlen(defaultValue) - 1] = ']';
			}
		}
		pstrmcat_multi(&createStr, "\"", columnName, "\"", " ", attrType, " ", nullable_string, " ", defaultValue, ", ");
		pfree(defaultValue);
		pfree(nullable_string);
		defaultValue = NULL;
	}
	
	while(createStr[strlen(createStr) - 1] == ' ' || createStr[strlen(createStr) - 1] == ',')
		createStr[strlen(createStr) - 1] = '\0';
	
	pstrmcat_multi(&createStr, ") SERVER ", server->servername, " OPTIONS(");
	if(ldapConn->options->basedn != NULL)
	{
		pstrmcat_multi(&createStr, " basedn '", ldapConn->options->basedn, "',");
	}
	if(ldapConn->options->filter != NULL)
	{
		pstrmcat_multi(&createStr, " filter '", ldapConn->options->filter, "',");
	}
	if(scope != NULL)
	{
		pstrmcat_multi(&createStr, " scope '" , scope, "',");
	}
	
	if(use_remotefiltering != NULL)
	{
		pstrmcat_multi(&createStr, " use_remotefiltering '", use_remotefiltering, "',");
	}
	
	if(createStr[strlen(createStr) - 1] == ',') createStr[strlen(createStr) - 1] = 0;
	pstrmcat_multi(&createStr, ");");
	
	if(recreate) commands = lappend(commands, pstrdup(dropStr));
	commands = lappend(commands, pstrdup(createStr));
	
	pfree(dropStr);
	pfree(createStr);
	
	ldap_unbind_ext_s(ldapConn->ldap, NULL, NULL);
	free_options(ldapConn->options);
	
	return commands;
}

