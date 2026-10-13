/*
 * transform_func_json.c
 *    JSON inspection helpers (json_get, json_keys, json_type) and
 *    isEmpty(). Moved from transform_func_list.c (I-0040 M10).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_func_json.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

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
    /* A list or map is empty by its JSON length (LENGTH('[]') is 2, so
     * isEmpty([]) was false); null gives null (GQLITE-T-0408). */
    ast_node *arg = func_call->args->items[0];
    char *x = cypher_transform_capture_expression(ctx, arg);
    if (!x) return -1;
    append_sql(ctx,
        "(CASE WHEN (%s) IS NULL THEN NULL "
        "WHEN json_valid(%s) AND json_type(%s) = 'array' THEN json_array_length(%s) = 0 "
        "WHEN json_valid(%s) AND json_type(%s) = 'object' THEN (SELECT count(*) FROM json_each(%s)) = 0 "
        "ELSE LENGTH(%s) = 0 END)", x, x, x, x, x, x, x, x);
    free(x);

    return 0;
}
