/*
 * transform_func_path.c
 *    Path function transformations for Cypher queries
 *
 * This file contains transformations for path navigation functions:
 * - length() for paths - returns number of relationships
 * - nodes() - returns list of nodes in a path
 * - relationships() - returns list of relationships in a path
 * - startNode() - returns start node of a relationship
 * - endNode() - returns end node of a relationship
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "entity_json_sql.h"
#include "transform/transform_func_path.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Transform length() function for paths - returns number of relationships in path */
int transform_path_length_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming path length() function");

    /* Already validated in caller that this is a path variable */
    ast_node *arg = func_call->args->items[0];
    cypher_identifier *id = (cypher_identifier*)arg;

    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
    if (!path_var || !path_var->path_elements) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Cannot get length of path variable: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Count relationships in the path */
    /* Path length = number of relationships = (number of elements - 1) / 2 for node-rel-node pattern */
    int rel_count = 0;
    for (int i = 0; i < path_var->path_elements->count; i++) {
        if (path_var->path_elements->items[i]->type == AST_NODE_REL_PATTERN) {
            rel_count++;
        }
    }

    append_sql(ctx, "%d", rel_count);
    return 0;
}

/* Transform nodes() function - returns list of nodes in a path */
int transform_path_nodes_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming nodes() function");

    /* nodes() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("nodes() function requires exactly one argument");
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* nodes(null) → null. */
    if (arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)arg;
        if (lit->literal_type == LITERAL_NULL) {
            append_sql(ctx, "NULL");
            return 0;
        }
        ctx->has_error = true;
        ctx->error_message = strdup("nodes() function argument must be a path variable");
        return -1;
    }

    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("nodes() function argument must be a path variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;

    /* Check if this is a path variable */
    if (!transform_var_is_path(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "nodes() function argument must be a path variable, got: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
    if (!path_var || !path_var->path_elements) {
        ctx->has_error = true;
        ctx->error_message = strdup("Cannot get nodes from path variable");
        return -1;
    }

    /* Collect node aliases in path order. */
    const char *node_aliases[64];
    int n_nodes = 0;
    for (int i = 0; i < path_var->path_elements->count && n_nodes < 64; i++) {
        ast_node *element = path_var->path_elements->items[i];
        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            if (node->variable) {
                const char *node_alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                if (node_alias) node_aliases[n_nodes++] = node_alias;
            }
        }
    }

    /* nodes() on a null path (OPTIONAL pattern that didn't match) is null,
     * not [null,...]. Guard on the first node alias's id (all-or-nothing). */
    if (n_nodes > 0) {
        append_sql(ctx, "(CASE WHEN %s.id IS NULL THEN NULL ELSE json_array(", node_aliases[0]);
        for (int i = 0; i < n_nodes; i++) {
            if (i > 0) append_sql(ctx, ", ");
            append_sql(ctx, "%s.id", node_aliases[i]);
        }
        append_sql(ctx, ") END)");
    } else {
        append_sql(ctx, "json_array()");
    }

    return 0;
}

/* Transform relationships() function - returns list of relationships in a path */
int transform_path_relationships_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming relationships() function");

    /* relationships() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("relationships() function requires exactly one argument");
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* relationships(null) → null. */
    if (arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)arg;
        if (lit->literal_type == LITERAL_NULL) {
            append_sql(ctx, "NULL");
            return 0;
        }
        ctx->has_error = true;
        ctx->error_message = strdup("relationships() function argument must be a path variable");
        return -1;
    }

    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("relationships() function argument must be a path variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;

    /* Check if this is a path variable */
    if (!transform_var_is_path(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "relationships() function argument must be a path variable, got: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
    if (!path_var || !path_var->path_elements) {
        ctx->has_error = true;
        ctx->error_message = strdup("Cannot get relationships from path variable");
        return -1;
    }

    /* Variable-length rel in the path: its edge ids live in the varlen CTE's
     * interleaved elem_ids (node,edge,node,... — edges at odd positions), not
     * in a single `.id` column. Build the rel-id list from there.
     * (Path2 [1]/[2].) */
    for (int i = 0; i < path_var->path_elements->count; i++) {
        ast_node *element = path_var->path_elements->items[i];
        if (element->type == AST_NODE_REL_PATTERN &&
            ((cypher_rel_pattern*)element)->varlen) {
            cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
            const char *alias = rel->variable ?
                transform_var_get_alias(ctx->var_ctx, rel->variable) : NULL;
            if (alias) {
                {
                    char *ej = gql_sql_edge_json_expr("", "e.id", "e.type", "e.source_id", "e.target_id");
                    if (!ej) return -1;
                    append_sql(ctx,
                        "(SELECT json_group_array(%s ORDER BY je.key) "
                        "FROM json_each('[' || %s.elem_ids || ']') je JOIN edges e ON e.id = je.value "
                        "WHERE (je.key %% 2) = 1)", ej, alias);
                    free(ej);
                }
                return 0;
            }
        }
    }

    /* Collect relationship aliases in path order. */
    const char *rel_aliases[64];
    int n_rels = 0;
    for (int i = 0; i < path_var->path_elements->count && n_rels < 64; i++) {
        ast_node *element = path_var->path_elements->items[i];
        if (element->type == AST_NODE_REL_PATTERN) {
            cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
            if (rel->variable) {
                const char *rel_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                if (rel_alias) rel_aliases[n_rels++] = rel_alias;
            }
        }
    }

    /* relationships() on a null path (e.g. an OPTIONAL pattern that didn't
     * match) is null, not [null]. Guard on the first rel alias's id being
     * NULL — an OPTIONAL path matches all-or-nothing. (Path2 [3].) */
    if (n_rels > 0) {
        append_sql(ctx, "(CASE WHEN %s.id IS NULL THEN NULL ELSE json_array(", rel_aliases[0]);
        for (int i = 0; i < n_rels; i++) {
            if (i > 0) append_sql(ctx, ", ");
            append_sql(ctx, "%s.id", rel_aliases[i]);
        }
        append_sql(ctx, ") END)");
    } else {
        append_sql(ctx, "json_array()");
    }

    return 0;
}

/* GQLITE-T-0181: shared validation for startNode()/endNode(). Emits the SQL
 * expression that yields the endpoint node id (source_id or target_id of the
 * relationship bound to the argument), or NULL for a null argument / an
 * OPTIONAL MATCH miss. Returns 1 when an id expression was emitted, 0 when a
 * literal NULL was emitted, -1 on error. */
static int emit_endpoint_id(cypher_transform_context *ctx, cypher_function_call *func_call,
                            const char *fname, const char *column)
{
    char err[256];

    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function requires exactly one argument", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* startNode(null) / endNode(null) → null. */
    if (arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)arg;
        if (lit->literal_type == LITERAL_NULL) {
            append_sql(ctx, "NULL");
            return 0;
        }
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function argument must be a relationship variable", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function argument must be a relationship variable", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    cypher_identifier *id = (cypher_identifier *)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "Unknown variable in %s() function: %s", fname, id->name);
        ctx->error_message = strdup(err);
        return -1;
    }

    /* Only relationships have endpoints. */
    if (!transform_var_is_edge(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function argument must be a relationship variable", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    /* A post-WITH / projected edge alias IS the id value; a MATCH-bound edge
     * alias is the edges row (same rule as type()). */
    bool skip_id = transform_var_is_projected(ctx->var_ctx, id->name) ||
                   transform_var_alias_is_id(ctx->var_ctx, id->name);
    append_sql(ctx, "(SELECT %s FROM edges WHERE id = %s%s)", column, alias, skip_id ? "" : ".id");
    return 1;
}

/* Emit the full node JSON ({"id","labels","properties"}) for the endpoint,
 * the same shape `RETURN n` produces (perf review F5 pass-through), guarded
 * so a NULL endpoint id (null argument, OPTIONAL MATCH miss) stays NULL. */
static int emit_endpoint_node(cypher_transform_context *ctx, cypher_function_call *func_call,
                              const char *fname, const char *column)
{
    /* Render the id expression into a scratch buffer so it can be spliced
     * into the node JSON template several times. */
    size_t saved_size = ctx->sql_size;
    int rc = emit_endpoint_id(ctx, func_call, fname, column);
    if (rc < 0) return -1;
    if (rc == 0) return 0; /* literal NULL already emitted */

    char *id_expr = strdup(ctx->sql_buffer + saved_size);
    if (!id_expr) return -1;
    ctx->sql_size = saved_size;
    ctx->sql_buffer[saved_size] = '\0';

    char *nj = gql_sql_node_json_expr("", id_expr);
    if (!nj) { free(id_expr); return -1; }
    append_sql(ctx, "(CASE WHEN %s IS NULL THEN NULL ELSE %s END)", id_expr, nj);
    free(nj);
    free(id_expr);
    return 0;
}

/* Transform startNode() function - returns the start node of a relationship
 * as a node object (GQLITE-T-0181; was the bare source_id integer). */
int transform_startnode_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming startNode() function");
    return emit_endpoint_node(ctx, func_call, "startNode", "source_id");
}

/* Transform endNode() function - returns the end node of a relationship
 * as a node object (GQLITE-T-0181; was the bare target_id integer). */
int transform_endnode_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming endNode() function");
    return emit_endpoint_node(ctx, func_call, "endNode", "target_id");
}

/* Emit only the endpoint node id for startNode(r)/endNode(r). Used where the
 * caller needs the id rather than the node object: property access
 * (`startNode(r).name`) and `id(startNode(r))`. Returns -1 if func_call is
 * not a startNode/endNode call or on a validation error. */
int transform_endpoint_node_id(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    if (!func_call || !func_call->function_name) return -1;
    if (strcasecmp(func_call->function_name, "startNode") == 0)
        return emit_endpoint_id(ctx, func_call, "startNode", "source_id") < 0 ? -1 : 0;
    if (strcasecmp(func_call->function_name, "endNode") == 0)
        return emit_endpoint_id(ctx, func_call, "endNode", "target_id") < 0 ? -1 : 0;
    ctx->has_error = true;
    ctx->error_message = strdup("Expected startNode() or endNode()");
    return -1;
}
