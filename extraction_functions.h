#ifndef __extraction_functions__
#define __extraction_functions__

#include "postgres.h"
#include "nodes/pathnodes.h"
#include "nodes/pg_list.h"
#include "nodes/primnodes.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/array.h"

Var *get_base_var(Node *node);
char * const_to_string(Const *c);
char * expr_to_ldap_filter(Node *node, PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);


#endif
