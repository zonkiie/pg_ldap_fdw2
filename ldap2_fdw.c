/*
 * Include all neccessary headers
 */

#include "postgres.h"
#include "access/sysattr.h"
#include "nodes/pg_list.h"
#include "nodes/makefuncs.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_type.h" 
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "commands/explain.h"
#include "optimizer/paths.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "parser/parsetree.h"
#include "optimizer/restrictinfo.h"

PG_MODULE_MAGIC;

