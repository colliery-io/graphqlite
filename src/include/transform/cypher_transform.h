#ifndef CYPHER_TRANSFORM_H
#define CYPHER_TRANSFORM_H

#include "graphqlite_sqlite.h"
#include "parser/cypher_ast.h"
#include "transform/transform_variables.h"
#include "transform/sql_builder.h"

/* Forward declarations */
typedef struct cypher_transform_context cypher_transform_context;
typedef struct cypher_query_result cypher_query_result;

/* Path types for shortest path support */
typedef enum {
    TRANSFORM_PATH_NORMAL,          /* Regular path matching */
    TRANSFORM_PATH_SHORTEST,        /* shortestPath() - single shortest path */
    TRANSFORM_PATH_ALL_SHORTEST     /* allShortestPaths() - all paths of minimum length */
} transform_path_type;

/* Transform context - tracks state during AST transformation */
struct cypher_transform_context {
    sqlite3 *db;                    /* SQLite database connection */

    /* Unified variable tracking (includes path variables) */
    transform_var_context *var_ctx;

    /* SQL generation */
    char *sql_buffer;               /* Generated SQL query */
    size_t sql_size;
    size_t sql_capacity;

    /* CTE count for generating unique CTE names */
    int cte_count;

    /* Parameter tracking for parameterized queries */
    char **param_names;             /* Parameter names in order of appearance */
    int param_count;
    int param_capacity;

    /* Error handling */
    bool has_error;
    char *error_message;
    
    /* Context flags */
    bool in_comparison;             /* True when transforming expressions in comparison context */
    bool in_union;                  /* True when transforming UNION branches (skip buffer reset) */
    bool emit_hydrated_path;        /* True when a path expression must emit the full
                                     * {nodes,rels} JSON object inline (e.g. as a list
                                     * element under UNWIND) rather than elem_ids for
                                     * executor post-hydration. GQLITE-T-0340 sub-C. */
    
    /* Unique alias counters */
    int global_alias_counter;       /* Global counter for all unnamed entities (like AGE) */
    int with_cte_counter;           /* Counter for WITH CTE names (_with_N) */
    int unwind_cte_counter;         /* Counter for UNWIND CTE names (_unwind_N) */
    int reduce_counter;             /* Counter for REDUCE CTE names (_reduce_N) */
    int prop_join_counter;          /* Counter for property JOIN aliases */
    int quantifier_counter;         /* Counter for list-predicate json_each aliases (_je_N) */
    int anon_node_counter;          /* Cumulative counter for anonymous nodes */
    int anon_node_base;             /* Base offset for the current pattern's anon nodes */

    /* Pending property JOINs buffer (accumulated during RETURN transform) */
    char *pending_prop_joins;
    size_t pending_prop_joins_len;
    size_t pending_prop_joins_cap;

    /* I-0047 P3: bound-rel endpoint constraint for an OPTIONAL MATCH, stashed
     * by the rel handler and flushed onto the last LEFT JOIN's ON after the
     * path loop (when the fresh target node's join exists). Emitting it inline
     * as WHERE would filter the preserved anchor row (Match7 [4],
     * MatchWhere6 [5]: a bound rel reused in the reverse direction). Owned. */
    char *pending_optional_on;

    /* Query type tracking */
    enum {
        QUERY_TYPE_UNKNOWN,
        QUERY_TYPE_READ,            /* MATCH, RETURN */
        QUERY_TYPE_WRITE,           /* CREATE, SET, DELETE */
        QUERY_TYPE_MIXED            /* Both read and write */
    } query_type;

    /* Multi-graph support: current graph for MATCH clause processing */
    const char *current_graph;      /* Active graph name (borrowed pointer, not owned) */

    /* Unified SQL builder for clause-based SQL generation */
    sql_builder *unified_builder;

    /* T-0310: byte length of the CTE prefix that prepend_cte_to_sql
     * wrote at the start of sql_buffer. Zero if no CTE prefix was
     * prepended. Used by cypher_transform_query to know where the
     * SELECT body starts when splitting DML out of raw_output —
     * the DML half needs the same CTE prefix to resolve any
     * CTE-bound variable references. */
    size_t cte_prefix_len;

    /* T-0320 helpers (defined in cypher_transform.c). */
    /* T-0320: OPTIONAL MATCH defer-pair tracking for WHERE rewrite.
     * Each entry records (edge_alias, deferred_node_alias,
     * deferred_endpoint_col). At WHERE-handling time, the WHERE
     * SQL is scanned for references to <deferred_node_alias>.id
     * and a rewritten copy with the alias replaced by
     * <edge_alias>.<endpoint_col> is appended to the edge JOIN's
     * ON clause. This pushes the WHERE filter PRE-LEFT-JOIN so
     * non-matching inner rows are excluded rather than producing
     * outer×candidate cartesian rows. */
    struct {
        char *edge_alias;          /* owned */
        char *deferred_alias;      /* owned */
        char *endpoint_col;        /* "source_id" or "target_id" — owned */
    } *optional_defer_pairs;
    int optional_defer_pairs_count;
    int optional_defer_pairs_capacity;
};

/* Result structure for executed queries */
struct cypher_query_result {
    /* Result data */
    sqlite3_stmt *stmt;             /* Prepared statement (for reads) */
    int rows_affected;              /* For write operations */

    /* T-0310: DML to exec BEFORE stepping `stmt`. Owned string. Set
     * when transform_set/delete/remove emitted into raw_output AND
     * the query has a trailing read. Executor runs sqlite3_exec on
     * this then steps stmt. cypher_free_result frees this. */
    char *pre_exec_dml;

    /* Column information */
    char **column_names;
    int column_count;

    /* Error information */
    bool has_error;
    char *error_message;
};

/* Transform context management */
cypher_transform_context* cypher_transform_create_context(sqlite3 *db);
void cypher_transform_free_context(cypher_transform_context *ctx);

/* T-0320 helpers — record/clear OPTIONAL-MATCH defer pairs. */
void cypher_transform_record_defer_pair(cypher_transform_context *ctx,
                                        const char *edge_alias,
                                        const char *deferred_alias,
                                        const char *endpoint_col);
void cypher_transform_clear_defer_pairs(cypher_transform_context *ctx);

/* Main transform entry point */
cypher_query_result* cypher_transform_query(cypher_transform_context *ctx, cypher_query *query);

/* Generate SQL only (for EXPLAIN) - returns 0 on success, -1 on error */
int cypher_transform_generate_sql(cypher_transform_context *ctx, cypher_query *query);

/* Individual clause transformers */
int transform_match_clause(cypher_transform_context *ctx, cypher_match *match);
int transform_create_clause(cypher_transform_context *ctx, cypher_create *create);
int transform_set_clause(cypher_transform_context *ctx, cypher_set *set);
int transform_delete_clause(cypher_transform_context *ctx, cypher_delete *delete_clause);
int transform_remove_clause(cypher_transform_context *ctx, cypher_remove *remove);
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret);
int transform_with_clause(cypher_transform_context *ctx, cypher_with *with);
int transform_unwind_clause(cypher_transform_context *ctx, cypher_unwind *unwind);
int transform_foreach_clause(cypher_transform_context *ctx, cypher_foreach *foreach);
int transform_load_csv_clause(cypher_transform_context *ctx, cypher_load_csv *load_csv);
int transform_where_clause(cypher_transform_context *ctx, ast_node *where);

/* Expression transformers */
int transform_expression(cypher_transform_context *ctx, ast_node *expr);
int transform_property_access(cypher_transform_context *ctx, cypher_property *prop);
int transform_label_expression(cypher_transform_context *ctx, cypher_label_expr *label_expr);
int transform_not_expression(cypher_transform_context *ctx, cypher_not_expr *not_expr);
int transform_null_check(cypher_transform_context *ctx, cypher_null_check *null_check);
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op);
int transform_exists_expression(cypher_transform_context *ctx, cypher_exists_expr *exists_expr);
int transform_function_call(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_type_function(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_count_function(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_aggregate_function(cypher_transform_context *ctx, cypher_function_call *func_call);

/* Alias generation */
char* get_next_default_alias(cypher_transform_context *ctx);

/* Path variable registration (uses unified transform_var system) */
int register_path_variable(cypher_transform_context *ctx, const char *name, cypher_path *path);

/* SQL generation helpers — see docs/internal/sql-migration-inventory.md
 * for the I-0039 migration story.
 *
 * Two valid use cases:
 *
 *   1. **Internal expression scratchpad** (transform_expression and
 *      its dispatched function transforms). These write into
 *      ctx->sql_buffer as a scratch area that the calling code
 *      redirects via transform_expression_to_string() to capture the
 *      resulting expression SQL. This is the *intended* internal
 *      API. Code in transform_expression / transform_expr_*.c /
 *      transform_func_*.c uses these heavily and should keep doing
 *      so until the broader expression machinery is rewritten.
 *
 *   2. **Direct DML emission** (transform_set/delete/remove/create —
 *      already migrated as of I-0039 S5+S6). NEW code in this
 *      category must use sql_raw() against unified_builder instead.
 *
 * The deprecation marker steers (2) toward sql_builder while
 * still allowing (1) — the existing transform_expression internals
 * generate warnings but compile cleanly. The eventual rewrite of
 * transform_expression to a string-returning style is tracked
 * separately (see I-0039 Phase 5+). */
__attribute__((deprecated("DML use should go via sql_raw / sql_builder; expression-scratchpad use within transform_expression is OK — see docs/internal/sql-migration-inventory.md")))
void append_sql(cypher_transform_context *ctx, const char *format, ...);
/* append_identifier was unused as of 2026-05-20 — removed.
 * If you need to append a quoted SQL identifier, use
 *   sql_raw(ctx->unified_builder, "\"%s\"", name)
 * for DML emission, or compose into the expression scratchpad with
 *   append_sql(ctx, "\"%s\"", name)
 * (which routes through ctx->sql_buffer like the rest of the trio). */
__attribute__((deprecated("DML use should go via sql_raw / sql_builder; expression-scratchpad use within transform_expression is OK — see docs/internal/sql-migration-inventory.md")))
void append_string_literal(cypher_transform_context *ctx, const char *value);

/* Capture transform_expression output into a malloc'd string instead
 * of writing into ctx->sql_buffer. Swaps sql_buffer around the call so
 * the expression's append_sql writes go into a temporary buffer that
 * the caller takes ownership of. Use this in DML-emitting transforms
 * (transform_set.c value expressions, etc.) that need to splice the
 * expression SQL into a sql_raw() emission without polluting
 * sql_buffer. Returns NULL on error; sql_buffer is restored either way.
 * I-0039 transitional helper. */
char *cypher_transform_capture_expression(cypher_transform_context *ctx, ast_node *expr);

/* I-0043 X1: new expression transform API.
 *
 * transform_expression_into() — transform `expr` and append the
 * resulting SQL into the caller-supplied `out` dynamic_buffer. Does
 * NOT touch ctx->sql_buffer (the legacy scratchpad). Returns 0 on
 * success, -1 on error.
 *
 * transform_expression_str() — transform `expr` and return the result
 * as a freshly-allocated NUL-terminated string. Caller frees. Returns
 * NULL on error.
 *
 * Both are transitional and currently delegate to the legacy
 * scratchpad path (sql_buffer swap, transform_expression, snapshot).
 * As individual AST cases are migrated under I-0043 X2.x they will be
 * routed to dynamic_buffer-native emitters; the old
 * transform_expression body will be deleted in X5. Callers that
 * want to use the new shape today can adopt these helpers without
 * waiting for the case-by-case migration. */
int transform_expression_into(cypher_transform_context *ctx,
                              ast_node *expr,
                              dynamic_buffer *out);
char *transform_expression_str(cypher_transform_context *ctx,
                               ast_node *expr);

/* Pending property joins for aggregation optimization */
void add_pending_prop_join(cypher_transform_context *ctx, const char *join_sql);
const char* get_pending_prop_joins(cypher_transform_context *ctx);
size_t get_pending_prop_joins_len(cypher_transform_context *ctx);
void reset_pending_prop_joins(cypher_transform_context *ctx);

/* Graph-aware table name helper - uses variable's associated graph */
void append_var_table(cypher_transform_context *ctx, const char *var_name, const char *table);

/* Get graph-prefixed table name using context's current_graph (for MATCH processing) */
/* Returns static buffer - use immediately or copy */
const char *get_graph_table(cypher_transform_context *ctx, const char *table);

/* Parameter tracking */
int register_parameter(cypher_transform_context *ctx, const char *name);

/* SQL builder finalization - assembles unified_builder into sql_buffer */
int finalize_sql_generation(cypher_transform_context *ctx);

/* Variable-length relationship SQL generation */
int generate_varlen_cte(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                       const char *source_alias, const char *target_alias,
                       const char *cte_name);
void prepend_cte_to_sql(cypher_transform_context *ctx);

/* Result management */
void cypher_free_result(cypher_query_result *result);
bool cypher_result_next(cypher_query_result *result);
const char* cypher_result_get_string(cypher_query_result *result, int column);
int cypher_result_get_int(cypher_query_result *result, int column);

#endif /* CYPHER_TRANSFORM_H */