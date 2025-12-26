/*
 * transform_func_aggregate.c
 *    Aggregate function transformations for Cypher queries
 *
 * This file contains transformations for aggregate functions:
 * - count() - count rows or expressions
 * - sum(), avg(), min(), max() - numeric aggregations
 * - type() - get relationship type
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Forward declaration for pending property joins (defined in transform_return.c) */
extern void add_pending_prop_join(const char *join_sql);

/* Transform COUNT function */
int transform_count_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming COUNT function");

    /* COUNT() with no arguments - treat as COUNT(*) */
    if (!func_call->args || func_call->args->count == 0) {
        append_sql(ctx, "COUNT(*)");
        return 0;
    }

    /* COUNT(*) case - represented as single NULL argument */
    if (func_call->args->count == 1 && func_call->args->items[0] == NULL) {
        append_sql(ctx, "COUNT(*)");
        return 0;
    }

    /* COUNT(expression) case */
    if (func_call->args->count == 1 && func_call->args->items[0] != NULL) {
        if (func_call->distinct) {
            append_sql(ctx, "COUNT(DISTINCT ");
        } else {
            append_sql(ctx, "COUNT(");
        }

        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }

        append_sql(ctx, ")");
        return 0;
    }

    /* Invalid COUNT usage */
    ctx->has_error = true;
    ctx->error_message = strdup("COUNT function accepts 0 or 1 argument");
    return -1;
}

/* Transform other aggregate functions (MIN, MAX, AVG, SUM) */
int transform_aggregate_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming aggregate function: %s", func_call->function_name);

    /* These functions require exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s function requires exactly one non-null argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Check if argument is a property access - optimize with JOINs instead of correlated subqueries */
    ast_node *arg = func_call->args->items[0];
    if (arg->type == AST_NODE_PROPERTY) {
        cypher_property *prop = (cypher_property *)arg;
        return transform_aggregate_with_property(ctx, func_call, prop);
    }

    /* Generate SQL function call - convert to uppercase for SQL compliance */
    char upper_func[64];
    strncpy(upper_func, func_call->function_name, sizeof(upper_func) - 1);
    upper_func[sizeof(upper_func) - 1] = '\0';
    for (int i = 0; upper_func[i]; i++) {
        upper_func[i] = toupper(upper_func[i]);
    }

    if (func_call->distinct) {
        append_sql(ctx, "%s(DISTINCT ", upper_func);
    } else {
        append_sql(ctx, "%s(", upper_func);
    }

    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }

    append_sql(ctx, ")");
    return 0;
}

/*
 * Optimized aggregation on property access using JOINs instead of correlated subqueries.
 * Instead of generating MIN((SELECT COALESCE(...))) which runs 4 subqueries per row,
 * we add LEFT JOINs to property tables and aggregate the joined columns directly.
 */
int transform_aggregate_with_property(cypher_transform_context *ctx,
                                      cypher_function_call *func_call,
                                      cypher_property *prop)
{
    CYPHER_DEBUG("Optimizing aggregate %s on property access", func_call->function_name);

    /* Get the base expression (should be an identifier) */
    if (prop->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("Complex property access not yet supported in aggregations");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier *)prop->expr;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in aggregation: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Check if this is a projected variable from WITH */
    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
    const char *node_id_ref = is_projected ? alias : NULL;
    char node_id_buf[256];
    if (!is_projected) {
        snprintf(node_id_buf, sizeof(node_id_buf), "%s.id", alias);
        node_id_ref = node_id_buf;
    }

    /* Generate unique alias for property joins */
    static int prop_join_counter = 0;
    int join_id = ++prop_join_counter;

    /* Generate SQL function call - convert to uppercase */
    char upper_func[64];
    strncpy(upper_func, func_call->function_name, sizeof(upper_func) - 1);
    upper_func[sizeof(upper_func) - 1] = '\0';
    for (int i = 0; upper_func[i]; i++) {
        upper_func[i] = toupper(upper_func[i]);
    }

    /*
     * For MIN/MAX/AVG/SUM, we want numeric comparison.
     * Generate: AGG_FUNC(COALESCE(_prop_N_int.value, _prop_N_real.value, CAST(_prop_N_text.value AS REAL)))
     * And add LEFT JOINs to the property tables.
     */

    /* Add LEFT JOINs to property tables via sql_builder if active, otherwise inline */
    char join_alias_int[64], join_alias_real[64], join_alias_text[64];
    snprintf(join_alias_int, sizeof(join_alias_int), "_prop_%d_int", join_id);
    snprintf(join_alias_real, sizeof(join_alias_real), "_prop_%d_real", join_id);
    snprintf(join_alias_text, sizeof(join_alias_text), "_prop_%d_text", join_id);

    /* Build property key subquery once */
    char pk_subquery[256];
    snprintf(pk_subquery, sizeof(pk_subquery),
             "(SELECT id FROM property_keys WHERE key = '%s')", prop->property_name);

    /*
     * When using sql_builder, accumulate property JOINs in the pending buffer.
     * These will be injected into the FROM clause by transform_return_clause.
     */
    if (ctx->sql_builder.using_builder) {
        /* Build the JOIN clauses and add to pending buffer */
        char join_sql[2048];
        snprintf(join_sql, sizeof(join_sql),
                 " LEFT JOIN node_props_int AS %s ON %s.node_id = %s AND %s.key_id = %s"
                 " LEFT JOIN node_props_real AS %s ON %s.node_id = %s AND %s.key_id = %s"
                 " LEFT JOIN node_props_text AS %s ON %s.node_id = %s AND %s.key_id = %s",
                 join_alias_int, join_alias_int, node_id_ref, join_alias_int, pk_subquery,
                 join_alias_real, join_alias_real, node_id_ref, join_alias_real, pk_subquery,
                 join_alias_text, join_alias_text, node_id_ref, join_alias_text, pk_subquery);

        add_pending_prop_join(join_sql);
        CYPHER_DEBUG("Added pending property JOINs for %s aggregation", upper_func);

        /* Generate the aggregation expression using joined columns */
        if (func_call->distinct) {
            append_sql(ctx, "%s(DISTINCT COALESCE(%s.value, %s.value, CAST(%s.value AS REAL)))",
                       upper_func, join_alias_int, join_alias_real, join_alias_text);
        } else {
            append_sql(ctx, "%s(COALESCE(%s.value, %s.value, CAST(%s.value AS REAL)))",
                       upper_func, join_alias_int, join_alias_real, join_alias_text);
        }
    } else {
        /*
         * sql_builder not active - need to use subquery approach but optimized.
         * Generate a single subquery that does JOIN + aggregation instead of correlated subqueries per row.
         * Pattern: (SELECT AGG_FUNC(COALESCE(...)) FROM matched_nodes LEFT JOIN props...)
         *
         * This is tricky because we need to replicate the FROM clause context.
         * For now, fall back to the original correlated subquery approach but preserve numeric types.
         */
        CYPHER_DEBUG("sql_builder not active, using optimized correlated subquery");

        if (func_call->distinct) {
            append_sql(ctx, "%s(DISTINCT ", upper_func);
        } else {
            append_sql(ctx, "%s(", upper_func);
        }

        /* Generate COALESCE that preserves numeric types instead of casting to TEXT */
        append_sql(ctx, "COALESCE(");

        /* Integer properties (preferred for numeric ops) */
        append_sql(ctx, "(SELECT npi.value FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s AND pk.key = ",
                   node_id_ref);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");

        /* Real properties */
        append_sql(ctx, "(SELECT npr.value FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s AND pk.key = ",
                   node_id_ref);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");

        /* Text properties (cast to REAL for numeric aggregation) */
        append_sql(ctx, "(SELECT CAST(npt.value AS REAL) FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s AND pk.key = ",
                   node_id_ref);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, ")");

        append_sql(ctx, "))");
    }

    return 0;
}

/* Transform TYPE function specifically */
int transform_type_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming TYPE function");

    /* TYPE function requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("type() function requires exactly one non-null argument");
        return -1;
    }

    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("type() function argument must be a relationship variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;

    /* Check if the variable is registered */
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in type() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Check if the variable is a relationship/edge variable */
    if (!transform_var_is_edge(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        ctx->error_message = strdup("type() function argument must be a relationship variable");
        return -1;
    }

    /* Generate SQL to extract the relationship type from the edges table */
    /* The relationship alias should point to the edges table with an 'id' column */
    /* We extract the 'type' column which contains the relationship type */
    append_sql(ctx, "(SELECT type FROM edges WHERE id = %s.id)", alias);

    return 0;
}
