#include "extraction_functions.h"

// Gemini-created funktions from here to EOF

static char *
const_to_string(Const *c)
{
	Oid typoutput;
	bool typisvarlena;

	if (c->constisnull)
		return NULL;

	getTypeOutputInfo(c->consttype, &typoutput, &typisvarlena);
	return OidOutputFunctionCall(typoutput, c->constvalue);
}

/* Hilfsfunktion: Entpackt Var aus Casts/Coercions */
static Var *
get_base_var(Node *node)
{
	while (node)
	{
		if (IsA(node, Var))
			return (Var *) node;
		else if (IsA(node, RelabelType))
			node = (Node *) ((RelabelType *) node)->arg;
		else if (IsA(node, ArrayCoerceExpr))
			node = (Node *) ((ArrayCoerceExpr *) node)->arg;
		else
			break;
	}
	return NULL;
}

/* Rekursive Funktion zum Bauen des LDAP-Filters */
static char *
expr_to_ldap_filter(Node *node, PlannerInfo *root, RelOptInfo *baserel)
{
	if (node == NULL)
		return NULL;

	/* Handle RestrictInfo Wrapper */
	if (IsA(node, RestrictInfo))
	{
		return expr_to_ldap_filter((Node *) ((RestrictInfo *) node)->clause, root, baserel);
	}

	/* Handle OpExpr (z.B. =, >=, <=, &&) */
	if (IsA(node, OpExpr))
	{
		OpExpr *opexpr = (OpExpr *) node;
		
		/* Wir erwarten genau 2 Argumente (Links Op Rechts) */
		if (list_length(opexpr->args) != 2)
			return NULL;

		Node *left = (Node *) linitial(opexpr->args);
		Node *right = (Node *) lsecond(opexpr->args);

		Var *var = get_base_var(left);
		
		/* Fall 1: Gleichheit (=) oder Array-Overlap (&&) mit Spalte links und Konstante rechts */
		if (var && IsA(right, Const))
		{
			AttrNumber attnum = var->varattno;
			/* Hol den Postgres-Spaltennamen (z.B. "cn") */
			//char *attr_name = get_attname(RelationGetRelid(baserel->generichost), attnum, false);
			char *attr_name = get_attname(baserel->relid, attnum, false);
			
			/* Wenn es ein Array-Match ist (wie bei deinem cn::text[] && array['...']) */
			if (IsA(right, Const))
			{
				Const *cnt = (Const *) right;
				
				/* Falls es ein Skalar-Wert ist (z.B. cn = 'FirstName1') */
				if (!cnt->constisnull)
				{
					char *val_str = const_to_string(cnt);
					
					// Beispiel für einfachen Filter: (cn=FirstName1)
					return psprintf("(%s=%s)", attr_name, val_str);
				}
			}
		}
	}

	/* Handle AND / OR Verknüpfungen */
	if (IsA(node, BoolExpr))
	{
		BoolExpr *boolexpr = (BoolExpr *) node;
		StringInfoData buf;
		initStringInfo(&buf);

		if (boolexpr->boolop == AND_EXPR)
			appendStringInfo(&buf, "(&");
		else if (boolexpr->boolop == OR_EXPR)
			appendStringInfo(&buf, "(|");
		else if (boolexpr->boolop == NOT_EXPR)
			appendStringInfo(&buf, "(!");
		else
			return NULL; /* Nicht unterstützt */

		ListCell *lc;
		foreach(lc, boolexpr->args)
		{
			char *subfilter = expr_to_ldap_filter((Node *) lfirst(lc), root, baserel);
			if (!subfilter)
				return NULL; /* Wenn ein Teil nicht gepusht werden kann, brechen wir ab */
			appendStringInfoString(&buf, subfilter);
		}
		appendStringInfoChar(&buf, ')');
		return buf.data;
	}

	/* Wenn der Ausdruck zu komplex ist -> NULL zurückgeben (Postgres filtert dann selbst) */
	return NULL;
}

