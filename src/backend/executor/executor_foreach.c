/*
 * FOREACH Clause Execution
 * Handles FOREACH clause iteration and body clause execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declaration for evaluating list expressions */
static int evaluate_list_expression(cypher_executor *executor, ast_node *expr,
                                   char ***values, int *count);

/* Helper to execute body clauses for one iteration */
static int execute_foreach_body(cypher_executor *executor, cypher_foreach *foreach,
                               cypher_result *result, foreach_context *ctx,
                               foreach_context *prev_ctx)
{
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
    return 0;
}

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

    /* Create foreach context for variable binding */
    foreach_context *ctx = create_foreach_context();
    if (!ctx) {
        set_result_error(result, "Failed to create foreach context");
        return -1;
    }

    /* Save previous context and set ours */
    foreach_context *prev_ctx = g_foreach_ctx;
    g_foreach_ctx = ctx;

    /* Handle list literals directly */
    if (foreach->list_expr->type == AST_NODE_LIST) {
        cypher_list *list = (cypher_list*)foreach->list_expr;
        if (!list->items || list->items->count == 0) {
            /* Empty list - nothing to do */
            g_foreach_ctx = prev_ctx;
            free_foreach_context(ctx);
            return 0;
        }

        /* Iterate over list literal items */
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

            if (execute_foreach_body(executor, foreach, result, ctx, prev_ctx) < 0) {
                return -1;
            }
        }
    } else {
        /* Evaluate the list expression (e.g., range(), collect(), variable) */
        char **values = NULL;
        int count = 0;

        if (evaluate_list_expression(executor, foreach->list_expr, &values, &count) < 0) {
            set_result_error(result, "Failed to evaluate FOREACH list expression");
            g_foreach_ctx = prev_ctx;
            free_foreach_context(ctx);
            return -1;
        }

        if (count == 0 || !values) {
            /* Empty list - nothing to do */
            g_foreach_ctx = prev_ctx;
            free_foreach_context(ctx);
            return 0;
        }

        /* Iterate over evaluated values */
        for (int i = 0; i < count; i++) {
            if (!values[i]) continue;

            /* Try to parse as integer first */
            char *endptr;
            long long int_val = strtoll(values[i], &endptr, 10);
            if (*endptr == '\0') {
                set_foreach_binding_int(ctx, foreach->variable, int_val);
            } else {
                set_foreach_binding_string(ctx, foreach->variable, values[i]);
            }

            CYPHER_DEBUG("FOREACH iteration %d, variable=%s, value=%s", i, foreach->variable, values[i]);

            if (execute_foreach_body(executor, foreach, result, ctx, prev_ctx) < 0) {
                /* Free values array */
                for (int k = 0; k < count; k++) {
                    free(values[k]);
                }
                free(values);
                return -1;
            }
        }

        /* Free values array */
        for (int i = 0; i < count; i++) {
            free(values[i]);
        }
        free(values);
    }

    /* Restore previous context */
    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);

    return 0;
}

/* Evaluate a list expression by transforming to SQL and executing */
static int evaluate_list_expression(cypher_executor *executor, ast_node *expr,
                                   char ***values, int *count)
{
    if (!executor || !expr || !values || !count) {
        return -1;
    }

    *values = NULL;
    *count = 0;

    /* Create a transform context to generate SQL for the expression */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        return -1;
    }

    /* Transform the expression to SQL */
    append_sql(ctx, "SELECT ");
    if (transform_expression(ctx, expr) < 0) {
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Get the generated SQL */
    const char *sql = ctx->sql_buffer;
    if (!sql || strlen(sql) == 0) {
        cypher_transform_free_context(ctx);
        return -1;
    }

    CYPHER_DEBUG("FOREACH evaluating expression: %s", sql);

    /* Execute the SQL */
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare FOREACH expression: %s", sqlite3_errmsg(executor->db));
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Get the result (should be a single JSON array) */
    char *json_result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        if (text) {
            json_result = strdup(text);
        }
    }
    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);

    if (!json_result) {
        return 0;  /* Empty result, not an error */
    }

    CYPHER_DEBUG("FOREACH expression result: %s", json_result);

    /* Parse the JSON array into values */
    /* Expected format: [val1, val2, ...] or just a single value */
    char *p = json_result;

    /* Skip leading whitespace */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;

    if (*p != '[') {
        /* Not an array - treat as single value */
        *values = malloc(sizeof(char*));
        if (*values) {
            (*values)[0] = json_result;
            *count = 1;
        }
        return 0;
    }

    /* Count elements first */
    int elem_count = 0;
    int depth = 0;
    bool in_string = false;
    for (char *c = p; *c; c++) {
        if (*c == '"' && (c == p || *(c-1) != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (*c == '[' || *c == '{') depth++;
            else if (*c == ']' || *c == '}') depth--;
            else if (*c == ',' && depth == 1) elem_count++;
        }
    }
    if (depth == 0 && p[1] != ']') elem_count++;  /* Account for last element */

    if (elem_count == 0) {
        free(json_result);
        return 0;  /* Empty array */
    }

    /* Allocate values array */
    *values = calloc(elem_count, sizeof(char*));
    if (!*values) {
        free(json_result);
        return -1;
    }

    /* Parse elements */
    int idx = 0;
    p++;  /* Skip '[' */
    while (*p && *p != ']' && idx < elem_count) {
        /* Skip whitespace */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')) p++;
        if (*p == ']') break;

        char *start = p;
        depth = 0;
        in_string = false;

        /* Find end of element */
        while (*p) {
            if (*p == '"' && (p == start || *(p-1) != '\\')) {
                in_string = !in_string;
            } else if (!in_string) {
                if (*p == '[' || *p == '{') depth++;
                else if (*p == ']' || *p == '}') {
                    if (depth == 0) break;
                    depth--;
                }
                else if (*p == ',' && depth == 0) break;
            }
            p++;
        }

        /* Extract element */
        int len = p - start;
        if (len > 0) {
            char *elem = malloc(len + 1);
            if (elem) {
                strncpy(elem, start, len);
                elem[len] = '\0';

                /* Remove quotes from strings */
                if (elem[0] == '"' && len >= 2 && elem[len-1] == '"') {
                    memmove(elem, elem + 1, len - 2);
                    elem[len - 2] = '\0';
                }

                (*values)[idx++] = elem;
            }
        }
    }

    *count = idx;
    free(json_result);
    return 0;
}
