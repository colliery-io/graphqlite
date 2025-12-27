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

    /* Build CTE query in a local buffer */
    dynamic_buffer cte_query;
    dbuf_init(&cte_query);

    /* Generate CTE that expands the list using json_each */
    dbuf_appendf(&cte_query, "SELECT value AS \"%s\" FROM json_each(", foreach->variable);

    /* Transform the list expression into a JSON array */
    /* For list literals like [1,2,3], we generate json_array(1,2,3) */
    if (foreach->list_expr->type == AST_NODE_LIST) {
        cypher_list *list = (cypher_list*)foreach->list_expr;
        dbuf_append(&cte_query, "json_array(");
        for (int i = 0; i < list->items->count; i++) {
            if (i > 0) {
                dbuf_append(&cte_query, ", ");
            }
            ast_node *elem = list->items->items[i];
            if (elem->type == AST_NODE_RETURN_ITEM) {
                elem = ((cypher_return_item*)elem)->expr;
            }
            if (elem->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)elem;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER:
                        dbuf_appendf(&cte_query, "%lld", lit->value.integer);
                        break;
                    case LITERAL_DECIMAL:
                        dbuf_appendf(&cte_query, "%f", lit->value.decimal);
                        break;
                    case LITERAL_STRING:
                        dbuf_appendf(&cte_query, "'%s'", lit->value.string);
                        break;
                    case LITERAL_BOOLEAN:
                        dbuf_appendf(&cte_query, "%s", lit->value.boolean ? "true" : "false");
                        break;
                    case LITERAL_NULL:
                        dbuf_append(&cte_query, "null");
                        break;
                }
            } else if (elem->type == AST_NODE_IDENTIFIER) {
                cypher_identifier *id = (cypher_identifier*)elem;
                const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
                if (alias) {
                    dbuf_appendf(&cte_query, "%s", alias);
                } else {
                    dbuf_appendf(&cte_query, "\"%s\"", id->name);
                }
            } else {
                /* For complex expressions, fall back to a placeholder */
                dbuf_append(&cte_query, "null");
            }
        }
        dbuf_append(&cte_query, ")");
    } else if (foreach->list_expr->type == AST_NODE_IDENTIFIER) {
        /* Variable reference - assume it's a JSON array already */
        cypher_identifier *id = (cypher_identifier*)foreach->list_expr;
        const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
        if (alias) {
            dbuf_appendf(&cte_query, "%s", alias);
        } else {
            dbuf_appendf(&cte_query, "\"%s\"", id->name);
        }
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("FOREACH list expression must be a list literal or variable");
        dbuf_free(&cte_query);
        return -1;
    }

    dbuf_append(&cte_query, ")");

    /* Add CTE to unified builder */
    sql_cte(ctx->unified_builder, cte_name, dbuf_get(&cte_query), false);
    dbuf_free(&cte_query);
    ctx->cte_count++;

    /* Register the loop variable in unified system */
    char var_alias[128];
    snprintf(var_alias, sizeof(var_alias), "%s.\"%s\"", cte_name, foreach->variable);
    transform_var_register_projected(ctx->var_ctx, foreach->variable, var_alias);

    /*
     * FOREACH is now handled in the executor via execute_foreach_clause().
     * This transform function is only used for EXPLAIN mode to show the CTE structure.
     * The actual execution happens imperatively in C.
     */
    (void)foreach;  /* Mark as intentionally unused in body processing */

    return 0;
}
