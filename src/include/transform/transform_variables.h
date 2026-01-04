/*
 * Unified Variable Tracking System
 * Replaces dual variable/entity tracking with single consistent system
 */

#ifndef TRANSFORM_VARIABLES_H
#define TRANSFORM_VARIABLES_H

#include <stdbool.h>
#include "parser/cypher_ast.h"

/* Forward declarations */
typedef struct transform_var transform_var;
typedef struct transform_var_context transform_var_context;

/* Variable kinds - unified from VAR_TYPE_* and ENTITY_TYPE_* */
typedef enum {
    VAR_KIND_NODE,       /* Node/vertex variable */
    VAR_KIND_EDGE,       /* Edge/relationship variable */
    VAR_KIND_PATH,       /* Path variable */
    VAR_KIND_PROJECTED,  /* WITH-projected variable (value is direct) */
    VAR_KIND_AGGREGATED  /* Result of aggregation */
} var_kind;

/* Path types for shortest path support */
typedef enum {
    VAR_PATH_NORMAL,        /* Regular path matching */
    VAR_PATH_SHORTEST,      /* shortestPath() - single shortest path */
    VAR_PATH_ALL_SHORTEST   /* allShortestPaths() - all paths of minimum length */
} var_path_type;

/* Unified variable structure */
struct transform_var {
    char *name;              /* Cypher variable name (e.g., "n", "r") */
    char *table_alias;       /* SQL alias (e.g., "n_0", "e_1") */
    var_kind kind;           /* Variable kind */

    /* Scope tracking */
    int declared_in_clause;  /* Which clause index declared this */
    bool is_visible;         /* Currently in scope? */
    bool is_bound;           /* Has a value assigned? */

    /* Multi-graph support */
    char *graph;             /* Source graph name (NULL = default graph) */

    /* For nodes/edges - optional label info */
    char *label;             /* Primary label if known */

    /* For path variables */
    ast_list *path_elements; /* AST nodes in the path */
    var_path_type path_type; /* Type of path */
    char *cte_name;          /* CTE name for variable-length paths */

    /* For projected variables */
    char *source_expr;       /* Original expression (for WITH aliasing) */
};

/* Variable context - manages all variables during transformation */
struct transform_var_context {
    transform_var *vars;
    int count;
    int capacity;
    int current_clause;      /* Current clause index for scope tracking */
};

/* Context lifecycle */
transform_var_context *transform_var_ctx_create(void);
void transform_var_ctx_free(transform_var_context *ctx);
void transform_var_ctx_reset(transform_var_context *ctx);

/* Variable registration - returns 0 on success, -1 on error */
int transform_var_register(transform_var_context *ctx,
                          const char *name,
                          var_kind kind,
                          const char *table_alias);

int transform_var_register_node(transform_var_context *ctx,
                               const char *name,
                               const char *table_alias,
                               const char *label);

int transform_var_register_edge(transform_var_context *ctx,
                               const char *name,
                               const char *table_alias,
                               const char *type);

int transform_var_register_path(transform_var_context *ctx,
                               const char *name,
                               const char *table_alias,
                               ast_list *elements,
                               var_path_type path_type);

int transform_var_register_projected(transform_var_context *ctx,
                                    const char *name,
                                    const char *source_expr);

/* Variable lookup - returns NULL if not found or not visible */
transform_var *transform_var_lookup(transform_var_context *ctx,
                                   const char *name);

/* Lookup by kind - returns NULL if not found, not visible, or wrong kind */
transform_var *transform_var_lookup_node(transform_var_context *ctx,
                                        const char *name);

transform_var *transform_var_lookup_edge(transform_var_context *ctx,
                                        const char *name);

transform_var *transform_var_lookup_path(transform_var_context *ctx,
                                        const char *name);

/* Convenience getters */
const char *transform_var_get_alias(transform_var_context *ctx,
                                   const char *name);

bool transform_var_is_edge(transform_var_context *ctx, const char *name);
bool transform_var_is_path(transform_var_context *ctx, const char *name);
bool transform_var_is_projected(transform_var_context *ctx, const char *name);
bool transform_var_is_bound(transform_var_context *ctx, const char *name);

/* Scope management */
void transform_var_enter_clause(transform_var_context *ctx);
void transform_var_exit_clause(transform_var_context *ctx);

/* Mark all current clause variables as inherited (for next clause) */
void transform_var_mark_inherited(transform_var_context *ctx);

/* Project specific variables for WITH clause - hides non-projected */
int transform_var_project(transform_var_context *ctx,
                         const char **names,
                         int count);

/* Set CTE name for path variable */
int transform_var_set_cte(transform_var_context *ctx,
                         const char *name,
                         const char *cte_name);

/* Set bound status */
int transform_var_set_bound(transform_var_context *ctx,
                           const char *name,
                           bool is_bound);

/* Set graph for multi-graph support */
int transform_var_set_graph(transform_var_context *ctx,
                           const char *name,
                           const char *graph);

/* Get graph for variable (returns NULL if not set or variable not found) */
const char *transform_var_get_graph(transform_var_context *ctx,
                                   const char *name);

/* Iteration helpers */
int transform_var_count(transform_var_context *ctx);
transform_var *transform_var_at(transform_var_context *ctx, int index);

/* Truncate to saved count (for pattern comprehension save/restore) */
void transform_var_truncate_to(transform_var_context *ctx, int count);

/* Debug helper */
void transform_var_dump(transform_var_context *ctx);

#endif /* TRANSFORM_VARIABLES_H */
