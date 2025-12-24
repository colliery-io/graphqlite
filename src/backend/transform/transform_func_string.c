/*
 * transform_func_string.c
 *    String function transformations for Cypher queries
 *
 * This file contains transformations for string manipulation functions:
 * - toUpper, toLower, trim, ltrim, rtrim, length, size, reverse
 * - substring
 * - replace
 * - split
 * - left, right
 * - startsWith, endsWith, contains
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Transform basic string functions (single argument, direct SQL mapping) */
int transform_string_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming string function: %s", func_call->function_name);

    /* These functions require exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() function requires exactly one argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Map Cypher function names to SQLite function names */
    const char *sql_func = NULL;
    if (strcasecmp(func_call->function_name, "toUpper") == 0) {
        sql_func = "UPPER";
    } else if (strcasecmp(func_call->function_name, "toLower") == 0) {
        sql_func = "LOWER";
    } else if (strcasecmp(func_call->function_name, "trim") == 0) {
        sql_func = "TRIM";
    } else if (strcasecmp(func_call->function_name, "ltrim") == 0) {
        sql_func = "LTRIM";
    } else if (strcasecmp(func_call->function_name, "rtrim") == 0) {
        sql_func = "RTRIM";
    } else if (strcasecmp(func_call->function_name, "length") == 0 ||
               strcasecmp(func_call->function_name, "size") == 0) {
        sql_func = "LENGTH";
    } else if (strcasecmp(func_call->function_name, "reverse") == 0) {
        sql_func = "REVERSE";
    } else {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown string function: %s", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    append_sql(ctx, "%s(", sql_func);
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, ")");

    return 0;
}

/* Transform substring function: substring(str, start) or substring(str, start, length) */
int transform_substring_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming substring function");

    /* substring requires 2 or 3 arguments */
    if (!func_call->args || (func_call->args->count != 2 && func_call->args->count != 3)) {
        ctx->has_error = true;
        ctx->error_message = strdup("substring() requires 2 or 3 arguments: substring(string, start) or substring(string, start, length)");
        return -1;
    }

    /* SQLite SUBSTR is 1-based, Cypher substring is 0-based - add 1 to start */
    append_sql(ctx, "SUBSTR(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, ", (");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) {
        return -1;
    }
    append_sql(ctx, ") + 1");  /* Convert 0-based to 1-based */

    if (func_call->args->count == 3) {
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[2]) < 0) {
            return -1;
        }
    }

    append_sql(ctx, ")");
    return 0;
}

/* Transform replace function: replace(str, search, replacement) */
int transform_replace_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming replace function");

    if (!func_call->args || func_call->args->count != 3) {
        ctx->has_error = true;
        ctx->error_message = strdup("replace() requires 3 arguments: replace(string, search, replacement)");
        return -1;
    }

    append_sql(ctx, "REPLACE(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, ", ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) {
        return -1;
    }
    append_sql(ctx, ", ");
    if (transform_expression(ctx, func_call->args->items[2]) < 0) {
        return -1;
    }
    append_sql(ctx, ")");

    return 0;
}

/* Transform split function: split(str, delimiter) -> returns JSON array */
int transform_split_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming split function");

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("split() requires 2 arguments: split(string, delimiter)");
        return -1;
    }

    /* Use json_each with recursive splitting via instr */
    /* For simplicity, use a subquery that splits into JSON array */
    append_sql(ctx, "(SELECT json_group_array(value) FROM (WITH RECURSIVE split_cte(remaining, value) AS (");
    append_sql(ctx, "SELECT ");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, " || ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) {
        return -1;
    }
    append_sql(ctx, ", '' UNION ALL SELECT SUBSTR(remaining, INSTR(remaining, ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) {
        return -1;
    }
    append_sql(ctx, ") + LENGTH(");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) {
        return -1;
    }
    append_sql(ctx, ")), SUBSTR(remaining, 1, INSTR(remaining, ");
    if (transform_expression(ctx, func_call->args->items[1]) < 0) {
        return -1;
    }
    append_sql(ctx, ") - 1) FROM split_cte WHERE remaining != ''");
    append_sql(ctx, ") SELECT value FROM split_cte WHERE value != ''))");

    return 0;
}

/* Transform left/right functions: left(str, n) or right(str, n) */
int transform_leftright_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming left/right function: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() requires 2 arguments: %s(string, length)",
                func_call->function_name, func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    if (strcasecmp(func_call->function_name, "left") == 0) {
        /* LEFT(str, n) = SUBSTR(str, 1, n) */
        append_sql(ctx, "SUBSTR(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, ", 1, ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) {
            return -1;
        }
        append_sql(ctx, ")");
    } else {
        /* RIGHT(str, n) = SUBSTR(str, -n) */
        append_sql(ctx, "SUBSTR(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, ", -(");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) {
            return -1;
        }
        append_sql(ctx, "))");
    }

    return 0;
}

/* Transform pattern matching functions: startsWith, endsWith, contains */
int transform_pattern_match_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming pattern match function: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 2) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() requires 2 arguments: %s(string, pattern)",
                func_call->function_name, func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    append_sql(ctx, "(");

    if (strcasecmp(func_call->function_name, "startsWith") == 0) {
        /* startsWith(str, prefix) -> str LIKE prefix || '%' */
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, " LIKE ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) {
            return -1;
        }
        append_sql(ctx, " || '%%'");
    } else if (strcasecmp(func_call->function_name, "endsWith") == 0) {
        /* endsWith(str, suffix) -> str LIKE '%' || suffix */
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, " LIKE '%%' || ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) {
            return -1;
        }
    } else if (strcasecmp(func_call->function_name, "contains") == 0) {
        /* contains(str, substr) -> INSTR(str, substr) > 0 */
        append_sql(ctx, "INSTR(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) {
            return -1;
        }
        append_sql(ctx, ") > 0");
    }

    append_sql(ctx, ")");
    return 0;
}
