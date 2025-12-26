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
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Transform length() function for paths - returns number of relationships in path */
int transform_path_length_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming path length() function");

    /* Already validated in caller that this is a path variable */
    ast_node *arg = func_call->args->items[0];
    cypher_identifier *id = (cypher_identifier*)arg;

    path_variable *path_var = get_path_variable(ctx, id->name);
    if (!path_var || !path_var->elements) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Cannot get length of path variable: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Count relationships in the path */
    /* Path length = number of relationships = (number of elements - 1) / 2 for node-rel-node pattern */
    int rel_count = 0;
    for (int i = 0; i < path_var->elements->count; i++) {
        if (path_var->elements->items[i]->type == AST_NODE_REL_PATTERN) {
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

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
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

    path_variable *path_var = get_path_variable(ctx, id->name);
    if (!path_var || !path_var->elements) {
        ctx->has_error = true;
        ctx->error_message = strdup("Cannot get nodes from path variable");
        return -1;
    }

    /* Build JSON array of node IDs */
    append_sql(ctx, "json_array(");
    bool first = true;
    for (int i = 0; i < path_var->elements->count; i++) {
        ast_node *element = path_var->elements->items[i];
        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            if (node->variable) {
                const char *node_alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                if (node_alias) {
                    if (!first) append_sql(ctx, ", ");
                    append_sql(ctx, "%s.id", node_alias);
                    first = false;
                }
            }
        }
    }
    append_sql(ctx, ")");

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

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
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

    path_variable *path_var = get_path_variable(ctx, id->name);
    if (!path_var || !path_var->elements) {
        ctx->has_error = true;
        ctx->error_message = strdup("Cannot get relationships from path variable");
        return -1;
    }

    /* Build JSON array of relationship IDs */
    append_sql(ctx, "json_array(");
    bool first = true;
    for (int i = 0; i < path_var->elements->count; i++) {
        ast_node *element = path_var->elements->items[i];
        if (element->type == AST_NODE_REL_PATTERN) {
            cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
            if (rel->variable) {
                const char *rel_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                if (rel_alias) {
                    if (!first) append_sql(ctx, ", ");
                    append_sql(ctx, "%s.id", rel_alias);
                    first = false;
                }
            }
        }
    }
    append_sql(ctx, ")");

    return 0;
}

/* Transform startNode() function - returns start node of a relationship */
int transform_startnode_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming startNode() function");

    /* startNode() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("startNode() function requires exactly one argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("startNode() function argument must be a relationship variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in startNode() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* startNode() only works on relationships */
    if (!transform_var_is_edge(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        ctx->error_message = strdup("startNode() function argument must be a relationship variable");
        return -1;
    }

    /* Return the source_id from the edges table */
    append_sql(ctx, "(SELECT source_id FROM edges WHERE id = %s.id)", alias);

    return 0;
}

/* Transform endNode() function - returns end node of a relationship */
int transform_endnode_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming endNode() function");

    /* endNode() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("endNode() function requires exactly one argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("endNode() function argument must be a relationship variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in endNode() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* endNode() only works on relationships */
    if (!transform_var_is_edge(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        ctx->error_message = strdup("endNode() function argument must be a relationship variable");
        return -1;
    }

    /* Return the target_id from the edges table */
    append_sql(ctx, "(SELECT target_id FROM edges WHERE id = %s.id)", alias);

    return 0;
}
