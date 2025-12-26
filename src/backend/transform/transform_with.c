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
#include "parser/cypher_debug.h"

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

    /* Generate a unique CTE name */
    static int with_cte_counter = 0;
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_with_%d", with_cte_counter++);

    /* Find the SELECT clause and build the projected columns */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (!select_pos) {
        select_pos = strstr(ctx->sql_buffer, "SELECT ");
    }

    if (!select_pos) {
        ctx->has_error = true;
        ctx->error_message = strdup("WITH clause requires a preceding MATCH");
        return -1;
    }

    /* Build the column list for the inner SELECT */
    char *inner_sql = strdup(ctx->sql_buffer);
    if (!inner_sql) {
        ctx->has_error = true;
        ctx->error_message = strdup("Memory allocation failed");
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

    /* Build the CTE and new SELECT */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';

    /* Add to CTE prefix */
    if (ctx->cte_prefix_size == 0) {
        append_cte_prefix(ctx, "WITH ");
    } else {
        append_cte_prefix(ctx, ", ");
    }
    append_cte_prefix(ctx, "%s AS (%s)", cte_name, inner_sql);
    ctx->cte_count++;

    free(inner_sql);

    /* Build the outer SELECT from the CTE */
    append_sql(ctx, "SELECT ");

    if (with->distinct) {
        append_sql(ctx, "DISTINCT ");
    }

    /* Clear old variables - WITH creates a new scope */
    for (int i = 0; i < ctx->variable_count; i++) {
        free(ctx->variables[i].name);
        free(ctx->variables[i].table_alias);
    }
    ctx->variable_count = 0;
    /* Clear unified system too */
    transform_var_ctx_reset(ctx->var_ctx);

    /* Register new variables from WITH projections and build SELECT list */
    for (int i = 0; i < with->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)with->items->items[i];

        if (i > 0) {
            append_sql(ctx, ", ");
        }

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
            append_sql(ctx, "%s.%s", cte_name, col_name);

            /* Register the new projected variable */
            /* The variable name is the alias (or original name) */
            /* The full reference is cte_name.col_name */
            register_projected_variable(ctx, col_name, cte_name, col_name);
            /* Register in unified system */
            char proj_source[256];
            snprintf(proj_source, sizeof(proj_source), "%s.%s", cte_name, col_name);
            transform_var_register_projected(ctx->var_ctx, col_name, proj_source);
            CYPHER_DEBUG("WITH: Registered projected variable '%s' -> %s.%s", col_name, cte_name, col_name);
        } else {
            append_sql(ctx, "*");
        }
    }

    append_sql(ctx, " FROM %s", cte_name);

    /* Handle WHERE clause (applied after projection) */
    if (with->where) {
        append_sql(ctx, " WHERE ");
        if (transform_expression(ctx, with->where) < 0) {
            return -1;
        }
    }

    /* Handle ORDER BY */
    if (with->order_by && with->order_by->count > 0) {
        append_sql(ctx, " ORDER BY ");
        for (int i = 0; i < with->order_by->count; i++) {
            if (i > 0) {
                append_sql(ctx, ", ");
            }
            cypher_order_by_item *order_item = (cypher_order_by_item*)with->order_by->items[i];
            if (transform_expression(ctx, order_item->expr) < 0) {
                return -1;
            }
            if (order_item->descending) {
                append_sql(ctx, " DESC");
            }
        }
    }

    /* Handle LIMIT */
    if (with->limit) {
        append_sql(ctx, " LIMIT ");
        if (transform_expression(ctx, with->limit) < 0) {
            return -1;
        }
    }

    /* Handle SKIP */
    if (with->skip) {
        append_sql(ctx, " OFFSET ");
        if (transform_expression(ctx, with->skip) < 0) {
            return -1;
        }
    }

    CYPHER_DEBUG("WITH clause generated CTE: %s", cte_name);
    return 0;
}

