/*
 * transform_func_entity.c
 *    Entity introspection function transformations for Cypher queries
 *
 * This file contains transformations for entity introspection functions:
 * - id() - returns internal ID of node or relationship
 * - labels() - returns list of labels for a node
 * - properties() - returns property map for node or relationship
 * - keys() - returns list of property keys
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Transform id() function - returns internal ID of node or relationship */
int transform_id_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming id() function");

    /* id() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("id() function requires exactly one argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("id() function argument must be a node or relationship variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in id() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Check if it's a projected variable (from WITH) */
    if (transform_var_is_projected(ctx->var_ctx, id->name)) {
        /* Projected variable already contains the ID value */
        append_sql(ctx, "%s", alias);
    } else {
        /* Node or edge variable - access .id field */
        append_sql(ctx, "%s.id", alias);
    }

    return 0;
}

/* Transform labels() function - returns list of labels for a node */
int transform_labels_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming labels() function");

    /* labels() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("labels() function requires exactly one argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("labels() function argument must be a node variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in labels() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* labels() only works on nodes, not relationships */
    if (transform_var_is_edge(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        ctx->error_message = strdup("labels() function argument must be a node variable, not a relationship");
        return -1;
    }

    /* Generate SQL to get labels as JSON array */
    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
    append_sql(ctx, "(SELECT json_group_array(label) FROM node_labels WHERE node_id = %s%s)",
               alias, is_projected ? "" : ".id");

    return 0;
}

/* Transform properties() function - returns property map for node or relationship */
int transform_properties_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming properties() function");

    /* properties() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("properties() function requires exactly one argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("properties() function argument must be a node or relationship variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in properties() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
    bool is_edge = transform_var_is_edge(ctx->var_ctx, id->name);
    const char *id_suffix = is_projected ? "" : ".id";

    if (is_edge) {
        /* For edges, query edge property tables */
        append_sql(ctx, "(SELECT json_group_object(pk.key, COALESCE("
            "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = %s%s AND ept.key_id = pk.id), "
            "(SELECT epi.value FROM edge_props_int epi WHERE epi.edge_id = %s%s AND epi.key_id = pk.id), "
            "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = %s%s AND epr.key_id = pk.id), "
            "(SELECT epb.value FROM edge_props_bool epb WHERE epb.edge_id = %s%s AND epb.key_id = pk.id))) "
            "FROM property_keys pk WHERE EXISTS ("
            "SELECT 1 FROM edge_props_text WHERE edge_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM edge_props_int WHERE edge_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM edge_props_real WHERE edge_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM edge_props_bool WHERE edge_id = %s%s AND key_id = pk.id))",
            alias, id_suffix, alias, id_suffix, alias, id_suffix, alias, id_suffix,
            alias, id_suffix, alias, id_suffix, alias, id_suffix, alias, id_suffix);
    } else {
        /* For nodes, query node property tables */
        append_sql(ctx, "(SELECT json_group_object(pk.key, COALESCE("
            "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %s%s AND npt.key_id = pk.id), "
            "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %s%s AND npi.key_id = pk.id), "
            "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %s%s AND npr.key_id = pk.id), "
            "(SELECT npb.value FROM node_props_bool npb WHERE npb.node_id = %s%s AND npb.key_id = pk.id))) "
            "FROM property_keys pk WHERE EXISTS ("
            "SELECT 1 FROM node_props_text WHERE node_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM node_props_int WHERE node_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM node_props_real WHERE node_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM node_props_bool WHERE node_id = %s%s AND key_id = pk.id))",
            alias, id_suffix, alias, id_suffix, alias, id_suffix, alias, id_suffix,
            alias, id_suffix, alias, id_suffix, alias, id_suffix, alias, id_suffix);
    }

    return 0;
}

/* Transform keys() function - returns list of property keys for node or relationship */
int transform_keys_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming keys() function");

    /* keys() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("keys() function requires exactly one argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("keys() function argument must be a node or relationship variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in keys() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
    bool is_edge = transform_var_is_edge(ctx->var_ctx, id->name);
    const char *id_suffix = is_projected ? "" : ".id";

    if (is_edge) {
        /* For edges, query edge property tables */
        append_sql(ctx, "(SELECT json_group_array(pk.key) FROM property_keys pk WHERE EXISTS ("
            "SELECT 1 FROM edge_props_text WHERE edge_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM edge_props_int WHERE edge_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM edge_props_real WHERE edge_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM edge_props_bool WHERE edge_id = %s%s AND key_id = pk.id))",
            alias, id_suffix, alias, id_suffix, alias, id_suffix, alias, id_suffix);
    } else {
        /* For nodes, query node property tables */
        append_sql(ctx, "(SELECT json_group_array(pk.key) FROM property_keys pk WHERE EXISTS ("
            "SELECT 1 FROM node_props_text WHERE node_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM node_props_int WHERE node_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM node_props_real WHERE node_id = %s%s AND key_id = pk.id UNION ALL "
            "SELECT 1 FROM node_props_bool WHERE node_id = %s%s AND key_id = pk.id))",
            alias, id_suffix, alias, id_suffix, alias, id_suffix, alias, id_suffix);
    }

    return 0;
}
