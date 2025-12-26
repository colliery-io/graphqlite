/*
 * Unified Variable Tracking System Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "transform/transform_variables.h"

#define INITIAL_CAPACITY 16

/* Context lifecycle */

transform_var_context *transform_var_ctx_create(void)
{
    transform_var_context *ctx = calloc(1, sizeof(transform_var_context));
    if (!ctx) {
        return NULL;
    }

    ctx->vars = calloc(INITIAL_CAPACITY, sizeof(transform_var));
    if (!ctx->vars) {
        free(ctx);
        return NULL;
    }

    ctx->capacity = INITIAL_CAPACITY;
    ctx->count = 0;
    ctx->current_clause = 0;

    return ctx;
}

static void free_var(transform_var *var)
{
    if (!var) return;
    free(var->name);
    free(var->table_alias);
    free(var->label);
    free(var->cte_name);
    free(var->source_expr);
    /* path_elements is owned by AST, don't free */
}

void transform_var_ctx_free(transform_var_context *ctx)
{
    if (!ctx) return;

    for (int i = 0; i < ctx->count; i++) {
        free_var(&ctx->vars[i]);
    }
    free(ctx->vars);
    free(ctx);
}

void transform_var_ctx_reset(transform_var_context *ctx)
{
    if (!ctx) return;

    for (int i = 0; i < ctx->count; i++) {
        free_var(&ctx->vars[i]);
    }
    ctx->count = 0;
    ctx->current_clause = 0;
}

/* Internal: grow array if needed */
static int ensure_capacity(transform_var_context *ctx)
{
    if (ctx->count < ctx->capacity) {
        return 0;
    }

    int new_capacity = ctx->capacity * 2;
    transform_var *new_vars = realloc(ctx->vars, new_capacity * sizeof(transform_var));
    if (!new_vars) {
        return -1;
    }

    ctx->vars = new_vars;
    ctx->capacity = new_capacity;
    return 0;
}

/* Internal: find variable by name (returns index or -1) */
static int find_var_index(transform_var_context *ctx, const char *name)
{
    if (!ctx || !name) return -1;

    for (int i = 0; i < ctx->count; i++) {
        if (ctx->vars[i].name && strcmp(ctx->vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Variable registration */

int transform_var_register(transform_var_context *ctx,
                          const char *name,
                          var_kind kind,
                          const char *table_alias)
{
    if (!ctx || !name) return -1;

    /* Check if already exists - update if so */
    int idx = find_var_index(ctx, name);
    if (idx >= 0) {
        /* Update existing variable */
        transform_var *var = &ctx->vars[idx];
        var->kind = kind;
        var->is_visible = true;
        var->declared_in_clause = ctx->current_clause;

        if (table_alias) {
            char *new_alias = strdup(table_alias);
            if (!new_alias) return -1;
            free(var->table_alias);
            var->table_alias = new_alias;
        }
        return 0;
    }

    /* Add new variable */
    if (ensure_capacity(ctx) < 0) {
        return -1;
    }

    char *name_copy = strdup(name);
    char *alias_copy = table_alias ? strdup(table_alias) : NULL;

    if (!name_copy || (table_alias && !alias_copy)) {
        free(name_copy);
        free(alias_copy);
        return -1;
    }

    transform_var *var = &ctx->vars[ctx->count];
    memset(var, 0, sizeof(transform_var));

    var->name = name_copy;
    var->table_alias = alias_copy;
    var->kind = kind;
    var->declared_in_clause = ctx->current_clause;
    var->is_visible = true;
    var->is_bound = false;

    ctx->count++;
    return 0;
}

int transform_var_register_node(transform_var_context *ctx,
                               const char *name,
                               const char *table_alias,
                               const char *label)
{
    if (transform_var_register(ctx, name, VAR_KIND_NODE, table_alias) < 0) {
        return -1;
    }

    if (label) {
        int idx = find_var_index(ctx, name);
        if (idx >= 0) {
            char *label_copy = strdup(label);
            if (!label_copy) return -1;
            free(ctx->vars[idx].label);
            ctx->vars[idx].label = label_copy;
        }
    }

    return 0;
}

int transform_var_register_edge(transform_var_context *ctx,
                               const char *name,
                               const char *table_alias,
                               const char *type)
{
    if (transform_var_register(ctx, name, VAR_KIND_EDGE, table_alias) < 0) {
        return -1;
    }

    if (type) {
        int idx = find_var_index(ctx, name);
        if (idx >= 0) {
            char *type_copy = strdup(type);
            if (!type_copy) return -1;
            free(ctx->vars[idx].label);  /* Using label field for edge type */
            ctx->vars[idx].label = type_copy;
        }
    }

    return 0;
}

int transform_var_register_path(transform_var_context *ctx,
                               const char *name,
                               const char *table_alias,
                               ast_list *elements,
                               var_path_type path_type)
{
    if (transform_var_register(ctx, name, VAR_KIND_PATH, table_alias) < 0) {
        return -1;
    }

    int idx = find_var_index(ctx, name);
    if (idx >= 0) {
        ctx->vars[idx].path_elements = elements;
        ctx->vars[idx].path_type = path_type;
    }

    return 0;
}

int transform_var_register_projected(transform_var_context *ctx,
                                    const char *name,
                                    const char *source_expr)
{
    if (transform_var_register(ctx, name, VAR_KIND_PROJECTED, NULL) < 0) {
        return -1;
    }

    if (source_expr) {
        int idx = find_var_index(ctx, name);
        if (idx >= 0) {
            char *expr_copy = strdup(source_expr);
            if (!expr_copy) return -1;
            free(ctx->vars[idx].source_expr);
            ctx->vars[idx].source_expr = expr_copy;
        }
    }

    return 0;
}

/* Variable lookup */

transform_var *transform_var_lookup(transform_var_context *ctx,
                                   const char *name)
{
    int idx = find_var_index(ctx, name);
    if (idx < 0) return NULL;

    transform_var *var = &ctx->vars[idx];
    if (!var->is_visible) return NULL;

    return var;
}

transform_var *transform_var_lookup_node(transform_var_context *ctx,
                                        const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    if (!var || var->kind != VAR_KIND_NODE) return NULL;
    return var;
}

transform_var *transform_var_lookup_edge(transform_var_context *ctx,
                                        const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    if (!var || var->kind != VAR_KIND_EDGE) return NULL;
    return var;
}

transform_var *transform_var_lookup_path(transform_var_context *ctx,
                                        const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    if (!var || var->kind != VAR_KIND_PATH) return NULL;
    return var;
}

/* Convenience getters */

const char *transform_var_get_alias(transform_var_context *ctx,
                                   const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    if (!var) return NULL;

    /* For projected variables, source_expr contains the alias (e.g., "_with_0.name") */
    if (var->kind == VAR_KIND_PROJECTED && var->source_expr) {
        return var->source_expr;
    }
    return var->table_alias;
}

bool transform_var_is_edge(transform_var_context *ctx, const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    return var && var->kind == VAR_KIND_EDGE;
}

bool transform_var_is_path(transform_var_context *ctx, const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    return var && var->kind == VAR_KIND_PATH;
}

bool transform_var_is_projected(transform_var_context *ctx, const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    return var && var->kind == VAR_KIND_PROJECTED;
}

bool transform_var_is_bound(transform_var_context *ctx, const char *name)
{
    transform_var *var = transform_var_lookup(ctx, name);
    return var && var->is_bound;
}

/* Scope management */

void transform_var_enter_clause(transform_var_context *ctx)
{
    if (ctx) {
        ctx->current_clause++;
    }
}

void transform_var_exit_clause(transform_var_context *ctx)
{
    if (ctx && ctx->current_clause > 0) {
        ctx->current_clause--;
    }
}

void transform_var_mark_inherited(transform_var_context *ctx)
{
    if (!ctx) return;

    /* For now, just ensure all visible variables remain visible.
     * Future: could implement more complex scoping rules here */
}

int transform_var_project(transform_var_context *ctx,
                         const char **names,
                         int count)
{
    if (!ctx) return -1;

    /* Mark all variables as not visible */
    for (int i = 0; i < ctx->count; i++) {
        ctx->vars[i].is_visible = false;
    }

    /* Make only the projected variables visible */
    for (int i = 0; i < count; i++) {
        int idx = find_var_index(ctx, names[i]);
        if (idx >= 0) {
            ctx->vars[idx].is_visible = true;
        }
    }

    return 0;
}

/* Setters */

int transform_var_set_cte(transform_var_context *ctx,
                         const char *name,
                         const char *cte_name)
{
    int idx = find_var_index(ctx, name);
    if (idx < 0) return -1;

    char *cte_copy = strdup(cte_name);
    if (!cte_copy) return -1;

    free(ctx->vars[idx].cte_name);
    ctx->vars[idx].cte_name = cte_copy;
    return 0;
}

int transform_var_set_bound(transform_var_context *ctx,
                           const char *name,
                           bool is_bound)
{
    int idx = find_var_index(ctx, name);
    if (idx < 0) return -1;

    ctx->vars[idx].is_bound = is_bound;
    return 0;
}

/* Iteration */

int transform_var_count(transform_var_context *ctx)
{
    return ctx ? ctx->count : 0;
}

transform_var *transform_var_at(transform_var_context *ctx, int index)
{
    if (!ctx || index < 0 || index >= ctx->count) return NULL;
    return &ctx->vars[index];
}

/* Debug */

void transform_var_dump(transform_var_context *ctx)
{
    if (!ctx) {
        fprintf(stderr, "transform_var_ctx: NULL\n");
        return;
    }

    fprintf(stderr, "transform_var_ctx: %d variables, clause %d\n",
            ctx->count, ctx->current_clause);

    const char *kind_names[] = {
        "NODE", "EDGE", "PATH", "PROJECTED", "AGGREGATED"
    };

    for (int i = 0; i < ctx->count; i++) {
        transform_var *v = &ctx->vars[i];
        fprintf(stderr, "  [%d] %s: kind=%s alias=%s visible=%d bound=%d clause=%d\n",
                i, v->name ? v->name : "(null)",
                v->kind < 5 ? kind_names[v->kind] : "?",
                v->table_alias ? v->table_alias : "(null)",
                v->is_visible, v->is_bound, v->declared_in_clause);
    }
}
