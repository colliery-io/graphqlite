/*
 * transform_func_typeconv.c
 *    Type conversion functions (toString, toInteger, toFloat,
 *    toBoolean, *OrNull variants, valueType, nullIf). Moved from
 *    transform_func_list.c (I-0040 M11).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_func_typeconv.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

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
                   arg->type == AST_NODE_NULL_CHECK ||
                   arg->type == AST_NODE_EXISTS_EXPR ||
                   arg->type == AST_NODE_LABEL_EXPR) {
            /* EXISTS { } and a label test are booleans too: toString of
             * them gave "1" (GQLITE-T-0513). */
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
        /* Boolean → text "true"/"false". Emit a plain CASE WHEN — the
         * result has no boolean subtype, so the JSON renderer correctly
         * quotes it. This used to be wrapped as ('"' || … || '"') so
         * the renderer's "starts with quote" fast-path passed it through,
         * but with M13 the subtype-based renderer no longer needs the
         * hack — strings without the boolean subtype are already
         * quoted/escaped. */
        append_sql(ctx, "(CASE WHEN ");
        if (transform_expression(ctx, arg) < 0) return -1;
        append_sql(ctx, " IS NULL THEN NULL WHEN ");
        if (transform_expression(ctx, arg) < 0) return -1;
        append_sql(ctx, " THEN 'true' ELSE 'false' END)");
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

