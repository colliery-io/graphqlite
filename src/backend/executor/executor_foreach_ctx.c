/*
 * FOREACH Context Implementation
 * Manages variable bindings during FOREACH clause iteration
 */

#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "parser/cypher_ast.h"

/* Thread-local foreach context for nested property resolution */
__thread foreach_context *g_foreach_ctx = NULL;

foreach_context* create_foreach_context(void)
{
    foreach_context *ctx = calloc(1, sizeof(foreach_context));
    if (!ctx) return NULL;
    ctx->capacity = 4;
    ctx->bindings = calloc(ctx->capacity, sizeof(foreach_binding));
    if (!ctx->bindings) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

void free_foreach_context(foreach_context *ctx)
{
    if (!ctx) return;
    for (int i = 0; i < ctx->count; i++) {
        free(ctx->bindings[i].variable);
        if (ctx->bindings[i].literal_type == LITERAL_STRING && ctx->bindings[i].value.string) {
            free(ctx->bindings[i].value.string);
        }
    }
    free(ctx->bindings);
    free(ctx);
}

int set_foreach_binding_int(foreach_context *ctx, const char *variable, int64_t value)
{
    if (!ctx || !variable) return -1;

    /* Check if exists */
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->bindings[i].variable, variable) == 0) {
            if (ctx->bindings[i].literal_type == LITERAL_STRING && ctx->bindings[i].value.string) {
                free(ctx->bindings[i].value.string);
            }
            ctx->bindings[i].literal_type = LITERAL_INTEGER;
            ctx->bindings[i].value.integer = value;
            return 0;
        }
    }

    /* Add new */
    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->bindings = realloc(ctx->bindings, ctx->capacity * sizeof(foreach_binding));
        if (!ctx->bindings) return -1;
    }

    ctx->bindings[ctx->count].variable = strdup(variable);
    ctx->bindings[ctx->count].literal_type = LITERAL_INTEGER;
    ctx->bindings[ctx->count].value.integer = value;
    ctx->count++;
    return 0;
}

int set_foreach_binding_string(foreach_context *ctx, const char *variable, const char *value)
{
    if (!ctx || !variable) return -1;

    /* Check if exists */
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->bindings[i].variable, variable) == 0) {
            if (ctx->bindings[i].literal_type == LITERAL_STRING && ctx->bindings[i].value.string) {
                free(ctx->bindings[i].value.string);
            }
            ctx->bindings[i].literal_type = LITERAL_STRING;
            ctx->bindings[i].value.string = value ? strdup(value) : NULL;
            return 0;
        }
    }

    /* Add new */
    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->bindings = realloc(ctx->bindings, ctx->capacity * sizeof(foreach_binding));
        if (!ctx->bindings) return -1;
    }

    ctx->bindings[ctx->count].variable = strdup(variable);
    ctx->bindings[ctx->count].literal_type = LITERAL_STRING;
    ctx->bindings[ctx->count].value.string = value ? strdup(value) : NULL;
    ctx->count++;
    return 0;
}

foreach_binding* get_foreach_binding(foreach_context *ctx, const char *variable)
{
    if (!ctx || !variable) return NULL;
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->bindings[i].variable, variable) == 0) {
            return &ctx->bindings[i];
        }
    }
    return NULL;
}
