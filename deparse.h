#ifndef __deparse__
#define __deparse__

#include "postgres.h"

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "catalog/pg_aggregate.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#if PG_VERSION_NUM >= 160000
	#include "catalog/pg_ts_config.h"
#endif
#include "catalog/pg_ts_dict.h"
#if (PG_VERSION_NUM < 130000)
	#include "catalog/pg_type.h"
#endif
#include "commands/defrem.h"
#include "mb/pg_wchar.h"
#include "nodes/nodeFuncs.h"
#include "nodes/plannodes.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#if (PG_VERSION_NUM >= 120000)
#include "nodes/pathnodes.h"
#include "access/table.h"
#include "utils/float.h"
#include "optimizer/optimizer.h"
#else
#include "nodes/relation.h"
#include "optimizer/var.h"
#endif

#include "funcapi.h"
#include "fmgr.h"
#include "foreign/foreign.h"
#include "lib/stringinfo.h"
#include "utils/rel.h"
#include "funcapi.h"

#define QUOTE '"'

bool ldap_fdw_is_foreign_expr(PlannerInfo *, RelOptInfo *, Expr *, bool);
void ldap2_fdw_log_nodeTags();

void
deparseDeleteSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *returningList,
				 List **retrieved_attrs);

#endif
