/*
 * UNWIND Clause Transformation
 * Converts UNWIND clauses that expand lists into rows
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_internal.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

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
    static int unwind_cte_counter = 0;
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_unwind_%d", unwind_cte_counter++);

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

    /* Check expression type and generate appropriate SQL */
    if (unwind->expr->type == AST_NODE_LIST) {
        /* List literal: use UNION ALL approach */
        cypher_list *list = (cypher_list*)unwind->expr;

        if (!list->items || list->items->count == 0) {
            /* Empty list: return no rows using impossible condition */
            dbuf_append(&cte_query, "SELECT NULL AS value WHERE 0");
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
                            dbuf_appendf(&cte_query, "%d", lit->value.integer);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&cte_query, "%g", lit->value.decimal);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&cte_query, "'%s'", lit->value.string ? lit->value.string : "");
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&cte_query, "%s", lit->value.boolean ? "1" : "0");
                            break;
                        case LITERAL_NULL:
                            dbuf_append(&cte_query, "NULL");
                            break;
                    }
                } else {
                    /* For other expression types, we'd need to transform them */
                    /* For now, just handle literals */
                    dbuf_append(&cte_query, "NULL");
                }
                dbuf_append(&cte_query, " AS value");
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
            dbuf_append(&cte_query, "SELECT json_each.value AS value FROM ");

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

        dbuf_append(&cte_query, "SELECT json_each.value AS value FROM ");

        if (inner_sql && strlen(inner_sql) > 0) {
            dbuf_appendf(&cte_query, "(%s) AS _prev, ", inner_sql);
        }

        dbuf_appendf(&cte_query, "json_each(%s)", alias ? alias : id->name);
    } else if (unwind->expr->type == AST_NODE_FUNCTION_CALL) {
        /* Function call that returns a list (e.g., range(), keys(), etc.) */
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

        /* Build json_each query on the function result */
        dbuf_append(&cte_query, "SELECT json_each.value AS value FROM ");

        if (inner_sql && strlen(inner_sql) > 0) {
            dbuf_appendf(&cte_query, "(%s) AS _prev, ", inner_sql);
        }

        dbuf_appendf(&cte_query, "json_each(%s)", func_sql);
        free(func_sql);
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("UNWIND requires list literal, property access, variable, or function call");
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

    /* Register the unwound variable in unified system */
    char unwind_source[256];
    snprintf(unwind_source, sizeof(unwind_source), "%s.value", cte_name);
    transform_var_register_projected(ctx->var_ctx, unwind->alias, unwind_source);

    /* Build the outer query using unified builder */
    char select_expr[256];
    snprintf(select_expr, sizeof(select_expr), "%s.value", cte_name);
    sql_select(ctx->unified_builder, select_expr, unwind->alias);
    sql_from(ctx->unified_builder, cte_name, NULL);

    free(inner_sql);
    CYPHER_DEBUG("UNWIND clause generated CTE via unified builder: %s", cte_name);
    return 0;
}
