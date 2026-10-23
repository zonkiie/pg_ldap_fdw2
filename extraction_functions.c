#include "extraction_functions.h"

// Gemini-created funktions from here to EOF

#include "utils/array.h"
#include "utils/lsyscache.h"

char *
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
Var *
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
char *
expr_to_ldap_filter(Node *node, PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	if (node == NULL)
		return NULL;

	/* Handle RestrictInfo Wrapper */
	if (IsA(node, RestrictInfo))
	{
		return expr_to_ldap_filter((Node *) ((RestrictInfo *) node)->clause, root, baserel, foreigntableid);
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
			char *attr_name = get_attname(foreigntableid, attnum, false);
			
			if (!attr_name)
				return NULL;

			Const *cnt = (Const *) right;
			if (!cnt->constisnull)
			{
				/* Prüfen, ob es sich um einen Array-Typ handelt */
				Oid element_type = get_element_type(cnt->consttype);

				if (OidIsValid(element_type))
				{
					/* --- ARRAY-BEHANDLUNG --- */
					ArrayType *arrayval = DatumGetArrayTypeP(cnt->constvalue);
					Datum *elems;
					bool *nulls;
					int nelems;
					int16 elmlen;
					bool elmbyval;
					char elmalign;
					Oid typoutput;
					bool typisvarlena;

					/* Metadaten des Elementtyps ermitteln */
					get_typlenbyvalalign(element_type, &elmlen, &elmbyval, &elmalign);
					getTypeOutputInfo(element_type, &typoutput, &typisvarlena);

					/* Array in Einzelwerte zerlegen */
					deconstruct_array(arrayval, element_type, elmlen, elmbyval, elmalign,
									&elems, &nulls, &nelems);

					if (nelems == 0)
						return psprintf("(%s=__no_match__)", attr_name);

					if (nelems == 1)
					{
						if (nulls[0])
							return NULL;
						char *val_str = OidOutputFunctionCall(typoutput, elems[0]);
						char *retval = NULL;
						struct berval out_val;
						struct berval in_val;
						in_val.bv_val = val_str;
						in_val.bv_len = strlen(val_str);
						if (ldap_bv2escaped_filter_value(&in_val, &out_val) == 0) {
							
							retval = psprintf("(%s=%s)", attr_name, out_val.bv_val);
							ldap_memfree(out_val.bv_val);
						}
						//return psprintf("(%s=%s)", attr_name, val_str);
						return retval;
					}

					/* Mehrere Elemente -> Als LDAP-OR verknüpfen: (|(attr=val1)(attr=val2)) */
					StringInfoData or_buf;
					initStringInfo(&or_buf);
					appendStringInfo(&or_buf, "(|");

					for (int i = 0; i < nelems; i++)
					{
						if (nulls[i])
							continue;
						char *val_str = OidOutputFunctionCall(typoutput, elems[i]);
						
						struct berval out_val;
						struct berval in_val;
						in_val.bv_val = val_str;
						in_val.bv_len = strlen(val_str);

						if (ldap_bv2escaped_filter_value(&in_val, &out_val) == 0) {
							
							appendStringInfo(&or_buf, "(%s=%s)", attr_name, out_val.bv_val);
							ldap_memfree(out_val.bv_val);
						}
						
						//appendStringInfo(&or_buf, "(%s=%s)", attr_name, val_str);
					}
					appendStringInfoChar(&or_buf, ')');
					return or_buf.data;
				}
				else
				{
					/* --- SKALAR-BEHANDLUNG (z.B. cn = 'Max') --- */
					char *val_str = const_to_string(cnt);
					char *retval = NULL;
					struct berval out_val;
					struct berval in_val;
					in_val.bv_val = val_str;
					in_val.bv_len = strlen(val_str);
					if (ldap_bv2escaped_filter_value(&in_val, &out_val) == 0) {
						
						retval = psprintf("(%s=%s)", attr_name, out_val.bv_val);
						ldap_memfree(out_val.bv_val);
					}
					//return psprintf("(%s=%s)", attr_name, val_str);
					return retval;
					//return psprintf("(%s=%s)", attr_name, val_str);
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
			char *subfilter = expr_to_ldap_filter((Node *) lfirst(lc), root, baserel, foreigntableid);
			if (!subfilter)
				return NULL;
			appendStringInfoString(&buf, subfilter);
		}
		appendStringInfoChar(&buf, ')');
		return buf.data;
	}

	/* Wenn der Ausdruck zu komplex ist -> NULL zurückgeben */
	return NULL;
}
