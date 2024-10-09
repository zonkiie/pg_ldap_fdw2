#include "deparse.h"

#define DEBUGPOINT elog(INFO, "ereport File %s, Func %s, Line %d\n", __FILE__, __FUNCTION__, __LINE__)

typedef struct deparse_expr_cxt
{
	PlannerInfo *root;			/* global planner state */
	RelOptInfo *foreignrel;		/* the foreign relation we are planning for */
	RelOptInfo *scanrel;		/* the underlying scan relation. Same as
								 * foreignrel, when that represents a join or
								 * a base relation. */
	List	  **params_list;	/* exprs that will become remote Params */
	bool		is_not_distinct_op; /* True in case of IS NOT DISTINCT clause */
	Oid 		foreignTableId;
	char        *cbuf;
	char		*colname;
	bool		remote_handle_able;
	List		*dn_list;
} deparse_expr_cxt;

static void deparseExpr(Expr *expr, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_relation(StringInfo buf, Relation rel);
static void ldap2_fdw_deparse_var(Var *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_const(Const *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_param(Param *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_array_ref(SubscriptingRef *node,
									deparse_expr_cxt *context);
static void ldap2_fdw_deparse_func_expr(FuncExpr *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_op_expr(OpExpr *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_distinct_expr(DistinctExpr *node,
										deparse_expr_cxt *context);
static void ldap2_fdw_deparse_relabel_type(RelabelType *node,
									   deparse_expr_cxt *context);
static void ldap2_fdw_deparse_bool_expr(BoolExpr *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_null_test(NullTest *node, deparse_expr_cxt *context);
static void ldap2_fdw_deparse_aggref(Aggref *node, deparse_expr_cxt *context);
//static void deparseRelation(StringInfo buf, Relation rel);
// static void ldap2_fdw_deparse_from_expr(List *, deparse_expr_cxt *);
// static void ldap2_fdw_append_conditions(List *, deparse_expr_cxt *);

/**
 * FROM mongo_query.c, function mongo_is_foreign_expr
 */
bool ldap_fdw_is_foreign_expr(PlannerInfo *root, RelOptInfo *baserel, Expr *expression, bool is_having_cond)
{
#warning implement code
	return false;
}




static void
ldap2_fdw_deparse_relation(StringInfo buf, Relation rel)
{
	ForeignTable *table;
	const char *nspname = NULL;
	const char *relname = NULL;
	ListCell   *lc;

	/* Obtain additional catalog information. */
	table = GetForeignTable(RelationGetRelid(rel));

	/*
	 * Use value of FDW options if any, instead of the name of object itself.
	 */
	foreach(lc, table->options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "dbname") == 0)
			nspname = defGetString(def);
		else if (strcmp(def->defname, "table_name") == 0)
			relname = defGetString(def);
	}

	/*
	 * Note: we could skip printing the schema name if it's pg_catalog, but
	 * that doesn't seem worth the trouble.
	 */
	if (nspname == NULL)
		nspname = get_namespace_name(RelationGetNamespace(rel));
	if (relname == NULL)
		relname = RelationGetRelationName(rel);

	//appendStringInfo(buf, "%s.%s", mysql_quote_identifier(nspname, '`'),  mysql_quote_identifier(relname, '`'));
}


static void
ldap2_fdw_deparse_var(Var *node, deparse_expr_cxt *context)
{
	//Relids		relids = context->scanrel->relids;
	ForeignTable *table = GetForeignTable(context->foreignTableId);
	RangeTblEntry *rte = planner_rt_fetch(node->varno, context->root);
	char	*colname = get_attname(rte->relid, node->varno, false);
	context->colname = pstrdup(colname);
	//elog(INFO, "Colname: %s", colname);
	strmcat_multi(&(context->cbuf), "(", colname, "=");
}

static void
ldap2_fdw_deparse_const(Const *node, deparse_expr_cxt *context)
{
	Oid			typoutput;
	bool		typIsVarlena;
	char	   *extval;
	
	if (node->constisnull)
	{
		return;
	}
	
	switch (node->consttype)
	{
		case INT2OID:
		case INT4OID:
		case INT8OID:
		case OIDOID:
		case FLOAT4OID:
		case FLOAT8OID:
		case NUMERICOID:
			{
				DEBUGPOINT;
				extval = OidOutputFunctionCall(typoutput, node->constvalue);

				/*
				 * No need to quote unless it's a special value such as 'NaN'.
				 * See comments in get_const_expr().
				 */
				/*if (strspn(extval, "0123456789+-eE.") == strlen(extval))
				{
					if (extval[0] == '+' || extval[0] == '-')
						//appendStringInfo(buf, "(%s)", extval);
					else
						//appendStringInfoString(buf, extval);
				}
				else
					//appendStringInfo(buf, "'%s'", extval);
				*/
			}
			break;
		case BITOID:
		case VARBITOID:
			DEBUGPOINT;
			extval = OidOutputFunctionCall(typoutput, node->constvalue);
			//appendStringInfo(buf, "B'%s'", extval);
			break;
		case BOOLOID:
			DEBUGPOINT;
			extval = OidOutputFunctionCall(typoutput, node->constvalue);
			/*if (strcmp(extval, "t") == 0)
				appendStringInfoString(buf, "true");
			else
				appendStringInfoString(buf, "false");*/
			break;
		case INTERVALOID:
			//deparse_interval(buf, node->constvalue);
			elog(ERROR, "Cannot handle interval!");
			break;
		case BYTEAOID:
			DEBUGPOINT;
			/*
			 * The string for BYTEA always seems to be in the format "\\x##"
			 * where # is a hex digit, Even if the value passed in is
			 * 'hi'::bytea we will receive "\x6869". Making this assumption
			 * allows us to quickly convert postgres escaped strings to mysql
			 * ones for comparison
			 */
			extval = OidOutputFunctionCall(typoutput, node->constvalue);
			//appendStringInfo(buf, "X\'%s\'", extval + 2);
			break;
		default:
			//extval = OidOutputFunctionCall(typoutput, node->constvalue);
			extval = DatumGetCString(DirectFunctionCall1(textout, node->constvalue));
			if(!strcmp(context->colname, "dn")) context->dn_list = list_make1(makeString(extval));
			//appendStringInfo(context->buf, "%s)", extval);
			strmcat_multi(&(context->cbuf), extval);
			//mysql_deparse_string_literal(buf, extval);
			break;
	}
	strmcat_multi(&(context->cbuf), ")");
}

static void
ldap2_fdw_deparse_param(Param *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

static void
ldap2_fdw_deparse_array_ref(SubscriptingRef *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

static void
ldap2_fdw_deparse_func_expr(FuncExpr *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

/**
 * From mysql_deparse_op_expr
 */
static void
ldap2_fdw_deparse_op_expr(OpExpr *node, deparse_expr_cxt *context)
{
	HeapTuple	tuple;
	Form_pg_operator form;
	ListCell   *arg;
	char		oprkind;

	tuple = SearchSysCache1(OPEROID, ObjectIdGetDatum(node->opno));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for operator %u", node->opno);
	form = (Form_pg_operator) GETSTRUCT(tuple);
	oprkind = form->oprkind;
	
	char * opname = NameStr(form->oprname);
	
	//elog(INFO, "Opname: %s", opname);
	/* Deparse left operand. */
	if (oprkind == 'r' || oprkind == 'b')
	{
		arg = list_head(node->args);
		deparseExpr(lfirst(arg), context);
		//appendStringInfoChar(buf, ' ');
	}

	/* Deparse right operand. */
	if (oprkind == 'l' || oprkind == 'b')
	{
		arg = list_tail(node->args);
		//appendStringInfoChar(buf, ' ');
		deparseExpr(lfirst(arg), context);
	}

	if(strcmp(opname, "="))
	{
		context->remote_handle_able = false;
		DEBUGPOINT;
	}
	//appendStringInfoChar(buf, ')');

	ReleaseSysCache(tuple);
	
}

static void
ldap2_fdw_deparse_distinct_expr(DistinctExpr *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

static void
ldap2_fdw_deparse_scalar_array_op_expr(ScalarArrayOpExpr *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
	if(list_length(node->args) == 2)
	{
		HeapTuple	tuple;
		Expr		*arg1 = linitial(node->args);
		DEBUGPOINT;
		Expr		*arg2 = lsecond(node->args);
		DEBUGPOINT;
		Form_pg_operator form;
		DEBUGPOINT;
		char	   *opname;
		
		DEBUGPOINT;
		
		tuple = SearchSysCache1(OPEROID, ObjectIdGetDatum(node->opno));
		DEBUGPOINT;
		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for operator %u", node->opno);
		form = (Form_pg_operator) GETSTRUCT(tuple);
		opname = NameStr(form->oprname);
		elog(INFO, "Opname: %s", opname);
		//if (strcmp(opname, "<>") == 0)
		//	appendStringInfo(buf, " NOT");
		//
		DEBUGPOINT;
		
		if (IsA(arg2, Const))
		{
			Const	   *arrayconst = (Const *) arg2;
			int 		i;
			int			num_attr;
			Datum	   *attr;
			int16		elmlen;
			bool		elmbyval;
			bool		typIsVarlena;
			char		elmalign;
			Oid 		typOutput;
			bool	   *nullsp = NULL;
			Oid			elemtype;
			ArrayType  *arrayval = DatumGetArrayTypeP(arrayconst->constvalue);
			elemtype = ARR_ELEMTYPE(arrayval);
			get_typlenbyvalalign(elemtype, &elmlen, &elmbyval, &elmalign);
			deconstruct_array(arrayval, elemtype, elmlen, elmbyval,
							elmalign, &attr, &nullsp, &num_attr);
			getTypeOutputInfo(elemtype, &typOutput, &typIsVarlena);
		}
	}
	DEBUGPOINT;
	
}

static void
ldap2_fdw_deparse_relabel_type(RelabelType *node, deparse_expr_cxt *context)
{
	deparseExpr(node->arg, context);
}

static void
ldap2_fdw_deparse_bool_expr(BoolExpr *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

static void
ldap2_fdw_deparse_null_test(NullTest *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

static void
ldap2_fdw_deparse_aggref(Aggref *node, deparse_expr_cxt *context)
{
	DEBUGPOINT;
}

/**
 * aus mysql_deparse_select_stmt_for_rel
 */
char * ldap2_fdw_extract_dn(PlannerInfo * root, Oid foreignTableId, List *scan_clauses)
{
	deparse_expr_cxt context;
	context.foreignTableId = foreignTableId;
	context.root = root;
	context.remote_handle_able = true;
	context.cbuf = strdup("");
	ListCell *cell = NULL;
	char * retval = NULL;
	int count = 0;
	foreach(cell, scan_clauses) {
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(cell);
		//Node *expr = (Node*)rinfo->clause;
		//deparseExpr(expr, &context);
		deparseExpr(rinfo->clause, &context);
		count++;
	}
	elog(INFO, "context.cbuf: %s, count: %d, remote_handle_able: %d", context.cbuf, count, context.remote_handle_able);
	retval = pstrdup(context.cbuf);
	free(context.cbuf);
	context.cbuf = NULL;
	if(!context.remote_handle_able) return NULL;
	if(count > 1) return NULL;
	if(retval[0] == ')') return NULL;
	return retval;
	
}

char * ldap2_fdw_extract_dn_value(PlannerInfo * root, Oid foreignTableId, List *scan_clauses)
{
	deparse_expr_cxt context;
	context.foreignTableId = foreignTableId;
	context.root = root;
	context.remote_handle_able = true;
	context.cbuf = strdup("");
	ListCell *cell = NULL;
	char * retval = NULL;
	int count = 0;
	foreach(cell, scan_clauses) {
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(cell);
		//Node *expr = (Node*)rinfo->clause;
		//deparseExpr(expr, &context);
		deparseExpr(rinfo->clause, &context);
		count++;
	}
	if (list_length(context.dn_list) > 0) retval = strVal(list_nth(context.dn_list, 0));
	if(!context.remote_handle_able) return NULL;
	if(count > 1) return NULL;
	return retval;
	
}



/*
 * from mysql_deparse_from_expr, mysql_fdw
 * 		Construct a FROM clause and, if needed, a WHERE clause, and
 * 		append those to "buf".
 *
 * quals is the list of clauses to be included in the WHERE clause.
 */
// static void
// ldap2_fdw_deparse_from_expr(List *quals, deparse_expr_cxt *context)
// {
// 	StringInfo	buf = context->buf;
// 	RelOptInfo *scanrel = context->scanrel;
// 
// 	/* For upper relations, scanrel must be either a joinrel or a baserel */
// 	Assert(!IS_UPPER_REL(context->foreignrel) ||
// 		   IS_JOIN_REL(scanrel) || IS_SIMPLE_REL(scanrel));
// 
// 	/* Construct FROM clause */
// 	appendStringInfoString(buf, " FROM ");
// 	mysql_deparse_from_expr_for_rel(buf, context->root, scanrel,
// 									(bms_membership(scanrel->relids) == BMS_MULTIPLE),
// 									context->params_list);
// 
// 	/* Construct WHERE clause */
// 	if (quals != NIL)
// 	{
// 		appendStringInfoString(buf, " WHERE ");
// 		mysql_append_conditions(quals, context);
// 	}
// }

/*
 * from mysql_append_conditions
 * 		Deparse conditions from the provided list and append them to buf.
 *
 * The conditions in the list are assumed to be ANDed.  This function is used
 * to deparse WHERE clauses, JOIN .. ON clauses and HAVING clauses.
 *
 * Depending on the caller, the list elements might be either RestrictInfos
 * or bare clauses.
 */
// static void
// ldap2_fdw_append_conditions(List *exprs, deparse_expr_cxt *context)
// {
// 	ListCell   *lc;
// 	bool		is_first = true;
// 	StringInfo	buf = context->buf;
// 
// 	foreach(lc, exprs)
// 	{
// 		Expr	   *expr = (Expr *) lfirst(lc);
// 
// 		/*
// 		 * Extract clause from RestrictInfo, if required. See comments in
// 		 * declaration of MySQLFdwRelationInfo for details.
// 		 */
// 		if (IsA(expr, RestrictInfo))
// 		{
// 			RestrictInfo *ri = (RestrictInfo *) expr;
// 
// 			expr = ri->clause;
// 		}
// 
// 		/* Connect expressions with "AND" and parenthesize each condition. */
// 		if (!is_first)
// 			appendStringInfoString(buf, " AND ");
// 
// 		appendStringInfoChar(buf, '(');
// 		deparseExpr(expr, context);
// 		appendStringInfoChar(buf, ')');
// 
// 		is_first = false;
// 	}
// }




/*
 * Append remote name of specified foreign table to buf.
 * Use value of table_name FDW option (if any) instead of relation's name.
 * Similarly, schema_name FDW option overrides schema name.
 */
// static void
// deparseRelation(StringInfo buf, Relation rel)
// {
// 	ForeignTable *table;
// 	const char *nspname = NULL;
// 	const char *relname = NULL;
// 	ListCell   *lc;
// 
// 	/* obtain additional catalog information. */
// 	table = GetForeignTable(RelationGetRelid(rel));
// 
// 	/*
// 	 * Use value of FDW options if any, instead of the name of object itself.
// 	 */
// 	foreach(lc, table->options)
// 	{
// 		DefElem    *def = (DefElem *) lfirst(lc);
// 
// 		if (strcmp(def->defname, "schema_name") == 0)
// 			nspname = defGetString(def);
// 		else if (strcmp(def->defname, "table_name") == 0)
// 			relname = defGetString(def);
// 	}
// 
// 	/*
// 	 * Note: we could skip printing the schema name if it's pg_catalog, but
// 	 * that doesn't seem worth the trouble.
// 	 */
// 	if (nspname == NULL){
// 		nspname = get_namespace_name(RelationGetNamespace(rel));
// 	}
// 	if (relname == NULL){
// 		relname = RelationGetRelationName(rel);
// 	}
// 
// 	if(strlen(nspname) == 0){ // schema_name '', will omit the schema from the object name
// 		appendStringInfo(buf, "%s", quote_identifier(relname));
// 	} else {
// 		appendStringInfo(buf, "%s.%s", quote_identifier(nspname), quote_identifier(relname));
// 	}
// }


/*
 * Deparse given expression into context->buf.
 *
 * This function must support all the same node types that foreign_expr_walker
 * accepts.
 *
 * Note: unlike ruleutils.c, we just use a simple hard-wired parenthesization
 * scheme: anything more complex than a Var, Const, function call or cast
 * should be self-parenthesized.
 */
static void
deparseExpr(Expr *node, deparse_expr_cxt *context)
{
	if (node == NULL)
		return;

	switch (nodeTag(node))
	{
		case T_Var:
			ldap2_fdw_deparse_var((Var *) node, context);
			break;
		case T_Const:
			ldap2_fdw_deparse_const((Const *) node, context);
			break;
// 		case T_Param:
// 			ldap2_fdw_deparse_param((Param *) node, context);
// 			break;
// 		case T_SubscriptingRef:
// 			ldap2_fdw_deparse_array_ref((SubscriptingRef *) node, context);
// 			break;
// 		case T_FuncExpr:
// 			ldap2_fdw_deparse_func_expr((FuncExpr *) node, context);
// 			break;
		case T_OpExpr:
			ldap2_fdw_deparse_op_expr((OpExpr *) node, context);
			break;
// 		case T_DistinctExpr:
// 			ldap2_fdw_deparse_distinct_expr((DistinctExpr *) node, context);
// 			break;
		case T_ScalarArrayOpExpr:
			ldap2_fdw_deparse_scalar_array_op_expr((ScalarArrayOpExpr *) node, context);
			break;
		case T_RelabelType:
			ldap2_fdw_deparse_relabel_type((RelabelType *) node, context);
			break;
// 		case T_BoolExpr:
// 			ldap2_fdw_deparse_bool_expr((BoolExpr *) node, context);
// 			break;
// 		case T_NullTest:
// 			ldap2_fdw_deparse_null_test((NullTest *) node, context);
// 			break;
// 		case T_Aggref:
// 			ldap2_fdw_deparse_aggref((Aggref *) node, context);
// 			break;
		case T_ArrayCoerceExpr:
			//elog(ERROR, "T_ArrayCoerceExpr expression not supported! NodeTag: %d", T_ArrayCoerceExpr);
		default:
			//elog(ERROR, "unsupported expression type for deparse: %d",
			//	 (int) nodeTag(node));
			elog(INFO, "unsupported expression type for deparse: %d", (int) nodeTag(node));
			context->remote_handle_able = false;
			break;
	}
}





/*
 * deparse remote DELETE statement
 *
 * The statement text is appended to buf, and we also create an integer List
 * of the columns being retrieved by RETURNING (if any), which is returned
 * to *retrieved_attrs.
 */
// void
// deparseDeleteSql(StringInfo buf, PlannerInfo *root,
// 				 Index rtindex, Relation rel,
// 				 List *returningList,
// 				 List **retrieved_attrs)
// {
// 	appendStringInfoString(buf, "DELETE FROM ");
// 	deparseRelation(buf, rel);
// 	appendStringInfoString(buf, " WHERE ctid = $1");
// 
// }
