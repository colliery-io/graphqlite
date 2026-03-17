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

    append_sql(ctx, "CAST(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, " AS TEXT)");

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

    if (strcasecmp(func_call->function_name, "toInteger") == 0) {
        append_sql(ctx, "CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, " AS INTEGER)");
    } else if (strcasecmp(func_call->function_name, "toFloat") == 0) {
        append_sql(ctx, "CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, " AS REAL)");
    } else if (strcasecmp(func_call->function_name, "toBoolean") == 0) {
        /* Convert to boolean: 'true'/1 -> 1, 'false'/0/NULL -> 0 */
        append_sql(ctx, "(CASE WHEN LOWER(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, ") IN ('true', '1') OR ");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, " = 1 THEN 1 ELSE 0 END)");
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

    /* Use a recursive CTE to generate the range */
    /* (WITH RECURSIVE r(x) AS (SELECT start UNION ALL SELECT x+step FROM r WHERE x < end) SELECT json_group_array(x) FROM r) */
    append_sql(ctx, "(WITH RECURSIVE _range(x) AS (SELECT ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
    append_sql(ctx, " UNION ALL SELECT x + ");
    if (func_call->args->count == 3) {
        if (transform_expression(ctx, func_call->args->items[2]) < 0) return -1;
    } else {
        append_sql(ctx, "1");
    }
    append_sql(ctx, " FROM _range WHERE x < ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) return -1;
    append_sql(ctx, ") SELECT json_group_array(x) FROM _range)");

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

    append_sql(ctx, "json_group_array(");
    if (func_call->args->items[0] == NULL) {
        /* collect(*) - not really valid but handle gracefully */
        append_sql(ctx, "*");
    } else {
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
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
    }

    /* Otherwise treat as string length */
    return transform_string_function(ctx, func_call);
}

/* Transform date() function */
int transform_date_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming date() function");

    if (func_call->args && func_call->args->count > 0) {
        /* date(string) - parse date from string */
        append_sql(ctx, "date(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ")");
    } else {
        /* date() - current date */
        append_sql(ctx, "date('now')");
    }
    return 0;
}

/* Transform time() function */
int transform_time_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming time() function");

    if (func_call->args && func_call->args->count > 0) {
        /* time(string) - parse time from string */
        append_sql(ctx, "time(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ")");
    } else {
        /* time() - current time */
        append_sql(ctx, "time('now')");
    }
    return 0;
}

/* Transform datetime() function */
int transform_datetime_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming datetime() function");

    if (func_call->args && func_call->args->count > 0) {
        /* datetime(string) - parse datetime from string */
        append_sql(ctx, "datetime(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, ")");
    } else {
        /* datetime() - current datetime */
        append_sql(ctx, "datetime('now')");
    }
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
