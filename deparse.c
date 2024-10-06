#include "deparse.h"


//static void deparseRelation(StringInfo buf, Relation rel);
// static void ldap2_fdw_deparse_from_expr(List *, deparse_expr_cxt *);
// static void ldap2_fdw_append_conditions(List *, deparse_expr_cxt *);

/**
 * FROM mongo_query.c, function mongo_is_foreign_expr
 */
bool ldap_fdw_is_foreign_expr(PlannerInfo *root, RelOptInfo *baserel, Expr *expression, bool is_having_cond)
{
#warning implement code
	return true;
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
// static void
// deparseExpr(Expr *node, deparse_expr_cxt *context)
// {
// 	if (node == NULL)
// 		return;
// 
// 	switch (nodeTag(node))
// 	{
// 		case T_Var:
// 			mysql_deparse_var((Var *) node, context);
// 			break;
// 		case T_Const:
// 			mysql_deparse_const((Const *) node, context);
// 			break;
// 		case T_Param:
// 			mysql_deparse_param((Param *) node, context);
// 			break;
// 		case T_SubscriptingRef:
// 			mysql_deparse_array_ref((SubscriptingRef *) node, context);
// 			break;
// 		case T_FuncExpr:
// 			mysql_deparse_func_expr((FuncExpr *) node, context);
// 			break;
// 		case T_OpExpr:
// 			mysql_deparse_op_expr((OpExpr *) node, context);
// 			break;
// 		case T_DistinctExpr:
// 			mysql_deparse_distinct_expr((DistinctExpr *) node, context);
// 			break;
// 		case T_ScalarArrayOpExpr:
// 			mysql_deparse_scalar_array_op_expr((ScalarArrayOpExpr *) node,
// 											   context);
// 			break;
// 		case T_RelabelType:
// 			mysql_deparse_relabel_type((RelabelType *) node, context);
// 			break;
// 		case T_BoolExpr:
// 			mysql_deparse_bool_expr((BoolExpr *) node, context);
// 			break;
// 		case T_NullTest:
// 			mysql_deparse_null_test((NullTest *) node, context);
// 			break;
// 		case T_Aggref:
// 			mysql_deparse_aggref((Aggref *) node, context);
// 			break;
// 		default:
// 			elog(ERROR, "unsupported expression type for deparse: %d",
// 				 (int) nodeTag(node));
// 			break;
// 	}
// }
// 


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
