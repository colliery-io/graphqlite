/*
 * transform_func_math.c
 *    Math function transformations for Cypher queries
 *
 * This file contains transformations for mathematical functions:
 * - abs, ceil, floor, sign, sqrt, log, log10, exp
 * - sin, cos, tan, asin, acos, atan
 * - round (with optional precision)
 * - rand, pi, e (no-argument functions)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Transform single-argument math functions */
int transform_math_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming math function: %s", func_call->function_name);

    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() function requires exactly one argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Map Cypher function names to SQLite function names */
    /* Note: SQLite has ABS, but for trig functions we need to compute them */
    const char *func_name = func_call->function_name;

    if (strcasecmp(func_name, "abs") == 0) {
        append_sql(ctx, "ABS(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL))");
        return 0;
    } else if (strcasecmp(func_name, "ceil") == 0) {
        /* SQLite doesn't have CEIL, use CASE to handle positive/negative */
        append_sql(ctx, "(CASE WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) = CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) THEN CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) > 0 THEN CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) + 1 ELSE CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) END)");
        return 0;
    } else if (strcasecmp(func_name, "floor") == 0) {
        /* SQLite doesn't have FLOOR, use CASE to handle positive/negative */
        append_sql(ctx, "(CASE WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) = CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) THEN CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) >= 0 THEN CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) ELSE CAST(CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) AS INTEGER) - 1 END)");
        return 0;
    } else if (strcasecmp(func_name, "sign") == 0) {
        /* SIGN(x) = CASE WHEN x > 0 THEN 1 WHEN x < 0 THEN -1 ELSE 0 END */
        append_sql(ctx, "(CASE WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) > 0 THEN 1 WHEN CAST(");
        if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
        append_sql(ctx, " AS REAL) < 0 THEN -1 ELSE 0 END)");
        return 0;
    } else if (strcasecmp(func_name, "sqrt") == 0) {
        append_sql(ctx, "SQRT(CAST(");
    } else if (strcasecmp(func_name, "log") == 0) {
        append_sql(ctx, "LN(CAST(");
    } else if (strcasecmp(func_name, "log10") == 0) {
        append_sql(ctx, "LOG10(CAST(");
    } else if (strcasecmp(func_name, "exp") == 0) {
        append_sql(ctx, "EXP(CAST(");
    } else if (strcasecmp(func_name, "sin") == 0) {
        append_sql(ctx, "SIN(CAST(");
    } else if (strcasecmp(func_name, "cos") == 0) {
        append_sql(ctx, "COS(CAST(");
    } else if (strcasecmp(func_name, "tan") == 0) {
        append_sql(ctx, "TAN(CAST(");
    } else if (strcasecmp(func_name, "asin") == 0) {
        append_sql(ctx, "ASIN(CAST(");
    } else if (strcasecmp(func_name, "acos") == 0) {
        append_sql(ctx, "ACOS(CAST(");
    } else if (strcasecmp(func_name, "atan") == 0) {
        append_sql(ctx, "ATAN(CAST(");
    } else {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown math function: %s", func_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, " AS REAL))");

    return 0;
}

/* Transform round function: round(x) or round(x, precision) */
int transform_round_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming round function");

    if (!func_call->args || func_call->args->count < 1 || func_call->args->count > 2) {
        ctx->has_error = true;
        ctx->error_message = strdup("round() requires 1 or 2 arguments: round(value) or round(value, precision)");
        return -1;
    }

    append_sql(ctx, "ROUND(CAST(");
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    append_sql(ctx, " AS REAL)");

    if (func_call->args->count == 2) {
        append_sql(ctx, ", ");
        if (transform_expression(ctx, func_call->args->items[1]) < 0) {
            return -1;
        }
    }

    append_sql(ctx, ")");
    return 0;
}

/* Transform no-argument functions: rand, pi, e */
int transform_noarg_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming no-arg function: %s", func_call->function_name);

    /* These functions should have 0 arguments */
    if (func_call->args && func_call->args->count > 0) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s() takes no arguments", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    if (strcasecmp(func_call->function_name, "rand") == 0 ||
        strcasecmp(func_call->function_name, "random") == 0) {
        /* SQLite random() returns int64, scale to [0,1) */
        append_sql(ctx, "(ABS(RANDOM()) / 9223372036854775807.0)");
    } else if (strcasecmp(func_call->function_name, "pi") == 0) {
        append_sql(ctx, "3.141592653589793");
    } else if (strcasecmp(func_call->function_name, "e") == 0) {
        append_sql(ctx, "2.718281828459045");
    } else {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown no-arg function: %s", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }

    return 0;
}
