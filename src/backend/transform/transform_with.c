/*
 * WITH Clause Transformation
 * Converts WITH clauses into SQL subqueries with variable projection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_internal.h"
#include "transform/transform_helpers.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/*
 * Walk an expression AST and validate every identifier reference resolves
 * to a variable currently in scope. Returns 0 if all OK, -1 if any
 * identifier is undefined (and sets ctx->error_message accordingly).
 * Used to enforce Cypher's UndefinedVariable semantics for ORDER BY in
 * WITH where variables drop out of scope at the projection boundary.
 */
/* Returns true if `name` is a Cypher aggregating function. */
static bool is_aggregating_function(const char *name)
{
    if (!name) return false;
    static const char *fns[] = {
        "count", "sum", "avg", "max", "min", "collect",
        "percentiledisc", "percentilecont", "stdev", "stdevp",
        NULL
    };
    for (int i = 0; fns[i]; i++) {
        size_t klen = strlen(fns[i]);
        if (strlen(name) == klen) {
            bool match = true;
            for (size_t j = 0; j < klen; j++) {
                char a = name[j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (a != fns[i][j]) { match = false; break; }
            }
            if (match) return true;
        }
    }
    return false;
}

/* Walk expr looking for an aggregating function call. Returns the name
 * of the first one found (or NULL). */
static const char *find_aggregating_call(ast_node *expr)
{
    if (!expr) return NULL;
    if (expr->type == AST_NODE_FUNCTION_CALL) {
        cypher_function_call *fc = (cypher_function_call*)expr;
        if (is_aggregating_function(fc->function_name)) return fc->function_name;
        if (fc->args) {
            for (int i = 0; i < fc->args->count; i++) {
                const char *r = find_aggregating_call(fc->args->items[i]);
                if (r) return r;
            }
        }
    } else if (expr->type == AST_NODE_BINARY_OP) {
        cypher_binary_op *b = (cypher_binary_op*)expr;
        const char *r = find_aggregating_call(b->left);
        if (r) return r;
        return find_aggregating_call(b->right);
    } else if (expr->type == AST_NODE_NOT_EXPR) {
        return find_aggregating_call(((cypher_not_expr*)expr)->expr);
    } else if (expr->type == AST_NODE_PROPERTY) {
        return find_aggregating_call(((cypher_property*)expr)->expr);
    } else if (expr->type == AST_NODE_NULL_CHECK) {
        return find_aggregating_call(((cypher_null_check*)expr)->expr);
    } else if (expr->type == AST_NODE_LIST) {
        cypher_list *l = (cypher_list*)expr;
        if (l->items) {
            for (int i = 0; i < l->items->count; i++) {
                const char *r = find_aggregating_call(l->items->items[i]);
                if (r) return r;
            }
        }
    }
    return NULL;
}

static int validate_identifiers_in_scope_ex(cypher_transform_context *ctx, ast_node *expr,
                                             char **extra, int extra_count)
{
    if (!expr) return 0;
    switch (expr->type) {
        case AST_NODE_IDENTIFIER: {
            cypher_identifier *id = (cypher_identifier*)expr;
            if (!id->name) return 0;
            if (transform_var_lookup(ctx->var_ctx, id->name)) return 0;
            for (int i = 0; i < extra_count; i++) {
                if (extra[i] && strcmp(extra[i], id->name) == 0) return 0;
            }
            ctx->has_error = true;
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Variable `%s` not defined (SyntaxError: UndefinedVariable)",
                     id->name);
            ctx->error_message = strdup(msg);
            return -1;
        }
        case AST_NODE_PROPERTY: {
            cypher_property *p = (cypher_property*)expr;
            return validate_identifiers_in_scope_ex(ctx, p->expr, extra, extra_count);
        }
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *b = (cypher_binary_op*)expr;
            if (validate_identifiers_in_scope_ex(ctx, b->left, extra, extra_count) < 0) return -1;
            return validate_identifiers_in_scope_ex(ctx, b->right, extra, extra_count);
        }
        case AST_NODE_NOT_EXPR: {
            cypher_not_expr *n = (cypher_not_expr*)expr;
            return validate_identifiers_in_scope_ex(ctx, n->expr, extra, extra_count);
        }
        case AST_NODE_NULL_CHECK: {
            cypher_null_check *nc = (cypher_null_check*)expr;
            return validate_identifiers_in_scope_ex(ctx, nc->expr, extra, extra_count);
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call*)expr;
            if (fc->args) {
                for (int i = 0; i < fc->args->count; i++) {
                    if (validate_identifiers_in_scope_ex(ctx, fc->args->items[i], extra, extra_count) < 0) return -1;
                }
            }
            return 0;
        }
        case AST_NODE_LIST: {
            cypher_list *l = (cypher_list*)expr;
            if (l->items) {
                for (int i = 0; i < l->items->count; i++) {
                    if (validate_identifiers_in_scope_ex(ctx, l->items->items[i], extra, extra_count) < 0) return -1;
                }
            }
            return 0;
        }
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *s = (cypher_subscript*)expr;
            if (validate_identifiers_in_scope_ex(ctx, s->expr, extra, extra_count) < 0) return -1;
            if (validate_identifiers_in_scope_ex(ctx, s->index, extra, extra_count) < 0) return -1;
            if (validate_identifiers_in_scope_ex(ctx, s->slice_start, extra, extra_count) < 0) return -1;
            if (validate_identifiers_in_scope_ex(ctx, s->slice_end, extra, extra_count) < 0) return -1;
            return 0;
        }
        default:
            return 0;
    }
}

static int validate_identifiers_in_scope(cypher_transform_context *ctx, ast_node *expr)
{
    return validate_identifiers_in_scope_ex(ctx, expr, NULL, 0);
}

/*
 * Transform an expression to a dynamically allocated string.
 * Uses a temporary buffer to capture output, then returns the result.
 * Caller must free the returned string.
 * Returns NULL on error.
 */
static char *transform_expression_to_string(cypher_transform_context *ctx, ast_node *expr)
{
    if (!ctx || !expr) return NULL;

    /* Save current buffer state */
    char *saved_buffer = ctx->sql_buffer;
    size_t saved_size = ctx->sql_size;
    size_t saved_capacity = ctx->sql_capacity;

    /* Allocate temporary buffer */
    size_t temp_capacity = 4096;
    char *temp_buffer = malloc(temp_capacity);
    if (!temp_buffer) return NULL;
    temp_buffer[0] = '\0';

    /* Switch to temporary buffer */
    ctx->sql_buffer = temp_buffer;
    ctx->sql_size = 0;
    ctx->sql_capacity = temp_capacity;

    /* Transform the expression */
    int result = transform_expression(ctx, expr);

    /* Capture the result */
    char *output = NULL;
    if (result == 0 && ctx->sql_size > 0) {
        output = strdup(ctx->sql_buffer);
    }

    /* Restore original buffer */
    free(temp_buffer);
    ctx->sql_buffer = saved_buffer;
    ctx->sql_size = saved_size;
    ctx->sql_capacity = saved_capacity;

    return output;
}

/*
 * Transform a WITH clause
 * WITH acts like an intermediate RETURN, projecting columns and optionally filtering
 * with a WHERE clause. The result becomes a CTE that subsequent clauses query from.
 */
int transform_with_clause(cypher_transform_context *ctx, cypher_with *with)
{
    CYPHER_DEBUG("Transforming WITH clause");

    if (!ctx || !with) {
        return -1;
    }

    /* Reset pending property JOINs for this WITH clause */
    reset_pending_prop_joins(ctx);

    /* GQLITE-T-0220: translate `with->where` in the PRE-WITH variable scope so
     * it can reference variables bound by prior MATCH/OPTIONAL MATCH that are
     * NOT in the projection (e.g. `WITH other WHERE r IS NULL` after an
     * `OPTIONAL MATCH (a)-[r]->(other)`). We embed the translated predicate in
     * the CTE body's WHERE below; the post-projection `with->where` path is
     * still used as a fallback for predicates that reference projected
     * aliases only. */
    char *with_where_pre = NULL;
    if (with->where) {
        with_where_pre = transform_expression_to_string(ctx, with->where);
        if (!with_where_pre && ctx->has_error) {
            /* Couldn't translate in pre-WITH scope (e.g. only projected
             * variables exist). Clear the error and try again post-projection
             * (the legacy path at the bottom of this function). */
            ctx->has_error = false;
            if (ctx->error_message) {
                free(ctx->error_message);
                ctx->error_message = NULL;
            }
        }
    }

    /* Generate a unique CTE name */
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_with_%d", ctx->with_cte_counter++);

    /*
     * Extract builder state directly instead of generating SQL and doing string manipulation.
     * This avoids the fragile "SELECT *" replacement pattern.
     */
    if (!ctx->unified_builder) {
        ctx->has_error = true;
        ctx->error_message = strdup("WITH clause requires a query builder context");
        return -1;
    }

    /* GQLITE-T-0219: WITH does NOT require a preceding MATCH in openCypher.
     * `WITH 1 AS x RETURN x`, `WITH 'hi' AS msg RETURN msg`, etc. are legal
     * (FROM-less projection). When no FROM has been built yet, emit a CTE of
     * the form `_with_N AS (SELECT <cols>)` with no FROM/JOIN/WHERE/GROUP BY. */
    bool no_from = !sql_builder_has_from(ctx->unified_builder);

    /* Extract FROM, JOINs, WHERE from builder (NULL/empty in no-FROM mode) */
    const char *from_clause = no_from ? NULL : sql_builder_get_from(ctx->unified_builder);
    const char *joins_clause = no_from ? NULL : sql_builder_get_joins(ctx->unified_builder);
    const char *where_clause = no_from ? NULL : sql_builder_get_where(ctx->unified_builder);
    const char *group_by_clause = no_from ? NULL : sql_builder_get_group_by(ctx->unified_builder);

    /* Save CTE buffer before reset */
    char *saved_cte = NULL;
    int saved_cte_count = 0;
    if (!dbuf_is_empty(&ctx->unified_builder->cte)) {
        saved_cte = strdup(dbuf_get(&ctx->unified_builder->cte));
        saved_cte_count = ctx->unified_builder->cte_count;
    }

    /* Build SELECT columns from WITH items */
    dynamic_buffer col_buf;
    dbuf_init(&col_buf);

    /* Track GROUP BY columns for aggregate handling */
    dynamic_buffer group_buf;
    dbuf_init(&group_buf);
    dynamic_buffer cte_body;
    bool has_aggregate = false;
    int group_count = 0;

    /* Handle WITH * - pass all visible variables through */
    if (with->pass_all) {
        CYPHER_DEBUG("Expanding WITH * with all bound variables");
        int var_count = transform_var_count(ctx->var_ctx);
        int added = 0;
        for (int vi = 0; vi < var_count; vi++) {
            transform_var *var = transform_var_at(ctx->var_ctx, vi);
            if (!var || !var->is_visible || !var->name) continue;

            if (added > 0) {
                dbuf_append(&col_buf, ", ");
                dbuf_append(&group_buf, ", ");
            }

            bool is_projected = (var->kind == VAR_KIND_PROJECTED);
            bool skip_id = var->alias_is_id || is_projected;
            const char *id_suffix = skip_id ? "" : ".id";

            { char idbuf[128];
              dbuf_appendf(&col_buf, "%s%s AS %s", var->table_alias, id_suffix,
                           sql_ident(idbuf, sizeof(idbuf), var->name)); }
            dbuf_appendf(&group_buf, "%s%s", var->table_alias, id_suffix);
            group_count++;
            added++;
        }
        goto with_star_columns_done;
    }

    for (int i = 0; i < with->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)with->items->items[i];

        if (i > 0) {
            dbuf_append(&col_buf, ", ");
        }

        /* Get the expression as SQL */
        if (item->expr->type == AST_NODE_IDENTIFIER) {
            cypher_identifier *id = (cypher_identifier*)item->expr;
            const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
            if (alias) {
                /* Determine column name */
                const char *col_name = item->alias ? item->alias : id->name;
                /* Check if alias IS already the id (projected or post-WITH node/edge) */
                bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
                bool alias_is_id = transform_var_alias_is_id(ctx->var_ctx, id->name);
                const char *id_suffix = (is_projected || alias_is_id) ? "" : ".id";
                { char idbuf[128];
                  dbuf_appendf(&col_buf, "%s%s AS %s", alias, id_suffix,
                               sql_ident(idbuf, sizeof(idbuf), col_name)); }
                /* Add to GROUP BY */
                if (group_count > 0) {
                    dbuf_append(&group_buf, ", ");
                }
                dbuf_appendf(&group_buf, "%s%s", alias, id_suffix);
                group_count++;
            } else {
                dbuf_append(&col_buf, id->name);
            }
        } else if (item->expr->type == AST_NODE_PROPERTY) {
            cypher_property *prop = (cypher_property*)item->expr;
            if (prop->expr->type == AST_NODE_IDENTIFIER) {
                cypher_identifier *id = (cypher_identifier*)prop->expr;
                const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
                const char *col_name = item->alias ? item->alias : prop->property_name;
                if (alias) {
                    /* Check if alias IS already the id (projected or post-WITH node/edge) */
                    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
                    bool alias_is_id = transform_var_alias_is_id(ctx->var_ctx, id->name);
                    const char *id_suffix = (is_projected || alias_is_id) ? "" : ".id";
                    /* Use COALESCE with all property tables - order int/real first to preserve type */
                    dbuf_appendf(&col_buf,
                        "(SELECT COALESCE("
                        "(SELECT npi.value FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = '%s'), "
                        "(SELECT npr.value FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = '%s'), "
                        "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = '%s'), "
                        "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = '%s')"
                        ")) AS %s",
                        alias, id_suffix, prop->property_name,
                        alias, id_suffix, prop->property_name,
                        alias, id_suffix, prop->property_name,
                        alias, id_suffix, prop->property_name,
                        ({ static char _idbuf[128]; sql_ident(_idbuf, sizeof(_idbuf), col_name); }));
                    /* Add the projected column name to GROUP BY (not node id) */
                    if (group_count > 0) {
                        dbuf_append(&group_buf, ", ");
                    }
                    dbuf_append(&group_buf, col_name);
                    group_count++;
                }
            }
        } else if (item->expr->type == AST_NODE_FUNCTION_CALL) {
            /* Handle function calls including aggregates */
            cypher_function_call *func = (cypher_function_call*)item->expr;
            const char *col_name = item->alias ? item->alias : func->function_name;

            /* Save current sql_buffer state */
            size_t saved_size = ctx->sql_size;
            char *saved_buffer = strdup(ctx->sql_buffer);
            if (!saved_buffer) {
                ctx->has_error = true;
                ctx->error_message = strdup("Memory allocation failed");
                dbuf_free(&col_buf);
                dbuf_free(&group_buf);
                free(saved_cte);
                return -1;
            }
            ctx->sql_size = 0;
            ctx->sql_buffer[0] = '\0';

            /* Transform the function call */
            if (transform_function_call(ctx, func) < 0) {
                free(saved_buffer);
                dbuf_free(&col_buf);
                dbuf_free(&group_buf);
                free(saved_cte);
                return -1;
            }

            /* Append to column buffer */
            { char _idbuf[128];
              dbuf_appendf(&col_buf, "%s AS %s", ctx->sql_buffer,
                           sql_ident(_idbuf, sizeof(_idbuf), col_name)); }

            /* Restore sql_buffer */
            ctx->sql_size = saved_size;
            strcpy(ctx->sql_buffer, saved_buffer);
            free(saved_buffer);

            /* Check if this is an aggregate function */
            if (strcasecmp(func->function_name, "count") == 0 ||
                strcasecmp(func->function_name, "sum") == 0 ||
                strcasecmp(func->function_name, "avg") == 0 ||
                strcasecmp(func->function_name, "min") == 0 ||
                strcasecmp(func->function_name, "max") == 0 ||
                strcasecmp(func->function_name, "collect") == 0) {
                has_aggregate = true;
            }
        } else {
            /* GQLITE-T-0225: any other expression type — let
             * transform_expression try. Common cases: list/map literals,
             * comparisons, unary ops, IS NULL, list comprehensions, etc.
             * If transform_expression doesn't know the type, it sets
             * ctx->error and we surface that to the user. */
            const char *col_name = item->alias;
            if (!col_name) {
                /* Generate a column name for expressions without alias */
                static char auto_col[32];
                snprintf(auto_col, sizeof(auto_col), "expr_%d", i);
                col_name = auto_col;
            }

            /* Save current sql_buffer state */
            size_t saved_size = ctx->sql_size;
            char *saved_buffer = strdup(ctx->sql_buffer);
            if (!saved_buffer) {
                ctx->has_error = true;
                ctx->error_message = strdup("Memory allocation failed");
                dbuf_free(&col_buf);
                dbuf_free(&group_buf);
                free(saved_cte);
                return -1;
            }
            ctx->sql_size = 0;
            ctx->sql_buffer[0] = '\0';

            /* Transform the expression */
            if (transform_expression(ctx, item->expr) < 0) {
                free(saved_buffer);
                dbuf_free(&col_buf);
                dbuf_free(&group_buf);
                free(saved_cte);
                return -1;
            }

            /* Append to column buffer */
            { char _idbuf[128];
              dbuf_appendf(&col_buf, "(%s) AS %s", ctx->sql_buffer,
                           sql_ident(_idbuf, sizeof(_idbuf), col_name)); }

            /* Restore sql_buffer */
            ctx->sql_size = saved_size;
            strcpy(ctx->sql_buffer, saved_buffer);
            free(saved_buffer);
        }
    }

with_star_columns_done:
    /* Build CTE body: SELECT <cols> FROM <from><joins> WHERE <where> GROUP BY <group> */
    dbuf_init(&cte_body);

    dbuf_append(&cte_body, "SELECT ");
    dbuf_append(&cte_body, dbuf_get(&col_buf));
    if (from_clause) {
        dbuf_append(&cte_body, " FROM ");
        dbuf_append(&cte_body, from_clause);
    }

    if (joins_clause) {
        dbuf_append(&cte_body, joins_clause);
    }

    /* Add pending property JOINs from aggregate functions */
    size_t pending_len = get_pending_prop_joins_len(ctx);
    if (pending_len > 0) {
        const char *pending_joins = get_pending_prop_joins(ctx);
        dbuf_append(&cte_body, pending_joins);
        CYPHER_DEBUG("WITH: Added property JOINs: %s", pending_joins);
        reset_pending_prop_joins(ctx);
    }

    if (where_clause || with_where_pre) {
        dbuf_append(&cte_body, " WHERE ");
        if (where_clause && with_where_pre) {
            dbuf_appendf(&cte_body, "(%s) AND (%s)", where_clause, with_where_pre);
        } else if (where_clause) {
            dbuf_append(&cte_body, where_clause);
        } else {
            dbuf_append(&cte_body, with_where_pre);
        }
    }

    /* Add GROUP BY if we have aggregates and non-aggregate columns */
    if (has_aggregate && group_count > 0) {
        dbuf_append(&cte_body, " GROUP BY ");
        dbuf_append(&cte_body, dbuf_get(&group_buf));
    } else if (group_by_clause) {
        /* Preserve existing GROUP BY from builder */
        dbuf_append(&cte_body, " GROUP BY ");
        dbuf_append(&cte_body, group_by_clause);
    }

    char *inner_sql = dbuf_finish(&cte_body);
    dbuf_free(&col_buf);
    dbuf_free(&group_buf);

    /* Reset builder and restore CTEs */
    sql_builder_reset(ctx->unified_builder);
    if (saved_cte) {
        dbuf_append(&ctx->unified_builder->cte, saved_cte);
        ctx->unified_builder->cte_count = saved_cte_count;
        free(saved_cte);
    }

    /* Add CTE to unified builder */
    sql_cte(ctx->unified_builder, cte_name, inner_sql, false);
    ctx->cte_count++;
    free(inner_sql);

    /* Before reset - save source variable kinds for simple identifiers
     * This preserves node/edge status so property lookups work after WITH */
    var_kind *source_kinds = NULL;
    int saved_var_count = 0;
    char **saved_var_names = NULL;
    var_kind *saved_var_kinds = NULL;

    if (with->pass_all) {
        /* WITH * - save all visible variable names and kinds */
        int total_vars = transform_var_count(ctx->var_ctx);
        saved_var_names = calloc(total_vars, sizeof(char*));
        saved_var_kinds = calloc(total_vars, sizeof(var_kind));
        if (saved_var_names && saved_var_kinds) {
            for (int vi = 0; vi < total_vars; vi++) {
                transform_var *var = transform_var_at(ctx->var_ctx, vi);
                if (var && var->is_visible && var->name) {
                    saved_var_names[saved_var_count] = strdup(var->name);
                    saved_var_kinds[saved_var_count] = var->kind;
                    saved_var_count++;
                }
            }
        }
    } else if (with->items && with->items->count > 0) {
        source_kinds = calloc(with->items->count, sizeof(var_kind));
        if (source_kinds) {
            for (int i = 0; i < with->items->count; i++) {
                cypher_return_item *item = (cypher_return_item*)with->items->items[i];
                if (item->expr->type == AST_NODE_IDENTIFIER) {
                    cypher_identifier *id = (cypher_identifier*)item->expr;
                    transform_var *var = transform_var_lookup(ctx->var_ctx, id->name);
                    if (var) {
                        source_kinds[i] = var->kind;
                    } else {
                        source_kinds[i] = VAR_KIND_PROJECTED;
                    }
                } else {
                    source_kinds[i] = VAR_KIND_PROJECTED;
                }
            }
        }
    }

    /* Snapshot the immediate pre-WITH variable names so the ORDER BY
     * validator below can accept references to them (Cypher allows ORDER
     * BY to see both the projection scope and the input scope of the
     * containing WITH). */
    int pre_var_count = 0;
    char **pre_var_names = NULL;
    {
        int total = transform_var_count(ctx->var_ctx);
        if (total > 0) {
            pre_var_names = calloc(total, sizeof(char*));
            if (pre_var_names) {
                for (int vi = 0; vi < total; vi++) {
                    transform_var *v = transform_var_at(ctx->var_ctx, vi);
                    if (v && v->name && v->is_visible) {
                        pre_var_names[pre_var_count++] = strdup(v->name);
                    }
                }
            }
        }
    }

    /* Clear old variables - WITH creates a new scope */
    transform_var_ctx_reset(ctx->var_ctx);

    /* Handle DISTINCT */
    if (with->distinct) {
        sql_distinct(ctx->unified_builder);
    }

    /* Set FROM to the CTE */
    sql_from(ctx->unified_builder, cte_name, NULL);

    if (with->pass_all) {
        /* WITH * - re-register all saved variables pointing at the CTE */
        for (int i = 0; i < saved_var_count; i++) {
            char select_expr[256];
            { char _idbuf[128];
              snprintf(select_expr, sizeof(select_expr), "%s.%s", cte_name,
                       sql_ident(_idbuf, sizeof(_idbuf), saved_var_names[i])); }

            var_kind kind = saved_var_kinds[i];
            if (kind == VAR_KIND_NODE) {
                transform_var_register_node(ctx->var_ctx, saved_var_names[i], select_expr, NULL);
                transform_var_set_alias_is_id(ctx->var_ctx, saved_var_names[i], true);
            } else if (kind == VAR_KIND_EDGE) {
                transform_var_register_edge(ctx->var_ctx, saved_var_names[i], select_expr, NULL);
                transform_var_set_alias_is_id(ctx->var_ctx, saved_var_names[i], true);
            } else {
                transform_var_register_projected(ctx->var_ctx, saved_var_names[i], select_expr);
            }
            free(saved_var_names[i]);
        }
        free(saved_var_names);
        free(saved_var_kinds);
    } else {
        /* Register new variables from WITH projections and build SELECT list */
        for (int i = 0; i < with->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)with->items->items[i];

            /* Determine the column name (alias or expression name) */
            const char *col_name = NULL;
            if (item->alias) {
                col_name = item->alias;
            } else if (item->expr->type == AST_NODE_IDENTIFIER) {
                col_name = ((cypher_identifier*)item->expr)->name;
            } else if (item->expr->type == AST_NODE_PROPERTY) {
                col_name = ((cypher_property*)item->expr)->property_name;
            } else if (item->expr->type == AST_NODE_FUNCTION_CALL) {
                col_name = ((cypher_function_call*)item->expr)->function_name;
            }

            if (col_name) {
                char select_expr[256];
                { char _idbuf[128];
                  snprintf(select_expr, sizeof(select_expr), "%s.%s", cte_name,
                           sql_ident(_idbuf, sizeof(_idbuf), col_name)); }

                var_kind kind = source_kinds ? source_kinds[i] : VAR_KIND_PROJECTED;
                if (kind == VAR_KIND_NODE) {
                    transform_var_register_node(ctx->var_ctx, col_name, select_expr, NULL);
                    transform_var_set_alias_is_id(ctx->var_ctx, col_name, true);
                    CYPHER_DEBUG("WITH: Registered node variable '%s' -> %s (alias_is_id)", col_name, select_expr);
                } else if (kind == VAR_KIND_EDGE) {
                    transform_var_register_edge(ctx->var_ctx, col_name, select_expr, NULL);
                    transform_var_set_alias_is_id(ctx->var_ctx, col_name, true);
                    CYPHER_DEBUG("WITH: Registered edge variable '%s' -> %s (alias_is_id)", col_name, select_expr);
                } else {
                    transform_var_register_projected(ctx->var_ctx, col_name, select_expr);
                    /* Mark vars whose projection is a scalar literal /
                     * scalar expression so MATCH can later reject
                     * `WITH 123 AS n MATCH (n)`. */
                    if (item->expr) {
                        bool is_scalar = false;
                        if (item->expr->type == AST_NODE_LITERAL) {
                            cypher_literal *lit = (cypher_literal*)item->expr;
                            if (lit->literal_type == LITERAL_INTEGER ||
                                lit->literal_type == LITERAL_DECIMAL ||
                                lit->literal_type == LITERAL_STRING  ||
                                lit->literal_type == LITERAL_BOOLEAN) {
                                is_scalar = true;
                            }
                        } else if (item->expr->type == AST_NODE_LIST ||
                                   item->expr->type == AST_NODE_MAP) {
                            is_scalar = true;
                        }
                        if (is_scalar) {
                            transform_var_set_scalar_value(ctx->var_ctx, col_name, true);
                        }
                    }
                    CYPHER_DEBUG("WITH: Registered projected variable '%s' -> %s", col_name, select_expr);
                }
            }
        }
    }

    /* Free saved kinds array */
    free(source_kinds);

    /* Handle WHERE clause (applied after projection) — only if the pre-WITH
     * translation didn't succeed (i.e., the WHERE references projected
     * aliases only, which weren't in scope earlier). */
    if (with->where && !with_where_pre) {
        char *where_str = transform_expression_to_string(ctx, with->where);
        if (where_str) {
            sql_where(ctx->unified_builder, where_str);
            free(where_str);
        } else {
            return -1;
        }
    }
    if (with_where_pre) {
        free(with_where_pre);
    }

    /* Handle ORDER BY */
    if (with->order_by && with->order_by->count > 0) {
        /* Validate: every identifier referenced in an ORDER BY expression
         * must be a projected variable. Cypher requires this — references
         * to variables that fall out of scope at WITH must raise
         * SyntaxError(UndefinedVariable). */
        /* Aggregating functions are not allowed in ORDER BY when the
         * containing WITH is not itself an aggregation. (Cypher TCK
         * scenarios in WithOrderBy2 [25] enforce this.) Reject before
         * the scope validation so we get an InvalidAggregation diagnostic
         * rather than an UndefinedVariable one. */
        {
            bool with_has_agg = false;
            if (with->items) {
                for (int i = 0; i < with->items->count; i++) {
                    cypher_return_item *it = (cypher_return_item*)with->items->items[i];
                    if (it && find_aggregating_call(it->expr)) {
                        with_has_agg = true;
                        break;
                    }
                }
            }
            if (!with_has_agg) {
                for (int i = 0; i < with->order_by->count; i++) {
                    cypher_order_by_item *order_item = (cypher_order_by_item*)with->order_by->items[i];
                    const char *agg = find_aggregating_call(order_item->expr);
                    if (agg) {
                        ctx->has_error = true;
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "SyntaxError: InvalidAggregation: aggregating function `%s` is not allowed in ORDER BY of a non-aggregating WITH",
                                 agg);
                        ctx->error_message = strdup(msg);
                        return -1;
                    }
                }
            }
        }
        for (int i = 0; i < with->order_by->count; i++) {
            cypher_order_by_item *order_item = (cypher_order_by_item*)with->order_by->items[i];
            if (validate_identifiers_in_scope_ex(ctx, order_item->expr,
                                                 pre_var_names, pre_var_count) < 0) {
                return -1;
            }
        }
        for (int i = 0; i < with->order_by->count; i++) {
            cypher_order_by_item *order_item = (cypher_order_by_item*)with->order_by->items[i];
            char *order_expr = transform_expression_to_string(ctx, order_item->expr);
            if (order_expr) {
                sql_order_by(ctx->unified_builder, order_expr, order_item->descending);
                free(order_expr);
            }
        }
    }

    /* Handle LIMIT and SKIP */
    int limit_val = -1;
    int offset_val = -1;

    if (with->limit) {
        char *limit_str = transform_expression_to_string(ctx, with->limit);
        if (limit_str) {
            limit_val = atoi(limit_str);
            free(limit_str);
        }
    }

    if (with->skip) {
        char *skip_str = transform_expression_to_string(ctx, with->skip);
        if (skip_str) {
            offset_val = atoi(skip_str);
            free(skip_str);
        }
    }

    if (limit_val >= 0 || offset_val >= 0) {
        sql_limit(ctx->unified_builder, limit_val, offset_val);
    }

    if (pre_var_names) {
        for (int i = 0; i < pre_var_count; i++) free(pre_var_names[i]);
        free(pre_var_names);
    }

    CYPHER_DEBUG("WITH clause generated CTE via unified builder: %s", cte_name);
    return 0;
}

