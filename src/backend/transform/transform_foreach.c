/*
 * FOREACH clause transformation
 * Transforms Cypher FOREACH clause to SQL
 *
 * FOREACH iterates over a list and executes update clauses for each element.
 * Example: FOREACH (x IN [1,2,3] | CREATE (n {val: x}))
 *
 * Transformation approach:
 * For simple cases, we use CTEs with json_each() to iterate over the list.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/*
 * Transform a FOREACH clause to SQL.
 *
 * The challenge with FOREACH is that SQL doesn't have an imperative loop construct.
 * We transform FOREACH using a CTE approach:
 *
 * FOREACH (x IN [1,2,3] | CREATE (n {val: x}))
 * becomes:
 * WITH foreach_data AS (
 *   SELECT value AS x FROM json_each(json_array(1,2,3))
 * )
 * INSERT INTO nodes (id, properties)
 * SELECT generate_id(), json_object('val', x) FROM foreach_data;
 *
 * For multiple update clauses, we use compound statements.
 */
int transform_foreach_clause(cypher_transform_context *ctx, cypher_foreach *foreach)
{
    if (!ctx || !foreach) {
        return -1;
    }

    CYPHER_DEBUG("Transforming FOREACH clause, variable=%s",
                 foreach->variable ? foreach->variable : "<null>");

    if (!foreach->variable || !foreach->list_expr || !foreach->body) {
        ctx->has_error = true;
        ctx->error_message = strdup("FOREACH clause missing required elements");
        return -1;
    }

    /* Check for nested FOREACH - not yet supported */
    for (int i = 0; i < foreach->body->count; i++) {
        ast_node *clause = foreach->body->items[i];
        if (clause && clause->type == AST_NODE_FOREACH) {
            ctx->has_error = true;
            ctx->error_message = strdup("Nested FOREACH is not yet supported");
            return -1;
        }
    }

    /* Generate a unique CTE name for this FOREACH */
    char cte_name[64];
    snprintf(cte_name, sizeof(cte_name), "_foreach_data_%d", ctx->global_alias_counter++);

    /* Start building the CTE */
    if (ctx->cte_count == 0 && ctx->cte_prefix_size == 0) {
        append_cte_prefix(ctx, "WITH ");
    } else if (ctx->cte_prefix_size > 0) {
        append_cte_prefix(ctx, ", ");
    }

    /* Generate CTE that expands the list */
    append_cte_prefix(ctx, "%s AS (SELECT value AS \"%s\" FROM json_each(",
                      cte_name, foreach->variable);

    /* Transform the list expression into a JSON array */
    /* For list literals like [1,2,3], we generate json_array(1,2,3) */
    if (foreach->list_expr->type == AST_NODE_LIST) {
        cypher_list *list = (cypher_list*)foreach->list_expr;
        append_cte_prefix(ctx, "json_array(");
        for (int i = 0; i < list->items->count; i++) {
            if (i > 0) {
                append_cte_prefix(ctx, ", ");
            }
            ast_node *elem = list->items->items[i];
            if (elem->type == AST_NODE_RETURN_ITEM) {
                elem = ((cypher_return_item*)elem)->expr;
            }
            if (elem->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)elem;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER:
                        append_cte_prefix(ctx, "%lld", lit->value.integer);
                        break;
                    case LITERAL_DECIMAL:
                        append_cte_prefix(ctx, "%f", lit->value.decimal);
                        break;
                    case LITERAL_STRING:
                        append_cte_prefix(ctx, "'%s'", lit->value.string);
                        break;
                    case LITERAL_BOOLEAN:
                        append_cte_prefix(ctx, "%s", lit->value.boolean ? "true" : "false");
                        break;
                    case LITERAL_NULL:
                        append_cte_prefix(ctx, "null");
                        break;
                }
            } else if (elem->type == AST_NODE_IDENTIFIER) {
                cypher_identifier *id = (cypher_identifier*)elem;
                const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
                if (alias) {
                    append_cte_prefix(ctx, "%s", alias);
                } else {
                    append_cte_prefix(ctx, "\"%s\"", id->name);
                }
            } else {
                /* For complex expressions, fall back to a placeholder */
                append_cte_prefix(ctx, "null");
            }
        }
        append_cte_prefix(ctx, ")");
    } else if (foreach->list_expr->type == AST_NODE_IDENTIFIER) {
        /* Variable reference - assume it's a JSON array already */
        cypher_identifier *id = (cypher_identifier*)foreach->list_expr;
        const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
        if (alias) {
            append_cte_prefix(ctx, "%s", alias);
        } else {
            append_cte_prefix(ctx, "\"%s\"", id->name);
        }
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("FOREACH list expression must be a list literal or variable");
        return -1;
    }

    append_cte_prefix(ctx, "))");
    ctx->cte_count++;

    /* Register the loop variable */
    char var_alias[128];
    snprintf(var_alias, sizeof(var_alias), "%s.\"%s\"", cte_name, foreach->variable);
    register_projected_variable(ctx, foreach->variable, cte_name, foreach->variable);
    /* Register in unified system */
    transform_var_register_projected(ctx->var_ctx, foreach->variable, var_alias);

    /*
     * FOREACH is now handled in the executor via execute_foreach_clause().
     * This transform function is only used for EXPLAIN mode to show the CTE structure.
     * The actual execution happens imperatively in C.
     */
    (void)foreach;  /* Mark as intentionally unused in body processing */

    return 0;
}
