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
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

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
    reset_pending_prop_joins();

    /* Generate a unique CTE name */
    static int with_cte_counter = 0;
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_with_%d", with_cte_counter++);

    /*
     * Extract builder state directly instead of generating SQL and doing string manipulation.
     * This avoids the fragile "SELECT *" replacement pattern.
     */
    if (!ctx->unified_builder || !sql_builder_has_from(ctx->unified_builder)) {
        ctx->has_error = true;
        ctx->error_message = strdup("WITH clause requires a preceding MATCH");
        return -1;
    }

    /* Extract FROM, JOINs, WHERE from builder */
    const char *from_clause = sql_builder_get_from(ctx->unified_builder);
    const char *joins_clause = sql_builder_get_joins(ctx->unified_builder);
    const char *where_clause = sql_builder_get_where(ctx->unified_builder);
    const char *group_by_clause = sql_builder_get_group_by(ctx->unified_builder);

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
    bool has_aggregate = false;
    int group_count = 0;

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
                /* Check if this is a projected variable (already just an id) vs a node alias */
                bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
                const char *id_suffix = is_projected ? "" : ".id";
                dbuf_appendf(&col_buf, "%s%s AS %s", alias, id_suffix, col_name);
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
                    /* Check if this is a projected variable (already just an id) vs a node alias */
                    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
                    const char *id_suffix = is_projected ? "" : ".id";
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
                        col_name);
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
            dbuf_appendf(&col_buf, "%s AS %s", ctx->sql_buffer, col_name);

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
        } else if (item->expr->type == AST_NODE_BINARY_OP ||
                   item->expr->type == AST_NODE_CASE_EXPR ||
                   item->expr->type == AST_NODE_LITERAL) {
            /* Handle binary operations (arithmetic), CASE expressions, and literals */
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
            dbuf_appendf(&col_buf, "(%s) AS %s", ctx->sql_buffer, col_name);

            /* Restore sql_buffer */
            ctx->sql_size = saved_size;
            strcpy(ctx->sql_buffer, saved_buffer);
            free(saved_buffer);
        } else {
            /* For other expression types, we'd need more complex handling */
            ctx->has_error = true;
            ctx->error_message = strdup("Complex expressions in WITH not yet supported");
            dbuf_free(&col_buf);
            dbuf_free(&group_buf);
            free(saved_cte);
            return -1;
        }
    }

    /* Build CTE body: SELECT <cols> FROM <from><joins> WHERE <where> GROUP BY <group> */
    dynamic_buffer cte_body;
    dbuf_init(&cte_body);

    dbuf_append(&cte_body, "SELECT ");
    dbuf_append(&cte_body, dbuf_get(&col_buf));
    dbuf_append(&cte_body, " FROM ");
    dbuf_append(&cte_body, from_clause);

    if (joins_clause) {
        dbuf_append(&cte_body, joins_clause);
    }

    /* Add pending property JOINs from aggregate functions */
    size_t pending_len = get_pending_prop_joins_len();
    if (pending_len > 0) {
        const char *pending_joins = get_pending_prop_joins();
        dbuf_append(&cte_body, pending_joins);
        CYPHER_DEBUG("WITH: Added property JOINs: %s", pending_joins);
        reset_pending_prop_joins();
    }

    if (where_clause) {
        dbuf_append(&cte_body, " WHERE ");
        dbuf_append(&cte_body, where_clause);
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

    /* Clear old variables - WITH creates a new scope */
    transform_var_ctx_reset(ctx->var_ctx);

    /* Handle DISTINCT */
    if (with->distinct) {
        sql_distinct(ctx->unified_builder);
    }

    /* Set FROM to the CTE */
    sql_from(ctx->unified_builder, cte_name, NULL);

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
            /* Function calls must have an alias to be useful in subsequent clauses */
            col_name = ((cypher_function_call*)item->expr)->function_name;
        }

        if (col_name) {
            /* Add SELECT column */
            char select_expr[256];
            snprintf(select_expr, sizeof(select_expr), "%s.%s", cte_name, col_name);
            sql_select(ctx->unified_builder, select_expr, NULL);

            /* Register the new projected variable */
            transform_var_register_projected(ctx->var_ctx, col_name, select_expr);
            CYPHER_DEBUG("WITH: Registered projected variable '%s' -> %s.%s", col_name, cte_name, col_name);
        }
    }

    /* Handle WHERE clause (applied after projection) */
    if (with->where) {
        char *where_str = transform_expression_to_string(ctx, with->where);
        if (where_str) {
            sql_where(ctx->unified_builder, where_str);
            free(where_str);
        } else {
            return -1;
        }
    }

    /* Handle ORDER BY */
    if (with->order_by && with->order_by->count > 0) {
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

    CYPHER_DEBUG("WITH clause generated CTE via unified builder: %s", cte_name);
    return 0;
}

