/*
 * FOREACH Clause Execution
 * Handles FOREACH clause iteration and body clause execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

/* Execute FOREACH clause - iterate over list and execute body clauses */
int execute_foreach_clause(cypher_executor *executor, cypher_foreach *foreach, cypher_result *result)
{
    if (!executor || !foreach || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing FOREACH clause, variable=%s", foreach->variable ? foreach->variable : "<null>");

    if (!foreach->variable || !foreach->list_expr || !foreach->body) {
        set_result_error(result, "FOREACH clause missing required elements");
        return -1;
    }

    /* Evaluate the list expression - for now, only handle list literals */
    if (foreach->list_expr->type != AST_NODE_LIST) {
        set_result_error(result, "FOREACH currently only supports list literals");
        return -1;
    }

    cypher_list *list = (cypher_list*)foreach->list_expr;
    if (!list->items || list->items->count == 0) {
        /* Empty list - nothing to do */
        return 0;
    }

    /* Create foreach context for variable binding */
    foreach_context *ctx = create_foreach_context();
    if (!ctx) {
        set_result_error(result, "Failed to create foreach context");
        return -1;
    }

    /* Save previous context and set ours */
    foreach_context *prev_ctx = g_foreach_ctx;
    g_foreach_ctx = ctx;

    /* Iterate over list items */
    for (int i = 0; i < list->items->count; i++) {
        ast_node *item = list->items->items[i];

        /* Unwrap return_item if present */
        if (item->type == AST_NODE_RETURN_ITEM) {
            item = ((cypher_return_item*)item)->expr;
        }

        /* Bind the loop variable based on item type */
        if (item->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)item;
            switch (lit->literal_type) {
                case LITERAL_INTEGER:
                    set_foreach_binding_int(ctx, foreach->variable, lit->value.integer);
                    break;
                case LITERAL_STRING:
                    set_foreach_binding_string(ctx, foreach->variable, lit->value.string);
                    break;
                case LITERAL_DECIMAL:
                    /* For now, store as int (truncated) */
                    set_foreach_binding_int(ctx, foreach->variable, (int64_t)lit->value.decimal);
                    break;
                default:
                    CYPHER_DEBUG("Unsupported literal type in FOREACH list: %d", lit->literal_type);
                    continue;
            }
        } else {
            CYPHER_DEBUG("Unsupported item type in FOREACH list: %d", item->type);
            continue;
        }

        CYPHER_DEBUG("FOREACH iteration %d, variable=%s", i, foreach->variable);

        /* Execute each clause in the body */
        for (int j = 0; j < foreach->body->count; j++) {
            ast_node *clause = foreach->body->items[j];
            if (!clause) continue;

            switch (clause->type) {
                case AST_NODE_CREATE:
                    if (execute_create_clause(executor, (cypher_create*)clause, result) < 0) {
                        g_foreach_ctx = prev_ctx;
                        free_foreach_context(ctx);
                        return -1;
                    }
                    break;

                case AST_NODE_SET:
                    if (execute_set_clause(executor, (cypher_set*)clause, result) < 0) {
                        g_foreach_ctx = prev_ctx;
                        free_foreach_context(ctx);
                        return -1;
                    }
                    break;

                case AST_NODE_FOREACH:
                    /* Nested FOREACH - recursive call */
                    if (execute_foreach_clause(executor, (cypher_foreach*)clause, result) < 0) {
                        g_foreach_ctx = prev_ctx;
                        free_foreach_context(ctx);
                        return -1;
                    }
                    break;

                default:
                    CYPHER_DEBUG("Unsupported clause type in FOREACH body: %d", clause->type);
                    break;
            }
        }
    }

    /* Restore previous context */
    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);

    return 0;
}
