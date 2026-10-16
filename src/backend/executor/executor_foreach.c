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
                                   variable_map *outer, char **json_out);

static void fold_var_map(variable_map *into, variable_map *from)
{
    if (!into || !from) return;
    for (int i = 0; i < from->count; i++) {
        variable_mapping *m = &from->mappings[i];
        if (m->type == VAR_MAP_TYPE_NODE) set_variable_node_id(into, m->variable, m->entity_id);
        else set_variable_edge_id(into, m->variable, m->entity_id);
    }
}

/* Run the body clauses for one iteration. `outer` holds the variables bound
 * before the FOREACH (NULL for none). Each iteration starts from a copy of
 * it, so a CREATE in the body is visible to a SET after it, and the outer
 * variables are visible to every clause (#135: SET rejected an outer MATCH
 * variable; MERGE, REMOVE and DELETE bodies were skipped). */
static int execute_foreach_body(cypher_executor *executor, cypher_foreach *foreach,
                               cypher_result *result, variable_map *outer)
{
    variable_map *vm = create_variable_map();
    if (!vm) {
        set_result_error(result, "Failed to create variable map for FOREACH");
        return -1;
    }
    fold_var_map(vm, outer);

    int rc = 0;
    for (int j = 0; j < foreach->body->count && rc >= 0; j++) {
        ast_node *clause = foreach->body->items[j];
        if (!clause) continue;

        switch (clause->type) {
            case AST_NODE_CREATE: {
                variable_map *cm = create_variable_map();
                if (cm) fold_var_map(cm, vm);
                rc = execute_create_clause_with_varmap(executor, (cypher_create*)clause, result, &cm);
                if (cm) { fold_var_map(vm, cm); free_variable_map(cm); }
                break;
            }
            case AST_NODE_MERGE: {
                variable_map *out = NULL;
                rc = execute_merge_clause(executor, (cypher_merge*)clause, result, vm, &out);
                if (out) { fold_var_map(vm, out); free_variable_map(out); }
                break;
            }
            case AST_NODE_SET:
                rc = execute_set_operations(executor, (cypher_set*)clause, vm, result);
                break;
            case AST_NODE_REMOVE:
                rc = execute_remove_operations(executor, (cypher_remove*)clause, vm, result);
                break;
            case AST_NODE_DELETE:
                rc = execute_delete_operations(executor, (cypher_delete*)clause, vm, result);
                break;
            case AST_NODE_FOREACH:
                /* Nested FOREACH sees this iteration's variables */
                rc = execute_foreach_clause_vm(executor, (cypher_foreach*)clause, vm, result);
                break;
            default:
                set_result_error(result, "Unsupported clause in FOREACH body");
                rc = -1;
                break;
        }
    }
    free_variable_map(vm);
    return rc;
}

/* Execute FOREACH clause - iterate over list and execute body clauses */
int execute_foreach_clause(cypher_executor *executor, cypher_foreach *foreach, cypher_result *result)
{
    return execute_foreach_clause_vm(executor, foreach, NULL, result);
}

/* FOREACH with the variables bound before it (`outer`, may be NULL). */
int execute_foreach_clause_vm(cypher_executor *executor, cypher_foreach *foreach,
                              variable_map *outer, cypher_result *result)
{
    if (!executor || !foreach || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing FOREACH clause, variable=%s", foreach->variable ? foreach->variable : "<null>");

    if (!foreach->variable || !foreach->list_expr || !foreach->body) {
        set_result_error(result, "FOREACH clause missing required elements");
        return -1;
    }

    /* The loop variable is added to the bindings of an enclosing FOREACH
     * or UNWIND, so they stay visible in the body. */
    foreach_context *ctx = create_foreach_context();
    if (!ctx) {
        set_result_error(result, "Failed to create foreach context");
        return -1;
    }
    foreach_context *prev_ctx = g_foreach_ctx;
    if (prev_ctx) {
        for (int i = 0; i < prev_ctx->count; i++) {
            foreach_binding *b = &prev_ctx->bindings[i];
            switch (b->literal_type) {
                case LITERAL_INTEGER: set_foreach_binding_int(ctx, b->variable, b->value.integer); break;
                case LITERAL_DECIMAL: set_foreach_binding_real(ctx, b->variable, b->value.decimal); break;
                case LITERAL_BOOLEAN: set_foreach_binding_bool(ctx, b->variable, b->value.boolean); break;
                case LITERAL_STRING:
                    if (b->is_json) set_foreach_binding_json(ctx, b->variable, b->value.string);
                    else set_foreach_binding_string(ctx, b->variable, b->value.string);
                    break;
                default: set_foreach_binding_null(ctx, b->variable); break;
            }
        }
    }
    g_foreach_ctx = ctx;

    /* A list literal of literals binds each item with its own type. A list
     * with any other item (`[a]`, `[x + 1]`) is evaluated as a whole below,
     * so an item that is an entity is bound as one. */
    bool literal_list = foreach->list_expr->type == AST_NODE_LIST;
    if (literal_list) {
        cypher_list *list = (cypher_list*)foreach->list_expr;
        for (int i = 0; list->items && i < list->items->count; i++) {
            ast_node *item = list->items->items[i];
            if (item && item->type == AST_NODE_RETURN_ITEM) item = ((cypher_return_item*)item)->expr;
            if (!item || item->type != AST_NODE_LITERAL) { literal_list = false; break; }
        }
    }

    int rc = 0;
    if (literal_list) {
        cypher_list *list = (cypher_list*)foreach->list_expr;
        for (int i = 0; list->items && i < list->items->count && rc >= 0; i++) {
            ast_node *item = list->items->items[i];
            if (item->type == AST_NODE_RETURN_ITEM) {
                item = ((cypher_return_item*)item)->expr;
            }
            if (bind_foreach_item_ast(ctx, foreach->variable, item) < 0) {
                set_result_error(result, "Failed to bind FOREACH list item");
                rc = -1;
                break;
            }
            rc = execute_foreach_body(executor, foreach, result, outer);
        }
    } else {
        /* Evaluate the list expression (range(), $param, a variable ...) and
         * iterate its JSON with each value's type (#135: a $param list was
         * evaluated without its parameters and created nothing). */
        char *json = NULL;
        if (evaluate_list_expression(executor, foreach->list_expr, outer, &json) < 0) {
            set_result_error(result, "Failed to evaluate FOREACH list expression");
            rc = -1;
        } else if (json) {
            /* Columns 2 and 3: the id and kind of an item that is a node or
             * a relationship (GQLITE-T-0423: `FOREACH (n IN nodes(p) | SET
             * n.x = 1)` bound n as a map, and SET found no entity). */
            sqlite3_stmt *st = NULL;
            if (sqlite3_prepare_v2(executor->db,
                    "SELECT value, type, "
                    "CASE WHEN type = 'object' THEN json_extract(value, '$.id') END, "
                    "CASE WHEN type <> 'object' OR json_type(value, '$.properties') <> 'object' THEN NULL "
                    "WHEN json_type(value, '$.labels') = 'array' THEN 'node' "
                    "WHEN json_type(value, '$.type') = 'text' AND json_type(value, '$.startNode') IS NOT NULL "
                    "THEN 'edge' END "
                    "FROM json_each(?1) WHERE json_valid(?1) AND json_type(?1) = 'array'",
                    -1, &st, NULL) != SQLITE_OK) {
                set_result_error(result, "Failed to iterate FOREACH list");
                rc = -1;
            } else {
                sqlite3_bind_text(st, 1, json, -1, SQLITE_TRANSIENT);
                while (rc >= 0 && sqlite3_step(st) == SQLITE_ROW) {
                    bind_foreach_item_json_each(ctx, foreach->variable, st, 0, 1);
                    const char *kind = (const char *)sqlite3_column_text(st, 3);
                    if (kind && sqlite3_column_type(st, 2) == SQLITE_INTEGER) {
                        variable_map *iter = create_variable_map();
                        if (!iter) { rc = -1; break; }
                        fold_var_map(iter, outer);
                        int eid = sqlite3_column_int(st, 2);
                        if (kind[0] == 'n') set_variable_node_id(iter, foreach->variable, eid);
                        else set_variable_edge_id(iter, foreach->variable, eid);
                        rc = execute_foreach_body(executor, foreach, result, iter);
                        free_variable_map(iter);
                    } else {
                        rc = execute_foreach_body(executor, foreach, result, outer);
                    }
                }
                sqlite3_finalize(st);
            }
            free(json);
        }
    }

    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);
    return rc < 0 ? -1 : 0;
}

/* Evaluate a list expression to JSON text. *json_out is owned; NULL for
 * a null list. The bindings of an enclosing FOREACH/UNWIND and the query
 * parameters are visible. */
static int evaluate_list_expression(cypher_executor *executor, ast_node *expr,
                                   variable_map *outer, char **json_out)
{
    if (!executor || !expr || !json_out) {
        return -1;
    }
    *json_out = NULL;

    /* With outer variables (MATCH entities), evaluate against them, so
     * `nodes(p)`, `collect(...)` columns and `n.list` resolve. */
    if (outer && outer->count > 0) {
        property_type pt = PROP_TYPE_TEXT;
        property_value pv;
        property_value_init(&pv);
        int erc = executor_eval_value(executor, expr, outer, &pt, &pv);
        if (erc == -2) { property_value_free(&pv); return 0; }
        if (erc < 0) { property_value_free(&pv); return -1; }
        if (pt == PROP_TYPE_JSON || pt == PROP_TYPE_TEXT) {
            *json_out = pv.as_str;
            pv.as_str = NULL;
        }
        property_value_free(&pv);
        return 0;
    }

    cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
    if (!ctx) {
        return -1;
    }
    executor_register_foreach_values(ctx);
    char *sql_expr = cypher_transform_capture_expression(ctx, expr);
    cypher_transform_free_context(ctx);
    if (!sql_expr) {
        return -1;
    }
    char *sql = sqlite3_mprintf("SELECT %s", sql_expr);
    free(sql_expr);
    if (!sql) return -1;

    CYPHER_DEBUG("FOREACH evaluating expression: %s", sql);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare FOREACH expression: %s", sqlite3_errmsg(executor->db));
        return -1;
    }
    if (executor->params_json) bind_params_from_json(stmt, executor->params_json);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        if (text) *json_out = strdup(text);
    }
    sqlite3_finalize(stmt);
    return 0;
}
