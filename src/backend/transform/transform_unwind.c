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

    /* Save the inner SQL if any (from previous clauses like MATCH) */
    char *inner_sql = NULL;
    if (ctx->sql_size > 0) {
        inner_sql = strdup(ctx->sql_buffer);
        if (!inner_sql) {
            ctx->has_error = true;
            ctx->error_message = strdup("Memory allocation failed");
            return -1;
        }
    }

    /* Start building CTE prefix */
    if (ctx->cte_prefix_size == 0) {
        append_cte_prefix(ctx, "WITH ");
    } else {
        append_cte_prefix(ctx, ", ");
    }

    append_cte_prefix(ctx, "%s AS (", cte_name);

    /* Check expression type and generate appropriate SQL */
    if (unwind->expr->type == AST_NODE_LIST) {
        /* List literal: use UNION ALL approach */
        cypher_list *list = (cypher_list*)unwind->expr;

        if (!list->items || list->items->count == 0) {
            /* Empty list: return no rows using impossible condition */
            append_cte_prefix(ctx, "SELECT NULL AS value WHERE 0");
        } else {
            for (int i = 0; i < list->items->count; i++) {
                if (i > 0) {
                    append_cte_prefix(ctx, " UNION ALL ");
                }
                append_cte_prefix(ctx, "SELECT ");

                /* Transform the expression to get its SQL value */
                ast_node *item = list->items->items[i];
                if (item->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)item;
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            append_cte_prefix(ctx, "%d", lit->value.integer);
                            break;
                        case LITERAL_DECIMAL:
                            append_cte_prefix(ctx, "%g", lit->value.decimal);
                            break;
                        case LITERAL_STRING:
                            append_cte_prefix(ctx, "'%s'", lit->value.string ? lit->value.string : "");
                            break;
                        case LITERAL_BOOLEAN:
                            append_cte_prefix(ctx, "%s", lit->value.boolean ? "1" : "0");
                            break;
                        case LITERAL_NULL:
                            append_cte_prefix(ctx, "NULL");
                            break;
                    }
                } else {
                    /* For other expression types, we'd need to transform them */
                    /* For now, just handle literals */
                    append_cte_prefix(ctx, "NULL");
                }
                append_cte_prefix(ctx, " AS value");
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
            append_cte_prefix(ctx, "SELECT json_each.value AS value FROM ");

            if (inner_sql && strlen(inner_sql) > 0) {
                append_cte_prefix(ctx, "(%s) AS _prev, ", inner_sql);
            }

            /* Get property from appropriate property table */
            append_cte_prefix(ctx, "json_each(COALESCE("
                "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id "
                "WHERE npt.node_id = %s%s AND pk.key = '%s'), '[]'))",
                alias ? alias : id->name,
                is_projected ? "" : ".id",
                prop->property_name);
        } else {
            ctx->has_error = true;
            ctx->error_message = strdup("UNWIND property access requires identifier base");
            free(inner_sql);
            return -1;
        }
    } else if (unwind->expr->type == AST_NODE_IDENTIFIER) {
        /* Variable reference - assume it's a list variable from previous clause */
        cypher_identifier *id = (cypher_identifier*)unwind->expr;
        const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);

        append_cte_prefix(ctx, "SELECT json_each.value AS value FROM ");

        if (inner_sql && strlen(inner_sql) > 0) {
            append_cte_prefix(ctx, "(%s) AS _prev, ", inner_sql);
        }

        append_cte_prefix(ctx, "json_each(%s)", alias ? alias : id->name);
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("UNWIND requires list literal, property access, or variable");
        free(inner_sql);
        return -1;
    }

    append_cte_prefix(ctx, ")");
    ctx->cte_count++;

    /* Clear old variables - UNWIND creates a new scope */
    transform_var_ctx_reset(ctx->var_ctx);

    /* Register the unwound variable in unified system */
    char unwind_source[256];
    snprintf(unwind_source, sizeof(unwind_source), "%s.value", cte_name);
    transform_var_register_projected(ctx->var_ctx, unwind->alias, unwind_source);

    /* Build the outer SELECT */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';
    append_sql(ctx, "SELECT %s.value AS %s FROM %s", cte_name, unwind->alias, cte_name);

    free(inner_sql);
    CYPHER_DEBUG("UNWIND clause generated CTE: %s", cte_name);
    return 0;
}
