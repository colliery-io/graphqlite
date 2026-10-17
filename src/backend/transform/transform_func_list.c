/*
 * transform_func_list.c
 *    List and utility function transformations for Cypher queries
 *
 * This file contains transformations for list and utility functions:
 * - head(), tail(), last() - list element access
 * - range() - generate list of integers
 * - collect() - aggregate into list
 * - coalesce() - return first non-null value
 * - toString() - convert to string
 * - toInteger(), toFloat(), toBoolean() - type conversion
 * - timestamp() - current Unix timestamp
 * - randomUUID() - generate UUID v4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"
#include "transform/transform_func_list.h"
#include "transform/transform_func_path.h"
#include "transform/transform_func_string.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"


/* Transform coalesce function: coalesce(expr1, expr2, ...) */
int transform_coalesce_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming coalesce function");

    if (!func_call->args || func_call->args->count < 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("coalesce() requires at least one argument");
        return -1;
    }

    append_sql(ctx, "COALESCE(");
    for (int i = 0; i < func_call->args->count; i++) {
        if (i > 0) {
            append_sql(ctx, ", ");
        }
        if (transform_expression(ctx, func_call->args->items[i]) < 0) {
            return -1;
        }
    }
    append_sql(ctx, ")");

    return 0;
}

/* Transform list functions: head(), tail(), last() */
int transform_list_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming list function: %s", func_call->function_name);

    /* Requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() function requires exactly one argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    if (strcasecmp(func_call->function_name, "head") == 0) {
        /* head(list) - get first element: json_extract(list, '$[0]') */
        append_sql(ctx, "json_extract(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", '$[0]')");
    } else if (strcasecmp(func_call->function_name, "last") == 0) {
        /* last(list) - get last element: json_extract(list, '$[#-1]') */
        append_sql(ctx, "json_extract(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", '$[#-1]')");
    } else if (strcasecmp(func_call->function_name, "tail") == 0) {
        /* tail(list) - all elements except first. json_remove keeps maps,
         * lists and entities as JSON; json_group_array over json_each.value
         * stored them as strings on SQLite before 3.46 (json_each gave no
         * JSON subtype there; Quantifier1-4 [8] on SQLite 3.45). */
        append_sql(ctx, "json_remove(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", '$[0]')");
    }

    return 0;
}

/* Transform range() function - generate list of integers */
int transform_range_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming range() function");

    /* range(start, end) or range(start, end, step) */
    if (!func_call->args || func_call->args->count < 2 || func_call->args->count > 3) {
        ctx->has_error = true;
        ctx->error_message = strdup("range() function requires 2 or 3 arguments: range(start, end) or range(start, end, step)");
        return -1;
    }

    /* Static validation: reject obviously-wrong literal arguments at
     * compile time. range() requires integer start/end/step; step must
     * not be zero. Non-literal arguments are passed through to the
     * recursive CTE below (best-effort runtime behavior). */
    for (int i = 0; i < func_call->args->count; i++) {
        ast_node *a = func_call->args->items[i];
        if (!a) continue;
        if (a->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)a;
            switch (lit->literal_type) {
                case LITERAL_INTEGER:
                    if (i == 2 && lit->value.integer == 0) {
                        ctx->has_error = true;
                        ctx->error_message = strdup(
                            "ArgumentError: NumberOutOfRange — range() step argument must not be zero");
                        return -1;
                    }
                    break;
                case LITERAL_DECIMAL:
                case LITERAL_STRING:
                case LITERAL_BOOLEAN:
                    ctx->has_error = true;
                    ctx->error_message = strdup(
                        "ArgumentError: InvalidArgumentType — range() arguments must be integers");
                    return -1;
                default:
                    break;
            }
        } else if (a->type == AST_NODE_LIST || a->type == AST_NODE_MAP) {
            ctx->has_error = true;
            ctx->error_message = strdup(
                "ArgumentError: InvalidArgumentType — range() arguments must be integers");
            return -1;
        }
    }

    /* Use a recursive CTE to generate the inclusive openCypher range.
     *
     * openCypher's range is INCLUSIVE on both ends; the iteration direction
     * follows the sign of step (default +1). `range(1, 0)` produces []
     * because there are no valid values; `range(5, 1, -1)` produces
     * [5,4,3,2,1]. We bake the step constant into the SQL twice (once for
     * initial guard, once for recursion bound) so the result is correct for
     * forward and backward ranges. */
    append_sql(ctx, "(WITH RECURSIVE _range(x) AS ("
                    "SELECT ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, " WHERE ((");
    /* step > 0 AND start <= end */
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") > 0 AND ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, " <= ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") OR ((");
    /* step < 0 AND start >= end */
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") < 0 AND ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, " >= ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ")"
                    " UNION ALL SELECT x + (");
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") FROM _range WHERE ((");
    /* recursion guard: (step > 0 AND x+step <= end) OR (step < 0 AND x+step >= end) */
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") > 0 AND x + (");
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") <= ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") OR ((");
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") < 0 AND x + (");
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, ") >= ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ")) SELECT COALESCE(json_group_array(x), json('[]')) FROM _range)");

    return 0;
}

/* Transform collect() aggregate function - aggregate values into list */
int transform_collect_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming collect() function");

    /* collect(expr) - aggregate into JSON array */
    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("collect() function requires exactly one argument");
        return -1;
    }

    /* collect() filters out NULL values per Cypher spec. SQLite's
     * FILTER clause does this cleanly without changing the aggregate. */
    append_sql(ctx, "json_group_array(");
    if (func_call->args->items[0] == NULL) {
        /* collect(*) - not really valid but handle gracefully */
        append_sql(ctx, "*");
    } else {
        /* _gql_json_elem keeps a boolean a JSON boolean (GQLITE-T-0404). A
         * comparison or other boolean expression is 1 / 0 in SQL, with no
         * boolean tag: give it one, so `collect(x = 1)` is [true, false],
         * not [1, 0]. */
        ast_node *arg = func_call->args->items[0];
        append_sql(ctx, "_gql_json_elem(");
        if (ast_is_boolean_expr(arg) && !(arg->type == AST_NODE_LITERAL)) {
            append_sql(ctx, "_gql_bool_str(CASE WHEN (");
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, ") IS NULL THEN NULL WHEN (");
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, ") THEN 1 ELSE 0 END)");
        } else {
            if (transform_expression(ctx, arg) < 0) return -1;
        }
        append_sql(ctx, ")");
    }
    append_sql(ctx, ") FILTER (WHERE ");
    if (func_call->args->items[0] != NULL) {
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " IS NOT NULL");
    } else {
        append_sql(ctx, "1=1");
    }
    append_sql(ctx, ")");

    return 0;
}

/* Transform randomUUID() function - generate a UUID v4 */
int transform_randomuuid_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming randomUUID() function");
    UNUSED_PARAMETER(func_call);

    /* Generate UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx */
    /* Using SQLite's random() and hex() functions */
    append_sql(ctx, "(lower(hex(randomblob(4))) || '-' || "
        "substr(lower(hex(randomblob(2))), 1, 4) || '-4' || "
        "substr(lower(hex(randomblob(2))), 2, 3) || '-' || "
        "substr('89ab', abs(random()) %% 4 + 1, 1) || "
        "substr(lower(hex(randomblob(2))), 2, 3) || '-' || "
        "lower(hex(randomblob(6))))");

    return 0;
}

/* Transform length() function - handles both paths and strings */
int transform_length_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming length() function");

    /* Check if argument is a path variable - use path length */
    if (func_call->args && func_call->args->count == 1 &&
        func_call->args->items[0] &&
        func_call->args->items[0]->type == AST_NODE_IDENTIFIER) {
        cypher_identifier *id = (cypher_identifier*)func_call->args->items[0];
        if (transform_var_is_path(ctx->var_ctx, id->name)) {
            return transform_path_length_function(ctx, func_call);
        }
        /* length() on a node or relationship is a SyntaxError per the
         * Cypher 9 spec (Path3 [2]/[3]). We detect "node" as bound but
         * not edge / path / projected / scalar. */
        if (transform_var_is_edge(ctx->var_ctx, id->name)) {
            ctx->has_error = true;
            ctx->error_message = strdup(
                "SyntaxError: InvalidArgumentType: length() does not accept relationships");
            return -1;
        }
        if (transform_var_is_bound(ctx->var_ctx, id->name) &&
            !transform_var_is_path(ctx->var_ctx, id->name) &&
            !transform_var_is_projected(ctx->var_ctx, id->name) &&
            !transform_var_is_scalar_value(ctx->var_ctx, id->name)) {
            /* Bound non-path/edge/scalar = node variable. */
            ctx->has_error = true;
            ctx->error_message = strdup(
                "SyntaxError: InvalidArgumentType: length() does not accept nodes");
            return -1;
        }
    }

    /* A projected value can be a path ({nodes, rels} object, e.g. a path
     * bound in a write pipeline): its length is the number of
     * relationships. Otherwise the length of a string. */
    if (func_call->args && func_call->args->count == 1 && func_call->args->items[0] &&
        func_call->args->items[0]->type == AST_NODE_IDENTIFIER &&
        transform_var_is_projected(ctx->var_ctx,
                                   ((cypher_identifier*)func_call->args->items[0])->name)) {
        char *v = cypher_transform_capture_expression(ctx, func_call->args->items[0]);
        if (!v) return -1;
        append_sql(ctx, "(CASE WHEN json_valid(%s) AND json_type(%s) = 'object' AND "
                        "json_type(%s, '$.rels') = 'array' THEN json_array_length(%s, '$.rels') ELSE ",
                   v, v, v, v);
        free(v);
        if (transform_string_function(ctx, func_call) < 0) return -1;
        append_sql(ctx, " END)");
        return 0;
    }
    return transform_string_function(ctx, func_call);
}

