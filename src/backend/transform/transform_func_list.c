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
#include "transform/transform_functions.h"
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

/* Transform toString function */
int transform_tostring_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming toString function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("toString() requires exactly one argument");
        return -1;
    }

    /* Special-case boolean expressions: SQLite can't tell a boolean
     * apart from integer 0/1 at SQL level, so `_gql_to_string_strict(true)`
     * returns "1". Detect the AST shapes that *must* produce a boolean
     * (literal true/false, NOT, AND/OR/XOR, comparison ops) and emit a
     * pre-JSON-encoded string so the JSON formatter's "starts with [, {, \""
     * fast-path passes it through verbatim. */
    ast_node *arg = func_call->args->items[0];
    bool is_bool_ast = false;
    if (arg) {
        if (arg->type == AST_NODE_LITERAL &&
            ((cypher_literal *)arg)->literal_type == LITERAL_BOOLEAN) {
            is_bool_ast = true;
        } else if (arg->type == AST_NODE_NOT_EXPR ||
                   arg->type == AST_NODE_NULL_CHECK) {
            is_bool_ast = true;
        } else if (arg->type == AST_NODE_BINARY_OP) {
            binary_op_type op = ((cypher_binary_op *)arg)->op_type;
            if (op == BINARY_OP_AND || op == BINARY_OP_OR || op == BINARY_OP_XOR ||
                op == BINARY_OP_EQ  || op == BINARY_OP_NEQ ||
                op == BINARY_OP_LT  || op == BINARY_OP_GT  ||
                op == BINARY_OP_LTE || op == BINARY_OP_GTE ||
                op == BINARY_OP_IN  || op == BINARY_OP_STARTS_WITH ||
                op == BINARY_OP_ENDS_WITH || op == BINARY_OP_CONTAINS ||
                op == BINARY_OP_REGEX_MATCH)
                is_bool_ast = true;
        }
    }
    if (is_bool_ast) {
        /* Wrap as ('"' || (CASE WHEN expr THEN 'true' ELSE 'false' END) || '"')
         * so the JSON formatter sees the quoted-string form. */
        append_sql(ctx, "('\"' || (CASE WHEN ");
        if (transform_expression(ctx, arg) < 0) return -1;
        append_sql(ctx, " IS NULL THEN NULL WHEN ");
        if (transform_expression(ctx, arg) < 0) return -1;
        append_sql(ctx, " THEN 'true' ELSE 'false' END) || '\"')");
        return 0;
    }

    /* Strict UDF rejects lists/maps/nodes/relationships/paths (TypeError)
     * but renders Duration JSON via its _iso8601 field. */
    append_sql(ctx, "_gql_to_string_strict(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ")");

    return 0;
}

/* Transform type conversion functions: toInteger, toFloat, toBoolean */
int transform_type_conversion_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming type conversion function: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() requires exactly one argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Route through strict UDFs that raise TypeError on incompatible
     * input types (Cypher contract). */
    const char *udf = NULL;
    if (strcasecmp(func_call->function_name, "toInteger") == 0) {
        udf = "_gql_to_int_strict";
    } else if (strcasecmp(func_call->function_name, "toFloat") == 0) {
        udf = "_gql_to_float_strict";
    } else if (strcasecmp(func_call->function_name, "toBoolean") == 0) {
        udf = "_gql_to_bool_strict";
    }
    if (udf) {
        append_sql(ctx, "%s(", udf);
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ")");
    }

    return 0;
}

/* Transform OrNull type conversion variants: toIntegerOrNull, toFloatOrNull, toBooleanOrNull, toStringOrNull
 * These return NULL instead of a default value when conversion is not possible. */
int transform_type_conversion_ornull_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming OrNull type conversion: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() requires exactly one argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    if (strcasecmp(func_call->function_name, "toIntegerOrNull") == 0) {
        /* Return NULL if not a valid integer, otherwise CAST AS INTEGER */
        append_sql(ctx, "(CASE WHEN typeof(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ") IN ('integer', 'real') OR CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT) GLOB '[0-9]*' OR CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT) GLOB '-[0-9]*' THEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS INTEGER) ELSE NULL END)");
    } else if (strcasecmp(func_call->function_name, "toFloatOrNull") == 0) {
        append_sql(ctx, "(CASE WHEN typeof(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ") IN ('integer', 'real') OR CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT) GLOB '[0-9]*' OR CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT) GLOB '-[0-9]*' OR CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT) GLOB '[0-9]*.[0-9]*' THEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) ELSE NULL END)");
    } else if (strcasecmp(func_call->function_name, "toBooleanOrNull") == 0) {
        append_sql(ctx, "(CASE WHEN LOWER(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT)) IN ('true', 'false', '1', '0') THEN (CASE WHEN LOWER(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT)) IN ('true', '1') THEN 1 ELSE 0 END) ELSE NULL END)");
    } else if (strcasecmp(func_call->function_name, "toStringOrNull") == 0) {
        /* toStringOrNull - always succeeds for non-null, returns NULL for null */
        append_sql(ctx, "CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS TEXT)");
    }

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
        /* tail(list) - all elements except first */
        /* Use json_remove to remove first element, but that doesn't work well */
        /* Instead, build a subquery that extracts elements 1..end */
        append_sql(ctx, "(SELECT json_group_array(value) FROM json_each(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ") WHERE key > 0)");
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
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
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

/* Transform timestamp() function - current Unix timestamp in milliseconds */
int transform_timestamp_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming timestamp() function");
    UNUSED_PARAMETER(func_call);

    /* Return Unix timestamp in milliseconds (Cypher standard) */
    /* SQLite: (strftime('%s', 'now') * 1000) + (strftime('%f', 'now') * 1000) % 1000 */
    append_sql(ctx, "CAST((julianday('now') - 2440587.5) * 86400000 AS INTEGER)");

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

    /* Otherwise treat as string length */
    return transform_string_function(ctx, func_call);
}

/* Transform date() function
 * date() - current date
 * date(string) - parse ISO date string
 * date({year, month, day}) - construct from map components
 */
int transform_date_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming date() function");

    if (func_call->args && func_call->args->count > 0) {
        ast_node *arg = func_call->args->items[0];
        if (arg->type == AST_NODE_MAP) {
            /* date({...}) — openCypher supports several map shapes:
             *   {year, month, day}
             *   {year, month}            (defaults day=1)
             *   {year}                   (defaults month=1, day=1)
             *   {year, week, dayOfWeek}  (ISO 8601 week date)
             *   {year, week}             (defaults dayOfWeek=1)
             *   {year, ordinalDay}       (day-of-year)
             *   {year, quarter, dayOfQuarter}
             *   {year, quarter}          (defaults dayOfQuarter=1)
             *
             * We emit one SQL expression that handles all of these by
             * inspecting which keys are present in the JSON. The expression
             * is the body of a CASE: precedence is week > ordinalDay >
             * quarter > month/day (since each precludes the others). */
            cypher_map *map = (cypher_map *)arg;
            bool has_week = false, has_ordinal = false, has_quarter = false;
            bool has_date_key = false, has_datetime_key = false;
            if (map->pairs) {
                for (int i = 0; i < map->pairs->count; i++) {
                    cypher_map_pair *p = (cypher_map_pair *)map->pairs->items[i];
                    if (!p || !p->key) continue;
                    if (strcasecmp(p->key, "week") == 0) has_week = true;
                    else if (strcasecmp(p->key, "ordinalDay") == 0) has_ordinal = true;
                    else if (strcasecmp(p->key, "quarter") == 0) has_quarter = true;
                    else if (strcasecmp(p->key, "date") == 0) has_date_key = true;
                    else if (strcasecmp(p->key, "datetime") == 0) has_datetime_key = true;
                }
            }

            /* When the map carries a `date` / `datetime` base value (used to
             * project from another temporal), delegate to the C composer
             * which inherits unspecified components from the base. */
            if (has_date_key || has_datetime_key) {
                append_sql(ctx, "_gql_date_compose(");
                const char *keys[] = {"year","month","day","week","dayOfWeek",
                                      "ordinalDay","quarter","dayOfQuarter",
                                      "date","datetime"};
                for (int i = 0; i < 10; i++) {
                    if (i > 0) append_sql(ctx, ", ");
                    append_sql(ctx, "json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", keys[i]);
                }
                append_sql(ctx, ")");
            } else if (has_week) {
                /* ISO week date: anchor on Jan 4 (always in week 1), back up
                 * to Monday-of-week-1, advance by (week-1)*7 + (dow-1) days. */
                append_sql(ctx, "(SELECT date("
                                 "printf('%%04d-01-04', json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year')),"
                                 "'-' || ((CAST(strftime('%%w', "
                                     "printf('%%04d-01-04', json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year'))) AS INTEGER) + 6) %% 7) || ' days',"
                                 "'+' || ((COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.week'), 1) - 1) * 7 + "
                                     "(COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.dayOfWeek'), 1) - 1)) || ' days'))");
            } else if (has_ordinal) {
                /* ordinalDay: day-of-year. date('YYYY-01-01', '+D-1 days'). */
                append_sql(ctx, "(SELECT date(printf('%%04d-01-01', "
                                 "json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year')), "
                                 "'+' || (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.ordinalDay'), 1) - 1) || ' days'))");
            } else if (has_quarter) {
                /* quarter / dayOfQuarter: start_month = (Q-1)*3+1. */
                append_sql(ctx, "(SELECT date("
                                 "printf('%%04d-%%02d-01', "
                                     "json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year'), "
                                     "(COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.quarter'), 1) - 1) * 3 + 1), "
                                 "'+' || (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.dayOfQuarter'), 1) - 1) || ' days'))");
            } else {
                /* Plain year/month/day. */
                append_sql(ctx, "(SELECT printf('%%04d-%%02d-%%02d', "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year'), strftime('%%Y','now')), "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.month'), 1), "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.day'), 1)))");
            }
        } else {
            /* date(value): normalize via the C helper which handles all
             * ISO date forms (week date, ordinal day, basic/extended). */
            append_sql(ctx, "_gql_normalize_date(");
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, ")");
        }
        if (false) {
            append_sql(ctx, "(SELECT date(CASE "
                /* basic ISO date YYYYMMDD (8 chars, no dash, no W) */
                "WHEN length(_str)=8 AND substr(_str,5,1) BETWEEN '0' AND '9' "
                    "AND instr(_str,'W')=0 AND instr(_str,'-')=0 "
                    "THEN substr(_str,1,4)||'-'||substr(_str,5,2)||'-'||substr(_str,7,2) "
                /* extended year-month YYYY-MM (7 chars) */
                "WHEN length(_str)=7 AND substr(_str,5,1)='-' THEN _str||'-01' "
                /* basic year-month YYYYMM (6 chars) */
                "WHEN length(_str)=6 AND instr(_str,'-')=0 AND instr(_str,'W')=0 "
                    "THEN substr(_str,1,4)||'-'||substr(_str,5,2)||'-01' "
                /* year only YYYY (4 chars) */
                "WHEN length(_str)=4 THEN _str||'-01-01' "
                /* extended ordinal YYYY-DDD (8 chars: 4-digit year, dash, 3-digit day) */
                "WHEN length(_str)=8 AND substr(_str,5,1)='-' "
                    "THEN date(substr(_str,1,4)||'-01-01', "
                        "'+'||(CAST(substr(_str,6,3) AS INTEGER)-1)||' days') "
                /* basic ordinal YYYYDDD (7 chars: 4-digit year + 3-digit day) */
                "WHEN length(_str)=7 AND instr(_str,'-')=0 AND instr(_str,'W')=0 "
                    "THEN date(substr(_str,1,4)||'-01-01', "
                        "'+'||(CAST(substr(_str,5,3) AS INTEGER)-1)||' days') "
                "ELSE _str END) FROM (SELECT (");
            (void)0;
            append_sql(ctx, ") AS _str))");
        }
    } else {
        /* date() - current date */
        append_sql(ctx, "date('now')");
    }
    return 0;
}

/* Transform time() function
 * time() - current time
 * time(string) - parse ISO time string
 * time({hour, minute, second}) - construct from map
 */
int transform_time_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming time() function");

    bool is_localtime = (func_call->function_name &&
                         strcasecmp(func_call->function_name, "localtime") == 0);

    if (func_call->args && func_call->args->count > 0) {
        ast_node *arg = func_call->args->items[0];
        if (arg->type == AST_NODE_MAP) {
            /* localtime/time({hour, minute, second?, [ns/us/ms]}) → emit the
             * right-precision format string. The fractional value is the
             * TOTAL nanosecond count summed across millisecond + microsecond
             * + nanosecond fields, then truncated to the highest precision
             * the input declared. time() (not localtime) appends a Z suffix
             * or explicit timezone. */
            cypher_map *map = (cypher_map *)arg;
            bool has_second=false, has_ms=false, has_us=false, has_ns=false;
            bool has_time_key=false, has_dt_key=false, has_ldt_key=false, has_lt_key=false;
            if (map->pairs) {
                for (int i = 0; i < map->pairs->count; i++) {
                    cypher_map_pair *p = (cypher_map_pair *)map->pairs->items[i];
                    if (!p || !p->key) continue;
                    if (strcasecmp(p->key, "second") == 0) has_second = true;
                    else if (strcasecmp(p->key, "millisecond") == 0) has_ms = true;
                    else if (strcasecmp(p->key, "microsecond") == 0) has_us = true;
                    else if (strcasecmp(p->key, "nanosecond") == 0) has_ns = true;
                    else if (strcasecmp(p->key, "time") == 0) has_time_key = true;
                    else if (strcasecmp(p->key, "datetime") == 0) has_dt_key = true;
                    else if (strcasecmp(p->key, "localdatetime") == 0) has_ldt_key = true;
                    else if (strcasecmp(p->key, "localtime") == 0) has_lt_key = true;
                }
            }
            /* Base-value projection: time({time: t, …}) / localtime({…}) — delegate
             * to _gql_time_compose which inherits unspecified components. */
            if (has_time_key || has_dt_key || has_ldt_key || has_lt_key) {
                append_sql(ctx, "_gql_time_compose(");
                const char *keys[] = {"hour","minute","second","millisecond","microsecond","nanosecond","timezone"};
                for (int i = 0; i < 7; i++) {
                    if (i > 0) append_sql(ctx, ", ");
                    append_sql(ctx, "json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", keys[i]);
                }
                append_sql(ctx, ", %d", is_localtime ? 0 : 1);
                /* base time / datetime / localdatetime / localtime */
                const char *base_keys[] = {"time","datetime","localdatetime","localtime"};
                for (int i = 0; i < 4; i++) {
                    append_sql(ctx, ", json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", base_keys[i]);
                }
                append_sql(ctx, ")");
                return 0;
            }
            const char *fmt_in_sql;
            int frac_divisor = 0;   /* 0 = no fractional, else divisor applied to total-ns */
            if (has_ns)        { fmt_in_sql = "'%02d:%02d:%02d.%09d'"; frac_divisor = 1; }
            else if (has_us)   { fmt_in_sql = "'%02d:%02d:%02d.%06d'"; frac_divisor = 1000; }
            else if (has_ms)   { fmt_in_sql = "'%02d:%02d:%02d.%03d'"; frac_divisor = 1000000; }
            else if (has_second) fmt_in_sql = "'%02d:%02d:%02d'";
            else                 fmt_in_sql = "'%02d:%02d'";

            append_sql(ctx, "(SELECT printf(%s, "
                              "COALESCE(json_extract(json(", fmt_in_sql);
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, "), '$.hour'), 0), "
                              "COALESCE(json_extract(json(");
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, "), '$.minute'), 0)");
            if (has_second || frac_divisor) {
                append_sql(ctx, ", COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.second'), 0)");
            }
            if (frac_divisor) {
                append_sql(ctx, ", (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.nanosecond'), 0) + "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.microsecond'), 0) * 1000 + "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.millisecond'), 0) * 1000000) / %d",
                           frac_divisor);
            }
            append_sql(ctx, ")");
            if (!is_localtime) {
                /* time() with no timezone → 'Z'; else verbatim or [Name]. */
                append_sql(ctx, " || (CASE "
                                 "WHEN json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone') IS NULL THEN 'Z' "
                                 "WHEN substr(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone'),1,1) IN ('+','-') "
                                 "THEN json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone') "
                                 "ELSE '[' || json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone') || ']' END)");
            }
            append_sql(ctx, ")");
        } else {
            /* time(string) / localtime(string): preserve verbatim so tz +
             * sub-second precision survive (SQLite's time() drops both).
             * For time() (not localtime), the value is tz-aware — if the
             * source string has no tz suffix, append 'Z' (UTC default) so
             * downstream temporal-diff code applies tz consistently. */
            if (!is_localtime) {
                /* Normalize to canonical form and append 'Z' if no tz. */
                append_sql(ctx, "((_gql_normalize_time(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, ")) || CASE WHEN _gql_extract_tz(_gql_normalize_time(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, ")) = '' THEN 'Z' ELSE '' END)");
            } else {
                /* localtime(other): normalize then strip any tz. */
                append_sql(ctx, "_gql_strip_tz(_gql_normalize_time(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "))");
            }
        }
    } else {
        /* time() - current time */
        append_sql(ctx, "time('now')");
    }
    return 0;
}

/* Transform datetime() / localdatetime() function
 * datetime() - current datetime
 * datetime(string) - parse ISO datetime string
 * datetime({year, month, day, hour, minute, second}) - construct from map
 */
int transform_datetime_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming datetime() function");

    /* datetime() emits a `Z` (UTC) suffix or explicit timezone; localdatetime()
     * never does. We pick the right behavior based on the original function
     * name. */
    bool is_local = (func_call->function_name &&
                     strcasecmp(func_call->function_name, "localdatetime") == 0);

    if (func_call->args && func_call->args->count > 0) {
        ast_node *arg = func_call->args->items[0];
        if (arg->type == AST_NODE_MAP) {
            /* localdatetime/datetime({...}): emit the precise format TCK
             * expects, which depends on the highest-precision sub-second
             * field present (nanosecond > microsecond > millisecond > none)
             * and whether second is present at all (else format ends at
             * "HH:MM"). Date portion uses the same calendar variants the
             * `date()` map constructor supports.
             *
             * We do this by inspecting the map AST keys at compile time so
             * the generated SQL has a fixed printf format. This avoids
             * deep CASE expressions at runtime. */
            cypher_map *map = (cypher_map *)arg;
            bool has_week=false, has_ordinal=false, has_quarter=false;
            bool has_hour=false, has_minute=false, has_second=false;
            bool has_ms=false, has_us=false, has_ns=false;
            bool has_date_key=false, has_time_key=false;
            bool has_dt_key=false, has_ldt_key=false;
            if (map->pairs) {
                for (int i = 0; i < map->pairs->count; i++) {
                    cypher_map_pair *p = (cypher_map_pair *)map->pairs->items[i];
                    if (!p || !p->key) continue;
                    if (strcasecmp(p->key, "week") == 0) has_week = true;
                    else if (strcasecmp(p->key, "ordinalDay") == 0) has_ordinal = true;
                    else if (strcasecmp(p->key, "quarter") == 0) has_quarter = true;
                    else if (strcasecmp(p->key, "hour") == 0) has_hour = true;
                    else if (strcasecmp(p->key, "minute") == 0) has_minute = true;
                    else if (strcasecmp(p->key, "second") == 0) has_second = true;
                    else if (strcasecmp(p->key, "millisecond") == 0) has_ms = true;
                    else if (strcasecmp(p->key, "microsecond") == 0) has_us = true;
                    else if (strcasecmp(p->key, "nanosecond") == 0) has_ns = true;
                    else if (strcasecmp(p->key, "date") == 0) has_date_key = true;
                    else if (strcasecmp(p->key, "time") == 0) has_time_key = true;
                    else if (strcasecmp(p->key, "datetime") == 0) has_dt_key = true;
                    else if (strcasecmp(p->key, "localdatetime") == 0) has_ldt_key = true;
                }
            }

            /* Map carries projection from another temporal — combine via
             * date+time composers. The C helpers handle inherited components. */
            if (has_date_key || has_time_key || has_dt_key || has_ldt_key) {
                append_sql(ctx, "(_gql_date_compose(");
                const char *dkeys[] = {"year","month","day","week","dayOfWeek",
                                       "ordinalDay","quarter","dayOfQuarter"};
                for (int i = 0; i < 8; i++) {
                    if (i > 0) append_sql(ctx, ", ");
                    append_sql(ctx, "json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", dkeys[i]);
                }
                /* For date_compose, base_date is map.date or map.datetime/localdatetime. */
                append_sql(ctx, ", COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.date'), json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.datetime'), json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.localdatetime'))");
                append_sql(ctx, ", json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.datetime'))");
                /* Date emitted. Now emit 'T' separator and time portion via composer. */
                append_sql(ctx, " || 'T' || _gql_time_compose(");
                const char *tkeys[] = {"hour","minute","second","millisecond","microsecond","nanosecond","timezone"};
                for (int i = 0; i < 7; i++) {
                    if (i > 0) append_sql(ctx, ", ");
                    append_sql(ctx, "json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", tkeys[i]);
                }
                append_sql(ctx, ", %d", is_local ? 0 : 1);
                /* time_compose bases: time, datetime, localdatetime, localtime. */
                const char *base_keys[] = {"time","datetime","localdatetime","localtime"};
                for (int i = 0; i < 4; i++) {
                    append_sql(ctx, ", json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", base_keys[i]);
                }
                append_sql(ctx, "))");
                return 0;
            }

            /* Compose the time portion's printf format + args. The format
             * string is embedded into the SQL via append_sql's vsnprintf
             * pass, where `%%` reduces to `%` and SQLite sees the right
             * format. */
            const char *time_fmt_in_sql;
            int frac_divisor = 0;
            /* These are inserted into the SQL via append_sql's `%s` arg, so
             * the `%`s here are taken literally and end up in the SQL where
             * SQLite's `printf()` will interpret them. The fractional value
             * is the total nanosecond count across ns/us/ms, then divided
             * to the chosen precision. */
            if (has_ns)        { time_fmt_in_sql = "'T%02d:%02d:%02d.%09d'"; frac_divisor = 1; }
            else if (has_us)   { time_fmt_in_sql = "'T%02d:%02d:%02d.%06d'"; frac_divisor = 1000; }
            else if (has_ms)   { time_fmt_in_sql = "'T%02d:%02d:%02d.%03d'"; frac_divisor = 1000000; }
            else if (has_second) time_fmt_in_sql = "'T%02d:%02d:%02d'";
            else                 time_fmt_in_sql = "'T%02d:%02d'";  /* hour/minute/none all use HH:MM */

            /* Build the date prefix once (same logic as date() function). */
            append_sql(ctx, "(SELECT ");
            if (has_week) {
                append_sql(ctx, "date("
                                 "printf('%%04d-01-04', json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year')),"
                                 "'-' || ((CAST(strftime('%%w', "
                                     "printf('%%04d-01-04', json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year'))) AS INTEGER) + 6) %% 7) || ' days',"
                                 "'+' || ((COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.week'), 1) - 1) * 7 + "
                                     "(COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.dayOfWeek'), 1) - 1)) || ' days')");
            } else if (has_ordinal) {
                append_sql(ctx, "date(printf('%%04d-01-01', json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year')), '+' || (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.ordinalDay'), 1) - 1) || ' days')");
            } else if (has_quarter) {
                append_sql(ctx, "date(printf('%%04d-%%02d-01', json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year'), (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.quarter'), 1) - 1) * 3 + 1), "
                                 "'+' || (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.dayOfQuarter'), 1) - 1) || ' days')");
            } else {
                append_sql(ctx, "printf('%%04d-%%02d-%%02d', "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.year'), strftime('%%Y','now')), "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.month'), 1), "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.day'), 1))");
            }

            /* Append the time portion via concatenation. */
            append_sql(ctx, " || printf(%s, "
                              "COALESCE(json_extract(json(", time_fmt_in_sql);
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, "), '$.hour'), 0), "
                              "COALESCE(json_extract(json(");
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, "), '$.minute'), 0)");
            if (has_second || frac_divisor) {
                append_sql(ctx, ", COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.second'), 0)");
            }
            if (frac_divisor) {
                append_sql(ctx, ", (COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.nanosecond'), 0) + "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.microsecond'), 0) * 1000 + "
                                 "COALESCE(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.millisecond'), 0) * 1000000) / %d",
                           frac_divisor);
            }
            append_sql(ctx, ")");
            /* datetime() (not localdatetime) appends a timezone suffix:
             *   - the explicit '$.timezone' if provided (offset like '+01:00'
             *     verbatim, named like 'Europe/Stockholm' wrapped in [...])
             *   - else 'Z' for UTC. */
            if (!is_local) {
                /* tz suffix: explicit offset verbatim; named zone gets
                 * '<offset>[Name]' with DST-aware offset via C helper. The
                 * date passed to _gql_tz_offset_for is composed from the
                 * full map so ordinalDay/week/quarter inputs resolve to
                 * the right calendar month before DST lookup. */
                append_sql(ctx, " || (CASE "
                                 "WHEN json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone') IS NULL THEN 'Z' "
                                 "WHEN substr(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone'),1,1) IN ('+','-') "
                                 "THEN json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone') "
                                 "ELSE _gql_tz_offset_for(json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone'), _gql_date_compose(");
                const char *dkeys2[] = {"year","month","day","week","dayOfWeek",
                                        "ordinalDay","quarter","dayOfQuarter"};
                for (int i = 0; i < 8; i++) {
                    if (i > 0) append_sql(ctx, ", ");
                    append_sql(ctx, "json_extract(json(");
                    if (transform_expression(ctx, arg) < 0) return -1;
                    append_sql(ctx, "), '$.%s')", dkeys2[i]);
                }
                /* base_date / base_datetime — null here for the legacy path. */
                append_sql(ctx, ", NULL, NULL)) || '[' || json_extract(json(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "), '$.timezone') || ']' END)");
            }
            append_sql(ctx, ")");
        } else {
            /* datetime/localdatetime(string|value): normalize via the C helper
             * which canonicalizes date and time portions (week date, ordinal,
             * basic vs extended). Then for datetime() add 'Z' if no tz; for
             * localdatetime() strip any tz. */
            if (is_local) {
                append_sql(ctx, "_gql_strip_tz(_gql_normalize_datetime(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, "))");
            } else {
                append_sql(ctx, "((_gql_normalize_datetime(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, ")) || CASE WHEN _gql_extract_tz(_gql_normalize_datetime(");
                if (transform_expression(ctx, arg) < 0) return -1;
                append_sql(ctx, ")) = '' THEN 'Z' ELSE '' END)");
            }
        }
    } else {
        /* datetime() - current datetime */
        append_sql(ctx, "datetime('now')");
    }
    return 0;
}

/* Transform duration() function
 * duration(string) - parse ISO 8601 duration string like 'P1Y2M3DT4H5M6S'
 * duration({years, months, days, hours, minutes, seconds}) - construct from map
 *
 * Stored as SQLite modifier string for use with date/time arithmetic:
 * '+N years', '+N months', '+N days', '+N hours', '+N minutes', '+N seconds'
 * Represented as a JSON object internally for composition.
 */
int transform_duration_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming duration() function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("duration() requires exactly one argument (map or string)");
        return -1;
    }

    ast_node *arg = func_call->args->items[0];
    if (arg->type == AST_NODE_MAP) {
        /* duration({years, months, weeks, days, hours, minutes, seconds,
         *           milliseconds, microseconds, nanoseconds}) — delegate to
         * the C composer which handles fractional cascade and produces the
         * canonical Duration JSON with `_iso8601`. */
        append_sql(ctx, "json(_gql_duration_compose(");
        const char *keys[] = {"years","months","weeks","days","hours",
                              "minutes","seconds","milliseconds",
                              "microseconds","nanoseconds"};
        for (int i = 0; i < 10; i++) {
            if (i > 0) append_sql(ctx, ", ");
            append_sql(ctx, "COALESCE(json_extract(json(");
            if (transform_expression(ctx, arg) < 0) return -1;
            append_sql(ctx, "), '$.%s'), 0)", keys[i]);
        }
        append_sql(ctx, "))");
    } else {
        /* duration('P…') — parse ISO 8601 string. */
        append_sql(ctx, "json(_gql_duration_parse_iso(");
        if (transform_expression(ctx, arg) < 0) return -1;
        append_sql(ctx, "))");
    }

    return 0;
}

/* Transform datetime.fromEpoch(seconds, nanoseconds) and datetime.fromEpochMillis(ms)
 * These are dispatched as function calls: datetimeFromEpoch, datetimeFromEpochMillis
 */
int transform_datetime_from_epoch_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming datetime.fromEpoch function");

    bool is_millis = (strcasecmp(func_call->function_name, "datetimeFromEpochMillis") == 0 ||
                     strcasecmp(func_call->function_name, "datetimefromepochmillis") == 0 ||
                     strcasecmp(func_call->function_name, "datetime.fromEpochMillis") == 0);

    if (!func_call->args || func_call->args->count < 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("datetime.fromEpoch() requires at least one argument");
        return -1;
    }

    if (is_millis) {
        /* datetime.fromEpochMillis(ms) →
         *   'YYYY-MM-DDTHH:MM:SS[.fff]Z'
         * Builds the ISO-8601 form openCypher specifies. */
        append_sql(ctx, "(strftime('%%Y-%%m-%%dT%%H:%%M:%%S', CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) / 1000.0, 'unixepoch') || "
            "CASE WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS INTEGER) %% 1000 = 0 THEN '' "
            "ELSE '.' || substr('000' || (CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS INTEGER) %% 1000), -3) END || 'Z')");
    } else {
        /* datetime.fromEpoch(seconds[, nanoseconds]) → ISO-8601 UTC. */
        append_sql(ctx, "(strftime('%%Y-%%m-%%dT%%H:%%M:%%S', ");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", 'unixepoch')");
        if (func_call->args->count >= 2) {
            /* Append .NNNNNNNNN nanoseconds (left-zero-padded to 9 chars). */
            append_sql(ctx, " || CASE WHEN CAST(");
            if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
            append_sql(ctx, " AS INTEGER) = 0 THEN '' ELSE '.' || substr('000000000' || CAST(");
            if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
            append_sql(ctx, " AS INTEGER), -9) END");
        }
        append_sql(ctx, " || 'Z')");
    }

    return 0;
}

/* Transform date.truncate(unit, temporal)
 * Truncates a temporal value to the specified unit.
 */
/* Forward declaration — implementation lives below with the datetime/time
 * truncate helpers so all units share one source of truth. */
static int emit_truncate_date_portion(cypher_transform_context *ctx,
                                       ast_node *unit, ast_node *src, ast_node *map);

int transform_date_truncate_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming date.truncate function");

    if (!func_call->args || (func_call->args->count != 2 && func_call->args->count != 3)) {
        ctx->has_error = true;
        ctx->error_message = strdup("date.truncate() requires 2 or 3 arguments: unit, temporal[, map]");
        return -1;
    }
    ast_node *unit = func_call->args->items[0];
    ast_node *src = func_call->args->items[1];
    ast_node *map = (func_call->args->count == 3) ? func_call->args->items[2] : NULL;
    append_sql(ctx, "(");
    if (emit_truncate_date_portion(ctx, unit, src, map) < 0) return -1;
    append_sql(ctx, ")");
    return 0;
}

/* Stub block to preserve old text indentation; the next block is unreachable
 * legacy code that the compiler may eliminate. Kept temporarily as comments
 * for reference. */
static int __unused_legacy_date_truncate(cypher_transform_context *ctx, cypher_function_call *func_call) {
    (void)ctx; (void)func_call;
    if (0) {
    append_sql(ctx, "(CASE ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    /* millennium */
    append_sql(ctx, " WHEN 'millennium' THEN printf('%%04d-01-%%02d', (CAST(strftime('%%Y', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") AS INTEGER)/1000)*1000, COALESCE(json_extract(");
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "'{}'");
    }
    append_sql(ctx, ", '$.day'), 1))");
    /* century */
    append_sql(ctx, " WHEN 'century' THEN printf('%%04d-01-%%02d', (CAST(strftime('%%Y', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") AS INTEGER)/100)*100, COALESCE(json_extract(");
    if (func_call->args->count == 3) { if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1; } else append_sql(ctx, "'{}'");
    append_sql(ctx, ", '$.day'), 1))");
    /* decade */
    append_sql(ctx, " WHEN 'decade' THEN printf('%%04d-01-%%02d', (CAST(strftime('%%Y', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") AS INTEGER)/10)*10, COALESCE(json_extract(");
    if (func_call->args->count == 3) { if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1; } else append_sql(ctx, "'{}'");
    append_sql(ctx, ", '$.day'), 1))");
    /* year */
    append_sql(ctx, " WHEN 'year' THEN strftime('%%Y', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") || printf('-01-%%02d', COALESCE(json_extract(");
    if (func_call->args->count == 3) { if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1; } else append_sql(ctx, "'{}'");
    append_sql(ctx, ", '$.day'), 1))");
    /* quarter */
    append_sql(ctx, " WHEN 'quarter' THEN strftime('%%Y', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") || printf('-%%02d-%%02d', ((CAST(strftime('%%m', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") AS INTEGER)-1)/3)*3+1, COALESCE(json_extract(");
    if (func_call->args->count == 3) { if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1; } else append_sql(ctx, "'{}'");
    append_sql(ctx, ", '$.day'), 1))");
    /* month */
    append_sql(ctx, " WHEN 'month' THEN strftime('%%Y-%%m', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") || printf('-%%02d', COALESCE(json_extract(");
    if (func_call->args->count == 3) { if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1; } else append_sql(ctx, "'{}'");
    append_sql(ctx, ", '$.day'), 1))");
    /* week: Monday of the week, plus dayOfWeek offset (1=Mon) */
    append_sql(ctx, " WHEN 'week' THEN date(date(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '-' || ((CAST(strftime('%%w', ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") AS INTEGER) + 6) %% 7) || ' days'), '+' || (COALESCE(json_extract(");
    if (func_call->args->count == 3) { if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1; } else append_sql(ctx, "'{}'");
    append_sql(ctx, ", '$.dayOfWeek'), 1) - 1) || ' days')");
    /* day */
    append_sql(ctx, " WHEN 'day' THEN date(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ")");
    /* fallback: SQLite native */
    append_sql(ctx, " ELSE date(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", 'start of ' || ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ") END)");
    }
    return 0;
}

/* ---------- Helpers for datetime/localdatetime/time/localtime.truncate ----------
 *
 * The SQL we emit is a single string-concatenation expression built from:
 *   [date_portion]   (omitted for time/localtime kinds)
 *   'T'              (only between date and time when both present)
 *   [time_portion]
 *   [tz_portion]     (omitted for localdatetime/localtime kinds)
 *
 * `unit`, `src`, `map` are AST expressions. The unit is a literal in TCK
 * scenarios so all CASE branches are still cheap; for safety we just emit
 * the AST as SQL each time it's referenced.
 *
 * `append_sql` is printf-style so every literal `%` must be doubled.
 */

#define EMIT(e) do { if (transform_expression(ctx, (e)) < 0) return -1; } while (0)

/* Emit the map expression, or '{}' if NULL. */
static int emit_map_or_empty(cypher_transform_context *ctx, ast_node *map) {
    if (map) { EMIT(map); }
    else append_sql(ctx, "'{}'");
    return 0;
}

/* Emit date portion (YYYY-MM-DD) — same logic as date.truncate. */
static int emit_truncate_date_portion(cypher_transform_context *ctx,
                                       ast_node *unit, ast_node *src, ast_node *map) {
    append_sql(ctx, "CASE "); EMIT(unit);
    /* millennium */
    append_sql(ctx, " WHEN 'millennium' THEN printf('%%04d-01-%%02d', (CAST(strftime('%%Y', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)/1000)*1000, COALESCE(json_extract(");
    if (emit_map_or_empty(ctx, map) < 0) return -1;
    append_sql(ctx, ", '$.day'), 1))");
    /* century */
    append_sql(ctx, " WHEN 'century' THEN printf('%%04d-01-%%02d', (CAST(strftime('%%Y', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)/100)*100, COALESCE(json_extract("); if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day'), 1))");
    /* decade */
    append_sql(ctx, " WHEN 'decade' THEN printf('%%04d-01-%%02d', (CAST(strftime('%%Y', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)/10)*10, COALESCE(json_extract("); if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day'), 1))");
    /* year */
    append_sql(ctx, " WHEN 'year' THEN strftime('%%Y', "); EMIT(src);
    append_sql(ctx, ") || printf('-01-%%02d', COALESCE(json_extract("); if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day'), 1))");
    /* quarter */
    append_sql(ctx, " WHEN 'quarter' THEN strftime('%%Y', "); EMIT(src);
    append_sql(ctx, ") || printf('-%%02d-%%02d', ((CAST(strftime('%%m', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)-1)/3)*3+1, COALESCE(json_extract("); if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day'), 1))");
    /* month */
    append_sql(ctx, " WHEN 'month' THEN strftime('%%Y-%%m', "); EMIT(src);
    append_sql(ctx, ") || printf('-%%02d', COALESCE(json_extract("); if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day'), 1))");
    /* week */
    append_sql(ctx, " WHEN 'week' THEN date(date("); EMIT(src);
    append_sql(ctx, ", '-' || ((CAST(strftime('%%w', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER) + 6) %% 7) || ' days'), '+' || (COALESCE(json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.dayOfWeek'), 1) - 1) || ' days')");

    /* weekYear: ISO week-year. Start = Monday of ISO week 1 of weekYear.
     * weekYear is the year of the Thursday of the week containing src.
     * ISO week 1 contains Jan 4. Monday of ISO week 1 = Jan 4 - dow-Monday-offset.
     *
     * Let th(d) = Thursday of week containing d
     *           = date(d, '-' || ((dow(d)+6)%7) || ' days', '+3 days')
     * Let wy_start(d) = date(strftime('%Y-01-04', th(d)),
     *                        '-' || ((dow(strftime('%Y-01-04', th(d)))+6)%7) || ' days')
     */
    /* weekYear with no day override → Monday of ISO week 1.
     * weekYear with {day: N}    → <weekYear>-01-N (day-of-month). */
    append_sql(ctx, " WHEN 'weekYear' THEN CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day') IS NOT NULL THEN printf('%%04d-01-%%02d', "
        "CAST(strftime('%%Y', date("); EMIT(src);
    append_sql(ctx, ", '-' || ((CAST(strftime('%%w', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)+6)%%7) || ' days', '+3 days')) AS INTEGER), json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.day')) ELSE date(strftime('%%Y-01-04', date("); EMIT(src);
    append_sql(ctx, ", '-' || ((CAST(strftime('%%w', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)+6)%%7) || ' days', '+3 days')), '-' || "
        "((CAST(strftime('%%w', strftime('%%Y-01-04', date("); EMIT(src);
    append_sql(ctx, ", '-' || ((CAST(strftime('%%w', "); EMIT(src);
    append_sql(ctx, ") AS INTEGER)+6)%%7) || ' days', '+3 days'))) AS INTEGER)+6)%%7) || ' days') END");

    /* Other units (hour, minute, second, ms, us, ns, day) — preserve calendar date */
    append_sql(ctx, " ELSE date("); EMIT(src); append_sql(ctx, ") END");
    return 0;
}

/* Extract clock-time fields directly from the ISO datetime string. We must
 * NOT use strftime because it normalizes timezone-bearing inputs to UTC,
 * which is the opposite of what truncate wants (preserve wall-clock time).
 * Source format is 'YYYY-MM-DDTHH:MM:SS[.frac][tz]' or 'YYYY-MM-DD'. */
/* Extract the time portion of a temporal string, normalized so it always
 * starts with 'HH:MM:SS' (zero-padded to 8 chars). Handles three input shapes:
 *  - 'YYYY-MM-DDTHH:MM:SS[.frac][tz]'  → take substring after 'T'
 *  - 'HH:MM:SS[.frac][tz]'            → use as-is
 *  - 'YYYY-MM-DD' (length <= 10)      → '00:00:00'
 */
#define EMIT_TIME_BASE(s) do { \
    append_sql(ctx, "(CASE WHEN instr("); EMIT(s); append_sql(ctx, ",'T')>0 THEN substr("); EMIT(s); \
    append_sql(ctx, ", instr("); EMIT(s); append_sql(ctx, ",'T')+1) WHEN length("); EMIT(s); \
    append_sql(ctx, ")<=10 THEN '00:00:00' ELSE "); EMIT(s); append_sql(ctx, " END || '00:00:00')"); \
} while(0)
#define EMIT_HH(s) do { append_sql(ctx, "substr("); EMIT_TIME_BASE(s); append_sql(ctx, ", 1, 2)"); } while(0)
#define EMIT_MM(s) do { append_sql(ctx, "substr("); EMIT_TIME_BASE(s); append_sql(ctx, ", 4, 2)"); } while(0)
#define EMIT_SS(s) do { append_sql(ctx, "substr("); EMIT_TIME_BASE(s); append_sql(ctx, ", 7, 2)"); } while(0)

/* Emit the time portion (HH:MM[:SS[.fff]]).
 *  - For 'date'-like units when map has no 'nanosecond' override → '00:00'.
 *  - For sub-second units → include fractional part.
 *  - When ns_override present, always pad to 9 fractional digits.
 * The output never includes the 'T' separator. */
static int emit_truncate_time_portion(cypher_transform_context *ctx,
                                       ast_node *unit, ast_node *src, ast_node *map) {
    /* Decide whether ns_override is present at runtime. We branch on
     * (json_extract(map, '$.nanosecond') IS NOT NULL). */
    /* For brevity, emit a nested CASE on unit producing the right format. */
    append_sql(ctx, "CASE "); EMIT(unit);

    /* DATELIKE units (millennium, century, decade, year, weekYear, quarter,
     *                  month, week, day) → no time component. */
    const char *date_units[] = {"millennium","century","decade","year","weekYear",
                                "quarter","month","week","day", NULL};
    for (int i = 0; date_units[i]; i++) {
        append_sql(ctx, " WHEN '%s' THEN CASE WHEN json_extract(", date_units[i]);
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.nanosecond') IS NOT NULL THEN printf('00:00:00.%%09d', json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.nanosecond')) WHEN json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.second') IS NOT NULL THEN printf('%%02d:%%02d:%%02d', COALESCE(json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.hour'),0), COALESCE(json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.minute'),0), json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.second')) WHEN json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.minute') IS NOT NULL THEN printf('%%02d:%%02d', COALESCE(json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.hour'),0), json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.minute')) WHEN json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.hour') IS NOT NULL THEN printf('%%02d:00', json_extract(");
        if (emit_map_or_empty(ctx, map)<0) return -1;
        append_sql(ctx, ", '$.hour')) ELSE '00:00' END");
    }

    /* hour: '<HH>:00', or with overrides */
    append_sql(ctx, " WHEN 'hour' THEN CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond') IS NOT NULL THEN "); EMIT_HH(src);
    append_sql(ctx, " || printf(':00:00.%%09d', json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond')) WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.second') IS NOT NULL THEN "); EMIT_HH(src);
    append_sql(ctx, " || printf(':%%02d:%%02d', COALESCE(json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.minute'),0), json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.second')) WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.minute') IS NOT NULL THEN "); EMIT_HH(src);
    append_sql(ctx, " || printf(':%%02d', json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.minute')) ELSE "); EMIT_HH(src); append_sql(ctx, " || ':00' END");

    /* minute: 'HH:MM', or with overrides */
    append_sql(ctx, " WHEN 'minute' THEN CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond') IS NOT NULL THEN "); EMIT_HH(src); append_sql(ctx, " || ':' "); append_sql(ctx, "|| "); EMIT_MM(src);
    append_sql(ctx, " || printf(':00.%%09d', json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond')) WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.second') IS NOT NULL THEN "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src);
    append_sql(ctx, " || printf(':%%02d', json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.second')) ELSE "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src); append_sql(ctx, " END");

    /* second: 'HH:MM:SS', with optional ns_override */
    append_sql(ctx, " WHEN 'second' THEN CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond') IS NOT NULL THEN "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src); append_sql(ctx, " || ':' || "); EMIT_SS(src);
    append_sql(ctx, " || printf('.%%09d', json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond')) ELSE "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src); append_sql(ctx, " || ':' || "); EMIT_SS(src); append_sql(ctx, " END");

    /* millisecond: 'HH:MM:SS.fff' (3 digits, no override) or 'HH:MM:SS.ffffffffff' (9 digits, with override) */
    append_sql(ctx, " WHEN 'millisecond' THEN "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src); append_sql(ctx, " || ':' || "); EMIT_SS(src);
    append_sql(ctx, " || CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond') IS NOT NULL THEN printf('.%%09d', (_gql_extract_ns("); EMIT(src);
    append_sql(ctx, ") / 1000000) * 1000000 + json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond')) ELSE printf('.%%03d', _gql_extract_ns("); EMIT(src); append_sql(ctx, ") / 1000000) END");
    /* microsecond */
    append_sql(ctx, " WHEN 'microsecond' THEN "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src); append_sql(ctx, " || ':' || "); EMIT_SS(src);
    append_sql(ctx, " || CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond') IS NOT NULL THEN printf('.%%09d', (_gql_extract_ns("); EMIT(src);
    append_sql(ctx, ") / 1000) * 1000 + json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond')) ELSE printf('.%%06d', _gql_extract_ns("); EMIT(src); append_sql(ctx, ") / 1000) END");
    /* nanosecond (always 9-digit) */
    append_sql(ctx, " WHEN 'nanosecond' THEN "); EMIT_HH(src); append_sql(ctx, " || ':' || "); EMIT_MM(src); append_sql(ctx, " || ':' || "); EMIT_SS(src);
    append_sql(ctx, " || printf('.%%09d', _gql_extract_ns("); EMIT(src);
    append_sql(ctx, ") + COALESCE(json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.nanosecond'), 0))");

    append_sql(ctx, " ELSE '00:00' END");
    return 0;
}

/* After emit_truncate_time_portion, normalize redundant fractional zeros so
 *   '12:31:14.645000000' → '12:31:14.645'
 *   '12:31:14.645876000' → '12:31:14.645876'
 *   '12:31:14.000000000' → '12:31:14'
 * Only when sub-second is trailing-zero-padded (i.e., not for ns_override case
 * which we kept explicit). Implemented by rtrim('.0').
 *
 * To preserve override outputs verbatim (e.g., '.000000002'), we only trim
 * when the unit is not 'second' and not paired with an override. Simpler
 * approach: trim trailing zeros, then trim a trailing '.'.
 */
static int emit_trim_trailing_zeros(cypher_transform_context *ctx) {
    /* wrap the previously-emitted time expression in:
     *   CASE WHEN x LIKE '%.%' THEN rtrim(rtrim(x, '0'), '.') ELSE x END
     * To do this, we need to refer to the expression twice — caller should
     * have wrapped it in a CTE or repeated. We instead emit it as a noop here
     * and rely on the explicit ns_override handling in emit_truncate_time_portion
     * to avoid producing trailing zeros that need trimming.
     *
     * (See unit-specific cases above — they pad to exact ms/us/ns widths
     *  intentionally; subsequent post-processing is a separate phase.)
     */
    (void)ctx;
    return 0;
}

/* Emit tz portion. For localdatetime/localtime kinds caller skips this.
 *  - If map has 'timezone' override:
 *       For named zones (contains '/'), we map known names to offset and
 *       output 'offset[name]'.
 *       For offset/Z, output verbatim.
 *  - Else preserve src tz (_gql_extract_tz(src)), defaulting to 'Z'.
 */
static int emit_truncate_tz_portion(cypher_transform_context *ctx, ast_node *src, ast_node *map) {
    /* Known named tz → offset (rough; ignores DST). */
    append_sql(ctx, "CASE WHEN json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.timezone') IS NOT NULL THEN CASE WHEN instr(json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.timezone'), '/') > 0 THEN (CASE json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.timezone')"
        " WHEN 'Europe/Stockholm' THEN '+01:00'"
        " WHEN 'Europe/London' THEN '+00:00'"
        " WHEN 'Europe/Berlin' THEN '+01:00'"
        " WHEN 'America/New_York' THEN '-05:00'"
        " WHEN 'America/Los_Angeles' THEN '-08:00'"
        " WHEN 'Asia/Tokyo' THEN '+09:00'"
        " WHEN 'Asia/Shanghai' THEN '+08:00'"
        " WHEN 'Australia/Sydney' THEN '+10:00'"
        " ELSE '+00:00' END) || '[' || json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.timezone') || ']' ELSE json_extract(");
    if (emit_map_or_empty(ctx, map)<0) return -1;
    append_sql(ctx, ", '$.timezone') END ELSE COALESCE(NULLIF(_gql_extract_tz("); EMIT(src);
    append_sql(ctx, "), ''), 'Z') END");
    return 0;
}

/* Common implementation for datetime/localdatetime/time/localtime truncate. */
static int transform_datetime_truncate_impl(cypher_transform_context *ctx,
                                             cypher_function_call *func_call,
                                             bool emit_date, bool emit_tz) {
    if (!func_call->args || (func_call->args->count != 2 && func_call->args->count != 3)) {
        ctx->has_error = true;
        ctx->error_message = strdup("truncate() requires 2 or 3 arguments");
        return -1;
    }
    ast_node *unit = func_call->args->items[0];
    ast_node *src = func_call->args->items[1];
    ast_node *map = (func_call->args->count == 3) ? func_call->args->items[2] : NULL;

    append_sql(ctx, "(");
    if (emit_date) {
        append_sql(ctx, "(");
        if (emit_truncate_date_portion(ctx, unit, src, map) < 0) return -1;
        append_sql(ctx, ") || 'T' || ");
    }
    append_sql(ctx, "(");
    if (emit_truncate_time_portion(ctx, unit, src, map) < 0) return -1;
    append_sql(ctx, ")");
    if (emit_tz) {
        append_sql(ctx, " || (");
        if (emit_truncate_tz_portion(ctx, src, map) < 0) return -1;
        append_sql(ctx, ")");
    }
    append_sql(ctx, ")");
    (void)emit_trim_trailing_zeros;
    return 0;
}

int transform_datetime_truncate_function(cypher_transform_context *ctx, cypher_function_call *func_call) {
    return transform_datetime_truncate_impl(ctx, func_call, /*date*/true, /*tz*/true);
}
int transform_localdatetime_truncate_function(cypher_transform_context *ctx, cypher_function_call *func_call) {
    return transform_datetime_truncate_impl(ctx, func_call, /*date*/true, /*tz*/false);
}
int transform_time_truncate_function(cypher_transform_context *ctx, cypher_function_call *func_call) {
    return transform_datetime_truncate_impl(ctx, func_call, /*date*/false, /*tz*/true);
}
int transform_localtime_truncate_function(cypher_transform_context *ctx, cypher_function_call *func_call) {
    return transform_datetime_truncate_impl(ctx, func_call, /*date*/false, /*tz*/false);
}

#undef EMIT

/* Transform duration.between(temporal1, temporal2)
 * Returns the number of days between two dates as a duration-like value
 */
int transform_duration_between_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming duration.between function");

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("duration.between() requires two arguments");
        return -1;
    }

    /* Calendar-aware Duration: years/months from date diff with borrowing,
     * days/time from sub-month diff, ISO 8601 formatted by C helper. */
    append_sql(ctx, "json(_gql_duration_calendar(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, "))");

    return 0;
}

/* Transform duration.inSeconds/inDays/inMonths */
int transform_duration_in_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming duration.in* function: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() requires two arguments (temporal1, temporal2)",
                 func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* All variants delegate to a dedicated C helper that knows the
     * mixed-type rules (e.g. inDays of a localtime returns PT0S). */
    bool in_seconds = strcasecmp(func_call->function_name, "duration.inSeconds") == 0 ||
                      strcasecmp(func_call->function_name, "durationInSeconds") == 0;
    bool in_days    = strcasecmp(func_call->function_name, "duration.inDays") == 0 ||
                      strcasecmp(func_call->function_name, "durationInDays") == 0;
    const char *helper = in_seconds ? "_gql_duration_in_seconds"
                       : in_days    ? "_gql_duration_in_days"
                                    : "_gql_duration_in_months";
    append_sql(ctx, "json(%s(", helper);
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, "))");
    return 0;
    /* Dead code below — old SQL-based variant kept for reference. */
    if (in_seconds) {
        /* Total elapsed nanoseconds as a PT-form Duration. The C helper
         * formats hours/minutes/seconds and populates accessor fields. */
        append_sql(ctx, "json(_gql_duration_from_total_ns(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ")))");
    } else if (in_days) {
        /* Days-only duration. Compute integer days from ns diff, then build a
         * P<n>D ISO string. We keep months=0, seconds=0, nanos=0. */
        append_sql(ctx, "json_object('_iso8601', "
            "(CASE WHEN (_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000) = 0 THEN 'PT0S' ELSE printf('P%%dD', "
            "_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000) END), "
            "'months', 0, 'days', _gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000, 'seconds', 0, 'nanosecondsOfSecond', 0)");
    } else {
        /* duration.inMonths — approximate: total_days / 30.44. Renders as
         * PnYnM (or 'PT0S' when zero months). */
        append_sql(ctx, "json_object('_iso8601', "
            "(CASE WHEN CAST(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000.0 / 30.44 AS INTEGER) = 0 THEN 'PT0S' "
            "ELSE ('P' || "
            "CASE WHEN CAST(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000.0 / 30.44 AS INTEGER) / 12 != 0 THEN "
            "(CAST(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000.0 / 30.44 AS INTEGER) / 12) || 'Y' "
            "ELSE '' END || "
            "CASE WHEN CAST(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000.0 / 30.44 AS INTEGER) %% 12 != 0 THEN "
            "(CAST(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000.0 / 30.44 AS INTEGER) %% 12) || 'M' "
            "ELSE '' END) END), 'months', CAST(_gql_temporal_diff_ns(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, ") / 86400000000000.0 / 30.44 AS INTEGER), "
            "'days', 0, 'seconds', 0, 'nanosecondsOfSecond', 0)");
    }

    return 0;
}

/* Transform temporal arithmetic functions
 * dateAdd(temporal, duration_map) - add duration components to a date/datetime
 * dateSub(temporal, duration_map) - subtract duration components
 *
 * Example: dateAdd('2024-01-15', {days: 30, months: 2})
 * → datetime('2024-01-15', '+2 months', '+30 days')
 *
 * This is the functional form of `date + duration` since operator overloading
 * for temporal types is not feasible at transform time (can't detect types).
 */
int transform_date_add_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming dateAdd/dateSub function: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("dateAdd()/dateSub() requires two arguments: temporal and duration map");
        return -1;
    }

    bool is_sub = (strcasecmp(func_call->function_name, "dateSub") == 0 ||
                   strcasecmp(func_call->function_name, "datesub") == 0);
    const char *sign = is_sub ? "-" : "+";

    /* Generate: datetime(temporal,
     *   '+N years', '+N months', '+N days', '+N hours', '+N minutes', '+N seconds')
     * where N values come from json_extract on the duration map */
    append_sql(ctx, "datetime(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;

    /* Add each duration component as a SQLite date modifier */
    const char *components[] = {"years", "months", "days", "hours", "minutes", "seconds"};
    const char *modifiers[] = {"years", "months", "days", "hours", "minutes", "seconds"};

    for (int i = 0; i < 6; i++) {
        append_sql(ctx, ", '%s' || COALESCE(json_extract(json(", sign);
        if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
        append_sql(ctx, "), '$.%s'), 0) || ' %s'", components[i], modifiers[i]);
    }

    append_sql(ctx, ")");
    return 0;
}

/* Transform point() function
 * point({x, y}) — 2D Cartesian point (SRID 7203)
 * point({x, y, z}) — 3D Cartesian point
 * point({latitude, longitude}) — 2D geographic WGS-84 point (SRID 4326)
 * point({latitude, longitude, height}) — 3D geographic point
 *
 * Stored as JSON object with type metadata for distance calculations.
 */
int transform_point_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming point() function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("point() requires exactly one map argument");
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* Build a JSON object that preserves all input keys plus a _srid for distance calculations.
     * The point function detects Cartesian (x,y) vs Geographic (latitude,longitude) by key names.
     * We use json_object to normalize the representation. */
    append_sql(ctx, "(SELECT CASE "
               "WHEN json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.latitude') IS NOT NULL THEN json_object("
               "'srid', 4326, "
               "'latitude', json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.latitude'), "
               "'longitude', json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.longitude'), "
               "'height', json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.height')) "
               "ELSE json_object("
               "'srid', 7203, "
               "'x', json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.x'), "
               "'y', json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.y'), "
               "'z', json_extract(json(");
    if (transform_expression(ctx, arg) < 0) return -1;
    append_sql(ctx, "), '$.z')) END)");

    return 0;
}

/* Transform point.distance(p1, p2) / distance(p1, p2)
 * For Cartesian points (srid 7203): sqrt((x2-x1)^2 + (y2-y1)^2)
 * For Geographic points (srid 4326): haversine formula in meters
 */
int transform_point_distance_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming point.distance() function");

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("point.distance() requires exactly two point arguments");
        return -1;
    }

    /* Detect SRID from first point and use appropriate formula */
    append_sql(ctx, "(SELECT CASE WHEN json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.srid') = 4326 THEN "
               /* Haversine formula: 2 * R * asin(sqrt(hav(dlat) + cos(lat1)*cos(lat2)*hav(dlon))) */
               "6371000.0 * 2.0 * ASIN(SQRT("
               "((1.0 - COS((json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') - json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.latitude')) * 3.141592653589793 / 180.0)) / 2.0) + "
               "COS(json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') * 3.141592653589793 / 180.0) * "
               "COS(json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') * 3.141592653589793 / 180.0) * "
               "((1.0 - COS((json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.longitude') - json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.longitude')) * 3.141592653589793 / 180.0)) / 2.0)"
               ")) "
               "ELSE "
               /* Euclidean distance for Cartesian */
               "SQRT("
               "POWER(json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.x') - json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.x'), 2) + "
               "POWER(json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.y') - json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.y'), 2)"
               ") END)");

    return 0;
}

/* Transform point.withinBBox(point, lowerLeft, upperRight)
 * Returns true if point is within the bounding box defined by two corner points.
 */
int transform_point_within_bbox_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming point.withinBBox() function");

    if (!func_call->args || func_call->args->count != 3) {
        ctx->has_error = true;
        ctx->error_message = strdup("point.withinBBox() requires three arguments: point, lowerLeft, upperRight");
        return -1;
    }

    /* Check if geographic or Cartesian */
    append_sql(ctx, "(SELECT CASE WHEN json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.srid') = 4326 THEN ("
               "json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') >= json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') AND json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') <= json_extract(");
    if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    append_sql(ctx, ", '$.latitude') AND json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.longitude') >= json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.longitude') AND json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.longitude') <= json_extract(");
    if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    append_sql(ctx, ", '$.longitude'))"
               " ELSE ("
               "json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.x') >= json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.x') AND json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.x') <= json_extract(");
    if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    append_sql(ctx, ", '$.x') AND json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.y') >= json_extract(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ", '$.y') AND json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", '$.y') <= json_extract(");
    if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    append_sql(ctx, ", '$.y'))"
               " END)");

    return 0;
}

/* Transform json_get(expr, path) → json_extract(expr, '$.path') or json_extract(expr, path) */
int transform_json_get_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming json_get() function");

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("json_get() requires exactly two arguments: json_get(expr, path)");
        return -1;
    }

    append_sql(ctx, "json_extract(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", ");

    /* Check if path argument is a string literal starting with '$' */
    ast_node *path_arg = func_call->args->items[1];
    if (path_arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal*)path_arg;
        if (lit->literal_type == LITERAL_STRING && lit->value.string && lit->value.string[0] == '$') {
            /* Path already starts with $ — pass through directly */
            append_string_literal(ctx, lit->value.string);
            append_sql(ctx, ")");
            return 0;
        }
        if (lit->literal_type == LITERAL_STRING) {
            /* Simple key name — prepend $. */
            char path_buf[512];
            snprintf(path_buf, sizeof(path_buf), "$.%s", lit->value.string);
            append_string_literal(ctx, path_buf);
            append_sql(ctx, ")");
            return 0;
        }
    }

    /* Non-literal path — use string concatenation: '$.' || path */
    append_sql(ctx, "'$.' || ");
    if (transform_expression(ctx, path_arg) < 0) return -1;
    append_sql(ctx, ")");

    return 0;
}

/* Transform json_keys(expr) → (SELECT json_group_array(key) FROM json_each(expr)) */
int transform_json_keys_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming json_keys() function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("json_keys() requires exactly one argument");
        return -1;
    }

    append_sql(ctx, "(SELECT json_group_array(key) FROM json_each(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, "))");

    return 0;
}

/* Transform json_type(expr) → json_type(expr) */
int transform_json_type_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming json_type() function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("json_type() requires exactly one argument");
        return -1;
    }

    append_sql(ctx, "json_type(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ")");

    return 0;
}

/* nullIf(expr1, expr2) - return NULL if the two expressions are equal */
int transform_nullif_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming nullIf function");

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("nullIf() requires exactly two arguments");
        return -1;
    }

    append_sql(ctx, "NULLIF(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ", ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ")");

    return 0;
}

/* valueType(expr) - return the type name of an expression as a string */
int transform_valuetype_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming valueType function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("valueType() requires exactly one argument");
        return -1;
    }

    /* Map SQLite typeof() results to Cypher type names */
    append_sql(ctx, "(CASE typeof(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, ") WHEN 'integer' THEN 'INTEGER' WHEN 'real' THEN 'FLOAT' "
               "WHEN 'text' THEN 'STRING' WHEN 'null' THEN 'NULL' "
               "WHEN 'blob' THEN 'BLOB' ELSE 'ANY' END)");

    return 0;
}

/* isEmpty(expr) - check if a string, list, or map is empty */
int transform_isempty_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming isEmpty function");

    if (!func_call->args || func_call->args->count != 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("isEmpty() requires exactly one argument");
        return -1;
    }

    /* Generate: CASE
     *   WHEN typeof(expr) = 'text' THEN LENGTH(expr) = 0
     *   WHEN json_type(expr) = 'array' THEN json_array_length(expr) = 0
     *   WHEN json_type(expr) = 'object' THEN json_array_length(json_each(expr)) = 0
     *   ELSE expr IS NULL
     * END
     * Simplified: just use COALESCE(LENGTH(expr), 0) = 0 which works for strings,
     * and json_array_length for arrays. Use a simple approach:
     * (expr IS NULL OR LENGTH(CAST(expr AS TEXT)) = 0 OR expr = '[]' OR expr = '{}')
     */
    append_sql(ctx, "(COALESCE(LENGTH(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, "), 0) = 0)");

    return 0;
}
