#ifndef CYPHER_EXECUTOR_H
#define CYPHER_EXECUTOR_H

#include "graphqlite_sqlite.h"
#include <stdbool.h>

#include "executor/cypher_schema.h"
#include "parser/cypher_parser.h"
#include "transform/cypher_transform.h"
#include "executor/agtype.h"

/* Forward declarations */
typedef struct cypher_executor cypher_executor;
struct cypher_transform_context;
struct cypher_return;

/* Execution result structure */
typedef struct cypher_result {
    bool success;
    char *error_message;
    
    /* Result data for queries that return data */
    int row_count;
    int column_count;
    char **column_names;
    char ***data; /* 2D array: data[row][column] - legacy format */
    int **data_types; /* 2D array: data_types[row][column] - SQLite type constants */
    
    /* AGType-compatible result data */
    agtype_value ***agtype_data; /* 2D array: agtype_data[row][column] */
    bool use_agtype; /* Whether to use agtype format */
    
    /* Statistics for modification queries */
    int nodes_created;
    int nodes_deleted;
    int relationships_created;
    int relationships_deleted;
    int properties_set;
} cypher_result;

/* Forward declaration for CSR graph (defined in graph_algorithms.h) */
struct csr_graph;

/* Execution engine - coordinates parser, transformer, and schema manager */
/* Perf review F8: one cached read query. The transform context is kept
 * because build_query_results() consults its variable context while
 * rendering rows; the AST is kept because the context points into it. */
typedef struct stmt_cache_entry {
    char *text;                          /* Cypher text, the cache key */
    ast_node *ast;                       /* owned parsed query */
    struct cypher_transform_context *ctx;/* owned transform state */
    sqlite3_stmt *stmt;                  /* owned prepared statement */
    struct cypher_return *ret;           /* RETURN clause, points into ast */
    unsigned long long last_used;        /* LRU tick */
    bool in_use;                         /* guards re-entrant use of stmt */
} stmt_cache_entry;

#define GQL_STMT_CACHE_MAX 64

struct cypher_executor {
    sqlite3 *db;
    cypher_schema_manager *schema_mgr;
    bool schema_initialized;
    const char *params_json;  /* Current query parameters (NULL if no params) */
    struct csr_graph *cached_graph;  /* Cached graph for algorithm acceleration (managed by connection) */
    struct csr_graph **cached_graph_slot; /* Owner's pointer, so a stale cache can be rebuilt in place */
    bool graph_dirty;                /* A write ran since cached_graph was loaded */

    /* Perf review F8: per-connection cache of pure read queries keyed by
     * Cypher text (parse + transform + prepare are ~90% of a point query).
     * Populated only by the two read paths that end in build_query_results
     * (MATCH+RETURN and the generic transform without pre-exec DML), via the
     * capture protocol below; every other path is unaffected. */
    stmt_cache_entry *stmt_cache;
    int stmt_cache_count;
    unsigned long long stmt_cache_tick;
    bool stmt_cache_enabled;             /* GQL_STMT_CACHE=0 disables (tests/diagnostics) */

    /* Capture protocol: cypher_executor_execute() arms stmt_capture for the
     * duration of one text-path execution; a read handler that would
     * otherwise finalize its statement parks it here instead, and the
     * entry point turns the parked state into a cache entry. */
    bool stmt_capture;
    struct cypher_transform_context *captured_ctx;
    sqlite3_stmt *captured_stmt;
    struct cypher_return *captured_ret;

    /* Indirection handed to the SQLITE_TRACE_CLOSE hook. The executor and
     * the connection can be destroyed in either order (tests free the
     * executor first; the extension frees it from the close path), so the
     * hook must never dereference a freed executor: freeing the executor
     * clears trace_ctx->ex, and the hook frees trace_ctx when it fires. */
    struct executor_trace_ctx *trace_ctx;
};

typedef struct executor_trace_ctx {
    cypher_executor *ex;
} executor_trace_ctx;

/* Finalize every cached statement (called from the SQLITE_TRACE_CLOSE hook
 * before SQLite's unfinalized-statement check, and from executor free). */
void cypher_executor_release_statements(cypher_executor *executor);

/* Executor lifecycle */
cypher_executor* cypher_executor_create(sqlite3 *db);
void cypher_executor_free(cypher_executor *executor);

/* Query execution */
cypher_result* cypher_executor_execute(cypher_executor *executor, const char *query);
cypher_result* cypher_executor_execute_params(cypher_executor *executor, const char *query, const char *params_json);
cypher_result* cypher_executor_execute_ast(cypher_executor *executor, ast_node *ast);
cypher_result* cypher_executor_execute_ast_params(cypher_executor *executor, ast_node *ast, const char *params_json);

/* Result management */
void cypher_result_free(cypher_result *result);
void cypher_result_print(cypher_result *result);

/* Utility functions */
bool cypher_executor_is_ready(cypher_executor *executor);
const char* cypher_executor_get_last_error(cypher_executor *executor);

#endif /* CYPHER_EXECUTOR_H */