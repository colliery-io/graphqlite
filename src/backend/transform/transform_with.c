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

    /* Get the inner SQL from the unified builder (WITHOUT CTEs - they'll be preserved) */
    char *inner_sql = NULL;
    char *saved_cte = NULL;
    int saved_cte_count = 0;
    if (ctx->unified_builder && !dbuf_is_empty(&ctx->unified_builder->from)) {
        /* Use subquery (no CTEs) - CTEs will be merged at the parent level */
        inner_sql = sql_builder_to_subquery(ctx->unified_builder);

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

    if (!inner_sql) {
        ctx->has_error = true;
        ctx->error_message = strdup("WITH clause requires a preceding MATCH");
        return -1;
    }

    /* Find the SELECT clause and build the projected columns */
    char *select_pos = strstr(inner_sql, "SELECT *");
    if (!select_pos) {
        select_pos = strstr(inner_sql, "SELECT ");
    }

    if (!select_pos) {
        ctx->has_error = true;
        ctx->error_message = strdup("WITH clause requires a preceding MATCH with SELECT");
        free(inner_sql);
        return -1;
    }

    /* If we have SELECT *, replace it with the actual columns */
    char *star_pos = strstr(inner_sql, "SELECT *");
    if (star_pos) {
        /* Build new column list */
        char col_buffer[8192] = "";
        int col_len = 0;

        /* Track GROUP BY columns for aggregate handling */
        char group_by_buffer[2048] = "";
        int group_by_len = 0;
        bool has_aggregate = false;

        for (int i = 0; i < with->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)with->items->items[i];

            if (i > 0) {
                col_len += snprintf(col_buffer + col_len, sizeof(col_buffer) - col_len, ", ");
            }

            /* Get the expression as SQL */
            if (item->expr->type == AST_NODE_IDENTIFIER) {
                cypher_identifier *id = (cypher_identifier*)item->expr;
                const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
                if (alias) {
                    /* Determine column name */
                    const char *col_name = item->alias ? item->alias : id->name;
                    col_len += snprintf(col_buffer + col_len, sizeof(col_buffer) - col_len,
                                       "%s.id AS %s", alias, col_name);
                    /* Add to GROUP BY */
                    if (group_by_len > 0) {
                        group_by_len += snprintf(group_by_buffer + group_by_len,
                                                sizeof(group_by_buffer) - group_by_len, ", ");
                    }
                    group_by_len += snprintf(group_by_buffer + group_by_len,
                                            sizeof(group_by_buffer) - group_by_len, "%s.id", alias);
                } else {
                    col_len += snprintf(col_buffer + col_len, sizeof(col_buffer) - col_len,
                                       "%s", id->name);
                }
            } else if (item->expr->type == AST_NODE_PROPERTY) {
                cypher_property *prop = (cypher_property*)item->expr;
                if (prop->expr->type == AST_NODE_IDENTIFIER) {
                    cypher_identifier *id = (cypher_identifier*)prop->expr;
                    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
                    const char *col_name = item->alias ? item->alias : prop->property_name;
                    if (alias) {
                        /* Use COALESCE with all property tables - order int/real first to preserve type */
                        col_len += snprintf(col_buffer + col_len, sizeof(col_buffer) - col_len,
                            "(SELECT COALESCE("
                            "(SELECT npi.value FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = '%s'), "
                            "(SELECT npr.value FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = '%s'), "
                            "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = '%s'), "
                            "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = '%s')"
                            ")) AS %s",
                            alias, prop->property_name,
                            alias, prop->property_name,
                            alias, prop->property_name,
                            alias, prop->property_name,
                            col_name);
                        /* Add the projected column name to GROUP BY (not node id) */
                        if (group_by_len > 0) {
                            group_by_len += snprintf(group_by_buffer + group_by_len,
                                                    sizeof(group_by_buffer) - group_by_len, ", ");
                        }
                        group_by_len += snprintf(group_by_buffer + group_by_len,
                                                sizeof(group_by_buffer) - group_by_len, "%s", col_name);
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
                    free(inner_sql);
                    return -1;
                }
                ctx->sql_size = 0;
                ctx->sql_buffer[0] = '\0';

                /* Transform the function call */
                if (transform_function_call(ctx, func) < 0) {
                    free(saved_buffer);
                    free(inner_sql);
                    return -1;
                }

                /* Copy the generated SQL to col_buffer */
                col_len += snprintf(col_buffer + col_len, sizeof(col_buffer) - col_len,
                                   "%s AS %s", ctx->sql_buffer, col_name);

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
                    free(inner_sql);
                    return -1;
                }
                ctx->sql_size = 0;
                ctx->sql_buffer[0] = '\0';

                /* Transform the expression */
                if (transform_expression(ctx, item->expr) < 0) {
                    free(saved_buffer);
                    free(inner_sql);
                    return -1;
                }

                /* Copy the generated SQL to col_buffer */
                col_len += snprintf(col_buffer + col_len, sizeof(col_buffer) - col_len,
                                   "(%s) AS %s", ctx->sql_buffer, col_name);

                /* Restore sql_buffer */
                ctx->sql_size = saved_size;
                strcpy(ctx->sql_buffer, saved_buffer);
                free(saved_buffer);
            } else {
                /* For other expression types, we'd need more complex handling */
                ctx->has_error = true;
                ctx->error_message = strdup("Complex expressions in WITH not yet supported");
                free(inner_sql);
                return -1;
            }
        }

        /* Add GROUP BY clause if we have aggregates and non-aggregate columns */
        char group_by_clause[2048] = "";
        if (has_aggregate && group_by_len > 0) {
            snprintf(group_by_clause, sizeof(group_by_clause), " GROUP BY %s", group_by_buffer);
        }

        /* Replace SELECT * with SELECT columns and add GROUP BY if needed */
        char *after_star = star_pos + strlen("SELECT *");
        size_t group_by_size = strlen(group_by_clause);
        size_t new_size = (star_pos - inner_sql) + strlen("SELECT ") + col_len + strlen(after_star) + group_by_size + 1;
        char *new_inner = malloc(new_size);
        if (!new_inner) {
            ctx->has_error = true;
            ctx->error_message = strdup("Memory allocation failed");
            free(inner_sql);
            return -1;
        }

        snprintf(new_inner, new_size, "%.*sSELECT %s%s%s",
                (int)(star_pos - inner_sql), inner_sql,
                col_buffer, after_star, group_by_clause);
        free(inner_sql);
        inner_sql = new_inner;
    }

    /* Inject any pending property JOINs from aggregate functions */
    size_t pending_len = get_pending_prop_joins_len();
    if (pending_len > 0) {
        const char *pending_joins = get_pending_prop_joins();

        /*
         * Find insertion point for JOINs. Check GROUP BY first since it's
         * unambiguous (not found inside subqueries). WHERE may appear inside
         * property subqueries, so we need to be careful.
         */
        char *insert_pos = NULL;

        /* First try GROUP BY (safest - always at top level) */
        char *group_pos = strstr(inner_sql, " GROUP BY");
        if (group_pos) {
            insert_pos = group_pos;
        } else {
            /*
             * Find the LAST WHERE at top level (not inside parentheses).
             * Count parentheses depth to avoid matching WHERE inside subqueries.
             */
            char *p = inner_sql;
            char *last_where = NULL;
            int paren_depth = 0;
            while (*p) {
                if (*p == '(') paren_depth++;
                else if (*p == ')') paren_depth--;
                else if (paren_depth == 0 && strncmp(p, " WHERE ", 7) == 0) {
                    last_where = p;
                }
                p++;
            }
            insert_pos = last_where;
        }

        if (insert_pos) {
            /* Insert JOINs at found position */
            size_t prefix_len = insert_pos - inner_sql;
            size_t suffix_len = strlen(insert_pos);
            size_t new_len = prefix_len + pending_len + suffix_len + 1;
            char *new_inner = malloc(new_len);
            if (!new_inner) {
                ctx->has_error = true;
                ctx->error_message = strdup("Memory allocation failed");
                free(inner_sql);
                reset_pending_prop_joins();
                return -1;
            }
            memcpy(new_inner, inner_sql, prefix_len);
            memcpy(new_inner + prefix_len, pending_joins, pending_len);
            memcpy(new_inner + prefix_len + pending_len, insert_pos, suffix_len + 1);
            free(inner_sql);
            inner_sql = new_inner;
            CYPHER_DEBUG("WITH: Injected property JOINs: %s", pending_joins);
        } else {
            /* Just append at the end */
            size_t old_len = strlen(inner_sql);
            size_t new_len = old_len + pending_len + 1;
            char *new_inner = malloc(new_len);
            if (!new_inner) {
                ctx->has_error = true;
                ctx->error_message = strdup("Memory allocation failed");
                free(inner_sql);
                reset_pending_prop_joins();
                return -1;
            }
            memcpy(new_inner, inner_sql, old_len);
            memcpy(new_inner + old_len, pending_joins, pending_len);
            new_inner[new_len - 1] = '\0';
            free(inner_sql);
            inner_sql = new_inner;
            CYPHER_DEBUG("WITH: Appended property JOINs: %s", pending_joins);
        }
        reset_pending_prop_joins();
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

