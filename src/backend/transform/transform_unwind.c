/*
 * UNWIND Clause Transformation
 * Converts UNWIND clauses that expand lists into rows
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/* Declared in executor/executor_internal.h. Re-declared here to avoid a
 * cross-layer include from transform → executor internals. */
extern char *serialize_ast_to_json(ast_node *expr);

/**
 * Transform UNWIND clause - expands list into rows
 *
 * UNWIND [1, 2, 3] AS x RETURN x
 * ->
 * WITH _unwind_0 AS (SELECT 1 AS value UNION ALL SELECT 2 UNION ALL SELECT 3)
 * SELECT _unwind_0.value AS x FROM _unwind_0
 */
int transform_unwind_clause(cypher_transform_context *ctx, cypher_unwind *unwind)
{
    CYPHER_DEBUG("Transforming UNWIND clause");

    if (!ctx || !unwind || !unwind->alias) {
        ctx->has_error = true;
        ctx->error_message = strdup("UNWIND requires expression and alias");
        return -1;
    }

    /* Generate unique CTE name */
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_unwind_%d", ctx->unwind_cte_counter++);

    /* GQLITE-T-0228 family: capture projected variables that are currently
     * in scope so we can carry them through the UNWIND CTE. Without this,
     * `WITH 1 AS a UNWIND [1,2] AS x RETURN a, x` errors out because the
     * UNWIND drops `a` from the var context. */
    char *carry_names[64];
    char *carry_aliases[64];
    int carry_count = 0;
    {
        int n = transform_var_count(ctx->var_ctx);
        for (int i = 0; i < n && carry_count < 64; i++) {
            transform_var *v = transform_var_at(ctx->var_ctx, i);
            if (!v || !v->name) continue;
            if (!v->is_visible) continue;
            if (v->kind != VAR_KIND_PROJECTED) continue;
            /* Projected vars store the source expression here (e.g.
             * "_with_0.msg"); table_alias may be NULL for them. */
            const char *alias = v->source_expr ? v->source_expr : v->table_alias;
            if (!alias) continue;
            carry_names[carry_count]   = strdup(v->name);
            carry_aliases[carry_count] = strdup(alias);
            carry_count++;
        }
    }

    /*
     * Build inner SQL from builder state directly.
     * This avoids the SELECT * pattern - we explicitly build the subquery.
     */
    char *inner_sql = NULL;
    char *saved_cte = NULL;
    int saved_cte_count = 0;

    if (ctx->unified_builder && sql_builder_has_from(ctx->unified_builder)) {
        /* Extract builder state */
        const char *from_clause = sql_builder_get_from(ctx->unified_builder);
        const char *joins_clause = sql_builder_get_joins(ctx->unified_builder);
        const char *where_clause = sql_builder_get_where(ctx->unified_builder);

        /* Build subquery: SELECT * FROM <from><joins> WHERE <where> */
        dynamic_buffer subquery;
        dbuf_init(&subquery);

        dbuf_append(&subquery, "SELECT * FROM ");
        dbuf_append(&subquery, from_clause);

        if (joins_clause) {
            dbuf_append(&subquery, joins_clause);
        }

        if (where_clause) {
            dbuf_append(&subquery, " WHERE ");
            dbuf_append(&subquery, where_clause);
        }

        inner_sql = dbuf_finish(&subquery);

        /* Save CTE buffer before reset */
        if (!dbuf_is_empty(&ctx->unified_builder->cte)) {
            saved_cte = strdup(dbuf_get(&ctx->unified_builder->cte));
            saved_cte_count = ctx->unified_builder->cte_count;
        }

        sql_builder_reset(ctx->unified_builder);

        /* Restore CTE buffer after reset */
        if (saved_cte) {
            dbuf_append(&ctx->unified_builder->cte, saved_cte);
            ctx->unified_builder->cte_count = saved_cte_count;
            free(saved_cte);
        }
    }

    /* Build CTE query in a local buffer */
    dynamic_buffer cte_query;
    dbuf_init(&cte_query);

    /* Pre-build a "_prev carry projection" snippet to splice into each
     * UNION arm when prior projected variables exist. Looks like:
     *   ", _prev.a AS a, _prev.b AS b"
     * Without prior projected vars OR an inner_sql to source them from,
     * this stays empty and behavior is unchanged. */
    dynamic_buffer carry_cols;
    dbuf_init(&carry_cols);
    /* Alternate carry-cols form for the function-call / subscript /
     * binary-op branch, which splices the inner FROM tables directly
     * (so var refs in the unwound expression stay in scope) and thus
     * cannot reference `_prev` — uses the original `<table>.<col>`
     * alias instead. T-0305. */
    dynamic_buffer carry_cols_orig;
    dbuf_init(&carry_cols_orig);
    bool has_carry = (carry_count > 0 && inner_sql && strlen(inner_sql) > 0);
    if (has_carry) {
        for (int i = 0; i < carry_count; i++) {
            /* Projected variables' table_alias is typically `<cte>.<col>`;
             * we just need the column name when sourcing from _prev. */
            const char *col = strrchr(carry_aliases[i], '.');
            col = col ? col + 1 : carry_aliases[i];
            { char _ib1[128], _ib2[128];
              dbuf_appendf(&carry_cols, ", _prev.%s AS %s",
                           sql_ident(_ib1, sizeof(_ib1), col),
                           sql_ident(_ib2, sizeof(_ib2), carry_names[i])); }
            { char _ib2[128];
              dbuf_appendf(&carry_cols_orig, ", %s AS %s",
                           carry_aliases[i],
                           sql_ident(_ib2, sizeof(_ib2), carry_names[i])); }
        }
    }

    /* UNWIND null AS x → zero rows (openCypher spec). Emit an empty
     * CTE; the common tail below handles var registration and FROM. */
    if (unwind->expr->type == AST_NODE_LITERAL &&
        ((cypher_literal *)unwind->expr)->literal_type == LITERAL_NULL) {
        dbuf_append(&cte_query, "SELECT NULL AS value");
        if (has_carry) dbuf_append(&cte_query, dbuf_get(&carry_cols));
        dbuf_append(&cte_query, " WHERE 0");
    } else if (unwind->expr->type == AST_NODE_LIST) {
        /* List literal: use UNION ALL approach */
        cypher_list *list = (cypher_list*)unwind->expr;

        if (!list->items || list->items->count == 0) {
            /* Empty list: return no rows using impossible condition */
            dbuf_append(&cte_query, "SELECT NULL AS value");
            if (has_carry) dbuf_append(&cte_query, dbuf_get(&carry_cols));
            dbuf_append(&cte_query, " WHERE 0");
        } else {
            for (int i = 0; i < list->items->count; i++) {
                if (i > 0) {
                    dbuf_append(&cte_query, " UNION ALL ");
                }
                dbuf_append(&cte_query, "SELECT ");

                /* Transform the expression to get its SQL value */
                ast_node *item = list->items->items[i];
                if (item->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)item;
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&cte_query, "%lld", (long long)lit->value.integer);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&cte_query, "%.17g", lit->value.decimal);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&cte_query, "'%s'", lit->value.string ? lit->value.string : "");
                            break;
                        case LITERAL_BOOLEAN:
                            /* Emit as 'true'/'false' string so downstream
                             * output formatting renders JSON booleans
                             * (matches the convention used by boolean
                             * property access). */
                            dbuf_append(&cte_query, lit->value.boolean ? "'true'" : "'false'");
                            break;
                        case LITERAL_NULL:
                            dbuf_append(&cte_query, "NULL");
                            break;
                    }
                } else if (item->type == AST_NODE_MAP || item->type == AST_NODE_LIST) {
                    /* Nested map/list item — serialize to JSON and emit as a
                     * JSON literal so downstream property access can use
                     * json_extract(_unwind_N.value, '$.key') (GQLITE-T-0185). */
                    char *json_str = serialize_ast_to_json(item);
                    if (json_str) {
                        dbuf_append(&cte_query, "json('");
                        for (const char *p = json_str; *p; p++) {
                            if (*p == '\'') dbuf_append(&cte_query, "''");
                            else dbuf_appendf(&cte_query, "%c", *p);
                        }
                        dbuf_append(&cte_query, "')");
                        free(json_str);
                    } else {
                        dbuf_append(&cte_query, "NULL");
                    }
                } else {
                    /* General expression (function call, identifier, etc.) —
                     * transform to SQL and emit inline. */
                    char *saved_buffer = ctx->sql_buffer;
                    size_t saved_size = ctx->sql_size;
                    size_t saved_capacity = ctx->sql_capacity;
                    size_t temp_capacity = 4096;
                    char *temp_buffer = malloc(temp_capacity);
                    bool ok = false;
                    if (temp_buffer) {
                        temp_buffer[0] = '\0';
                        ctx->sql_buffer = temp_buffer;
                        ctx->sql_size = 0;
                        ctx->sql_capacity = temp_capacity;
                        if (transform_expression(ctx, item) == 0 && ctx->sql_buffer[0]) {
                            dbuf_appendf(&cte_query, "(%s)", ctx->sql_buffer);
                            ok = true;
                        }
                        /* append_sql may realloc; free what ctx->sql_buffer
                         * now points to, not the original temp_buffer. */
                        free(ctx->sql_buffer);
                        ctx->sql_buffer = saved_buffer;
                        ctx->sql_size = saved_size;
                        ctx->sql_capacity = saved_capacity;
                    }
                    if (!ok) dbuf_append(&cte_query, "NULL");
                }
                dbuf_append(&cte_query, " AS value");
                if (has_carry) {
                    dbuf_append(&cte_query, dbuf_get(&carry_cols));
                    dbuf_appendf(&cte_query, " FROM (%s) AS _prev", inner_sql);
                }
            }
        }
    } else if (unwind->expr->type == AST_NODE_PROPERTY) {
        /* Property access: assume JSON array, use json_each */
        cypher_property *prop = (cypher_property*)unwind->expr;

        if (prop->expr->type == AST_NODE_IDENTIFIER) {
            cypher_identifier *id = (cypher_identifier*)prop->expr;
            const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
            bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);

            /* Build json_each on the property value */
            dbuf_append(&cte_query, "SELECT json_each.value AS value");
            if (has_carry) dbuf_append(&cte_query, dbuf_get(&carry_cols));
            dbuf_append(&cte_query, " FROM ");

            if (inner_sql && strlen(inner_sql) > 0) {
                dbuf_appendf(&cte_query, "(%s) AS _prev, ", inner_sql);
            }

            /* Get property from appropriate property table */
            dbuf_appendf(&cte_query, "json_each(COALESCE("
                "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id "
                "WHERE npt.node_id = %s%s AND pk.key = '%s'), '[]'))",
                alias ? alias : id->name,
                is_projected ? "" : ".id",
                prop->property_name);
        } else {
            ctx->has_error = true;
            ctx->error_message = strdup("UNWIND property access requires identifier base");
            dbuf_free(&cte_query);
            free(inner_sql);
            return -1;
        }
    } else if (unwind->expr->type == AST_NODE_IDENTIFIER) {
        /* Variable reference - assume it's a list variable from previous clause */
        cypher_identifier *id = (cypher_identifier*)unwind->expr;
        const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);

        dbuf_append(&cte_query, "SELECT json_each.value AS value");
        if (has_carry) dbuf_append(&cte_query, dbuf_get(&carry_cols));
        dbuf_append(&cte_query, " FROM ");

        /* When the unwound variable IS a carried-through projection, source
         * it from `_prev` (the subquery alias) rather than its original CTE
         * alias — the original CTE isn't in the new FROM clause directly. */
        const char *json_each_arg = alias ? alias : id->name;
        char prev_arg[128];
        if (has_carry && alias) {
            const char *col = strrchr(alias, '.');
            if (col) {
                char _ib[128];
                snprintf(prev_arg, sizeof(prev_arg), "_prev.%s",
                         sql_ident(_ib, sizeof(_ib), col + 1));
                json_each_arg = prev_arg;
            }
        }

        if (inner_sql && strlen(inner_sql) > 0) {
            dbuf_appendf(&cte_query, "(%s) AS _prev, ", inner_sql);
        }

        dbuf_appendf(&cte_query, "json_each(%s)", json_each_arg);
    } else if (unwind->expr->type == AST_NODE_FUNCTION_CALL ||
               unwind->expr->type == AST_NODE_SUBSCRIPT ||
               unwind->expr->type == AST_NODE_BINARY_OP) {
        /* Function call that returns a list (e.g., range(), keys(), etc.),
         * subscript expression (qrows[p]), or binary op (list concat). */
        /* Transform the function expression to SQL, then use json_each on the result */

        /* Save current SQL buffer state */
        char *saved_buffer = ctx->sql_buffer;
        size_t saved_size = ctx->sql_size;
        size_t saved_capacity = ctx->sql_capacity;

        /* Allocate temporary buffer for expression transformation */
        size_t temp_capacity = 4096;
        char *temp_buffer = malloc(temp_capacity);
        if (!temp_buffer) {
            ctx->has_error = true;
            ctx->error_message = strdup("Memory allocation failed in UNWIND");
            dbuf_free(&cte_query);
            free(inner_sql);
            return -1;
        }
        temp_buffer[0] = '\0';

        /* Switch to temporary buffer */
        ctx->sql_buffer = temp_buffer;
        ctx->sql_size = 0;
        ctx->sql_capacity = temp_capacity;

        /* Transform the function call expression */
        if (transform_expression(ctx, unwind->expr) < 0) {
            free(temp_buffer);
            ctx->sql_buffer = saved_buffer;
            ctx->sql_size = saved_size;
            ctx->sql_capacity = saved_capacity;
            dbuf_free(&cte_query);
            free(inner_sql);
            return -1;
        }

        /* Capture the transformed function SQL */
        char *func_sql = strdup(ctx->sql_buffer);

        /* Restore original buffer */
        free(temp_buffer);
        ctx->sql_buffer = saved_buffer;
        ctx->sql_size = saved_size;
        ctx->sql_capacity = saved_capacity;

        /* Build json_each query on the function result. T-0305: when
         * inner_sql is splice-able (`SELECT * FROM <tables>`), the
         * outer FROM uses original aliases (no _prev wrapping), so
         * the carry-cols must reference those original aliases too. */
        bool splicable = (inner_sql && strlen(inner_sql) > 0 &&
            strncmp(inner_sql, "SELECT * FROM ", 14) == 0);
        /* When inner_sql has a WHERE clause, we need to splice the FROM
         * tables BEFORE the comma + json_each(...) and emit the WHERE
         * AFTER the join. Reproducer: Graph8 [5]
         *   MATCH ()-[r:KNOWS]-() UNWIND keys(r) AS x ...
         * Previously emitted `... WHERE <conds>, json_each(...)` which
         * misplaces the comma. */
        const char *where_in_inner = NULL;
        if (splicable) {
            where_in_inner = strstr(inner_sql, " WHERE ");
        }
        dbuf_append(&cte_query, "SELECT json_each.value AS value");
        if (has_carry) {
            dbuf_append(&cte_query,
                splicable ? dbuf_get(&carry_cols_orig) : dbuf_get(&carry_cols));
        }
        dbuf_append(&cte_query, " FROM ");

        if (inner_sql && strlen(inner_sql) > 0) {
            if (splicable) {
                if (where_in_inner) {
                    /* Emit just the FROM tables (between "SELECT * FROM "
                     * and the " WHERE "), then the json_each, then re-attach
                     * WHERE afterwards. */
                    size_t tables_len = (size_t)(where_in_inner - (inner_sql + 14));
                    dbuf_appendf(&cte_query, "%.*s, ", (int)tables_len, inner_sql + 14);
                } else {
                    dbuf_appendf(&cte_query, "%s, ", inner_sql + 14);
                }
            } else {
                dbuf_appendf(&cte_query, "(%s) AS _prev, ", inner_sql);
            }
        }

        dbuf_appendf(&cte_query, "json_each(%s)", func_sql);
        if (splicable && where_in_inner) {
            /* WHERE keyword is included in where_in_inner (" WHERE ..."). */
            dbuf_append(&cte_query, where_in_inner);
        }
        free(func_sql);
    } else if (unwind->expr->type == AST_NODE_PARAMETER) {
        /* Parameter reference: use json_each on the bound parameter value.
         * Parameters become SQLite named parameters (:name), and json_each()
         * can iterate over JSON array text directly. */
        cypher_parameter *param = (cypher_parameter*)unwind->expr;
        if (!param->name) {
            ctx->has_error = true;
            ctx->error_message = strdup("UNWIND parameter must be named");
            dbuf_free(&cte_query);
            free(inner_sql);
            return -1;
        }

        register_parameter(ctx, param->name);

        dbuf_append(&cte_query, "SELECT json_each.value AS value");
        if (has_carry) dbuf_append(&cte_query, dbuf_get(&carry_cols));
        dbuf_append(&cte_query, " FROM ");

        if (inner_sql && strlen(inner_sql) > 0) {
            dbuf_appendf(&cte_query, "(%s) AS _prev, ", inner_sql);
        }

        dbuf_appendf(&cte_query, "json_each(:%s)", param->name);
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("UNWIND requires list literal, property access, variable, function call, or parameter");
        dbuf_free(&cte_query);
        free(inner_sql);
        return -1;
    }

    /* Add CTE to unified builder */
    sql_cte(ctx->unified_builder, cte_name, dbuf_get(&cte_query), false);
    dbuf_free(&cte_query);
    ctx->cte_count++;

    /* Clear old variables - UNWIND creates a new scope */
    transform_var_ctx_reset(ctx->var_ctx);

    /* Restore the projected variables we captured pre-reset, now pointing
     * at the new UNWIND CTE's columns (which we projected through above). */
    for (int i = 0; i < carry_count; i++) {
        if (has_carry) {
            char src[256], _ib[128];
            snprintf(src, sizeof(src), "%s.%s", cte_name,
                     sql_ident(_ib, sizeof(_ib), carry_names[i]));
            transform_var_register_projected(ctx->var_ctx, carry_names[i], src);
        }
        free(carry_names[i]);
        free(carry_aliases[i]);
    }
    dbuf_free(&carry_cols);
    dbuf_free(&carry_cols_orig);

    /* Register the unwound variable in unified system */
    char unwind_source[256];
    snprintf(unwind_source, sizeof(unwind_source), "%s.value", cte_name);
    transform_var_register_projected(ctx->var_ctx, unwind->alias, unwind_source);

    /* Set up FROM for the outer query — SELECT columns are added by RETURN clause */
    sql_from(ctx->unified_builder, cte_name, NULL);

    free(inner_sql);
    CYPHER_DEBUG("UNWIND clause generated CTE via unified builder: %s", cte_name);
    return 0;
}
