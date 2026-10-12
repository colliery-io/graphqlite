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

/* The binding for `variable`: the existing one with its old string freed,
 * or a new one. NULL on OOM. */
static foreach_binding *foreach_slot(foreach_context *ctx, const char *variable)
{
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->bindings[i].variable, variable) == 0) {
            if (ctx->bindings[i].literal_type == LITERAL_STRING && ctx->bindings[i].value.string) {
                free(ctx->bindings[i].value.string);
                ctx->bindings[i].value.string = NULL;
            }
            ctx->bindings[i].is_json = false;
            return &ctx->bindings[i];
        }
    }

    if (ctx->count >= ctx->capacity) {
        int capacity = ctx->capacity * 2;
        foreach_binding *grown = realloc(ctx->bindings, capacity * sizeof(foreach_binding));
        if (!grown) return NULL;
        ctx->bindings = grown;
        ctx->capacity = capacity;
    }

    foreach_binding *b = &ctx->bindings[ctx->count];
    b->variable = strdup(variable);
    if (!b->variable) return NULL;
    b->is_json = false;
    ctx->count++;
    return b;
}

int set_foreach_binding_int(foreach_context *ctx, const char *variable, int64_t value)
{
    if (!ctx || !variable) return -1;
    foreach_binding *b = foreach_slot(ctx, variable);
    if (!b) return -1;
    b->literal_type = LITERAL_INTEGER;
    b->value.integer = value;
    return 0;
}

int set_foreach_binding_string(foreach_context *ctx, const char *variable, const char *value)
{
    if (!ctx || !variable) return -1;
    foreach_binding *b = foreach_slot(ctx, variable);
    if (!b) return -1;
    b->literal_type = LITERAL_STRING;
    b->value.string = value ? strdup(value) : NULL;
    return 0;
}

int set_foreach_binding_json(foreach_context *ctx, const char *variable, const char *json)
{
    if (set_foreach_binding_string(ctx, variable, json) < 0) return -1;
    get_foreach_binding(ctx, variable)->is_json = true;
    return 0;
}

int set_foreach_binding_real(foreach_context *ctx, const char *variable, double value)
{
    if (!ctx || !variable) return -1;
    foreach_binding *b = foreach_slot(ctx, variable);
    if (!b) return -1;
    b->literal_type = LITERAL_DECIMAL;
    b->value.decimal = value;
    return 0;
}

int set_foreach_binding_bool(foreach_context *ctx, const char *variable, bool value)
{
    if (!ctx || !variable) return -1;
    foreach_binding *b = foreach_slot(ctx, variable);
    if (!b) return -1;
    b->literal_type = LITERAL_BOOLEAN;
    b->value.boolean = value;
    return 0;
}

int set_foreach_binding_null(foreach_context *ctx, const char *variable)
{
    if (!ctx || !variable) return -1;
    foreach_binding *b = foreach_slot(ctx, variable);
    if (!b) return -1;
    b->literal_type = LITERAL_NULL;
    return 0;
}

int bind_foreach_item_ast(foreach_context *ctx, const char *variable, ast_node *item)
{
    if (!item) return -1;
    if (item->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)item;
        switch (lit->literal_type) {
            case LITERAL_INTEGER: return set_foreach_binding_int(ctx, variable, lit->value.integer);
            case LITERAL_DECIMAL: return set_foreach_binding_real(ctx, variable, lit->value.decimal);
            case LITERAL_STRING:  return set_foreach_binding_string(ctx, variable, lit->value.string);
            case LITERAL_BOOLEAN: return set_foreach_binding_bool(ctx, variable, lit->value.boolean);
            case LITERAL_NULL:    return set_foreach_binding_null(ctx, variable);
        }
        return -1;
    }
    if (item->type == AST_NODE_MAP || item->type == AST_NODE_LIST) {
        char *json = serialize_ast_to_json(item);
        if (!json) return -1;
        int rc = set_foreach_binding_json(ctx, variable, json);
        free(json);
        return rc;
    }
    return -1;
}

int bind_foreach_item_json_each(foreach_context *ctx, const char *variable,
                                sqlite3_stmt *stmt, int value_col, int type_col)
{
    const char *type = (const char *)sqlite3_column_text(stmt, type_col);
    if (!type || strcmp(type, "null") == 0) return set_foreach_binding_null(ctx, variable);
    if (strcmp(type, "true") == 0)    return set_foreach_binding_bool(ctx, variable, true);
    if (strcmp(type, "false") == 0)   return set_foreach_binding_bool(ctx, variable, false);
    if (strcmp(type, "integer") == 0) return set_foreach_binding_int(ctx, variable, sqlite3_column_int64(stmt, value_col));
    if (strcmp(type, "real") == 0)    return set_foreach_binding_real(ctx, variable, sqlite3_column_double(stmt, value_col));
    /* objects and arrays come back as JSON text */
    const char *value = (const char *)sqlite3_column_text(stmt, value_col);
    if (strcmp(type, "object") == 0 || strcmp(type, "array") == 0) {
        return set_foreach_binding_json(ctx, variable, value);
    }
    return set_foreach_binding_string(ctx, variable, value);
}

int json_map_property(sqlite3 *db, const char *json, const char *key,
                      property_type *out_type, property_value *out)
{
    if (!db || !json || !key) return -1;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT json_type(?1, '$.\"' || ?2 || '\"'), json_extract(?1, '$.\"' || ?2 || '\"') "
            "WHERE json_valid(?1) AND json_type(?1) = 'object'",
            -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC);

    int rc = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *type = (const char *)sqlite3_column_text(stmt, 0);
        if (!type || strcmp(type, "null") == 0) {
            rc = -2;
        } else if (strcmp(type, "true") == 0 || strcmp(type, "false") == 0) {
            *out_type = PROP_TYPE_BOOLEAN;
            out->as_bool = (type[0] == 't');
            rc = 0;
        } else if (strcmp(type, "integer") == 0) {
            *out_type = PROP_TYPE_INTEGER;
            out->as_int = sqlite3_column_int64(stmt, 1);
            rc = 0;
        } else if (strcmp(type, "real") == 0) {
            *out_type = PROP_TYPE_REAL;
            out->as_real = sqlite3_column_double(stmt, 1);
            rc = 0;
        } else {
            const char *text = (const char *)sqlite3_column_text(stmt, 1);
            *out_type = strcmp(type, "text") == 0 ? PROP_TYPE_TEXT : PROP_TYPE_JSON;
            out->as_str = strdup(text ? text : "");
            out->as_str_len = out->as_str ? strlen(out->as_str) : 0;
            rc = out->as_str ? 0 : -1;
        }
    }
    sqlite3_finalize(stmt);
    return rc;
}

int foreach_map_property(sqlite3 *db, foreach_context *ctx, ast_node *expr,
                         property_type *out_type, property_value *out)
{
    if (!db || !ctx || !expr || expr->type != AST_NODE_PROPERTY) return -1;
    cypher_property *prop = (cypher_property *)expr;
    if (!prop->expr || prop->expr->type != AST_NODE_IDENTIFIER || !prop->property_name) return -1;
    foreach_binding *b = get_foreach_binding(ctx, ((cypher_identifier *)prop->expr)->name);
    if (!b) return -1;
    if (b->literal_type == LITERAL_NULL) return -2;
    /* Any text that is a JSON object: FOREACH over an evaluated list binds
     * maps as text without is_json. json_map_property checks validity. */
    if (b->literal_type != LITERAL_STRING || !b->value.string) return -1;
    return json_map_property(db, b->value.string, prop->property_name, out_type, out);
}

int foreach_value(sqlite3 *db, foreach_context *ctx, ast_node *expr,
                  property_type *out_type, property_value *out)
{
    if (!ctx || !expr) return -1;
    if (expr->type == AST_NODE_PROPERTY) {
        return foreach_map_property(db, ctx, expr, out_type, out);
    }
    if (expr->type != AST_NODE_IDENTIFIER) return -1;
    foreach_binding *b = get_foreach_binding(ctx, ((cypher_identifier *)expr)->name);
    if (!b) return -1;
    switch (b->literal_type) {
        case LITERAL_INTEGER:
            *out_type = PROP_TYPE_INTEGER;
            out->as_int = b->value.integer;
            return 0;
        case LITERAL_DECIMAL:
            *out_type = PROP_TYPE_REAL;
            out->as_real = b->value.decimal;
            return 0;
        case LITERAL_BOOLEAN:
            *out_type = PROP_TYPE_BOOLEAN;
            out->as_bool = b->value.boolean ? 1 : 0;
            return 0;
        case LITERAL_STRING:
            if (!b->value.string) return -2;
            *out_type = b->is_json ? PROP_TYPE_JSON : PROP_TYPE_TEXT;
            out->as_str = strdup(b->value.string);
            out->as_str_len = out->as_str ? strlen(out->as_str) : 0;
            return out->as_str ? 0 : -1;
        default:
            return -2;
    }
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
