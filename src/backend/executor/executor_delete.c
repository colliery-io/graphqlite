/*
 * DELETE Clause Execution
 * Handles MATCH+DELETE query execution and entity deletion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

/* Execute MATCH+DELETE query combination */
int execute_match_delete_query(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause, cypher_result *result)
{
    if (!executor || !match || !delete_clause || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing MATCH+DELETE query");

    /* Step 1: Execute MATCH to find entities to delete */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }

    /* Transform MATCH clause to build SELECT query */
    if (transform_match_clause(ctx, match) < 0) {
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Following AGE's approach: execute MATCH first to get a result set,
     * then iterate through each row and delete the specified entities */

    /* Create a synthetic RETURN clause for the variables to delete */
    cypher_return *synthetic_return = calloc(1, sizeof(cypher_return));
    if (!synthetic_return) {
        set_result_error(result, "Failed to allocate memory for DELETE processing");
        cypher_transform_free_context(ctx);
        return -1;
    }

    synthetic_return->base.type = AST_NODE_RETURN;
    synthetic_return->items = ast_list_create();
    synthetic_return->distinct = false;
    synthetic_return->order_by = NULL;
    synthetic_return->limit = NULL;
    synthetic_return->skip = NULL;

    /* Add RETURN items for each variable to delete */
    for (int i = 0; i < delete_clause->items->count; i++) {
        cypher_delete_item *del_item = (cypher_delete_item*)delete_clause->items->items[i];
        if (del_item && del_item->variable) {
            /* Create identifier for the variable */
            cypher_identifier *id = calloc(1, sizeof(cypher_identifier));
            id->base.type = AST_NODE_IDENTIFIER;
            id->name = strdup(del_item->variable);

            /* Create return item */
            cypher_return_item *ret_item = calloc(1, sizeof(cypher_return_item));
            ret_item->base.type = AST_NODE_RETURN_ITEM;
            ret_item->expr = (ast_node*)id;
            ret_item->alias = NULL;

            ast_list_append(synthetic_return->items, (ast_node*)ret_item);
        }
    }

    /* Execute the MATCH query to get entities */
    cypher_result *match_result = create_empty_result();
    if (execute_match_return_query(executor, match, synthetic_return, match_result) < 0) {
        set_result_error(result, "Failed to execute MATCH for DELETE");
        cypher_transform_free_context(ctx);
        cypher_result_free(match_result);
        /* Clean up synthetic return - ast_list_free handles freeing items */
        if (synthetic_return->items) {
            ast_list_free(synthetic_return->items);
        }
        free(synthetic_return);
        return -1;
    }

    /* Process each entity found by the MATCH and delete it */
    int deleted_nodes = 0, deleted_edges = 0;

    /* Following AGE's process_delete_list pattern */
    for (int i = 0; i < delete_clause->items->count; i++) {
        cypher_delete_item *item = (cypher_delete_item*)delete_clause->items->items[i];
        if (!item || !item->variable) continue;

        /* Check if this variable is an edge or node */
        /* bool is_edge = is_edge_variable(ctx, item->variable); -- not needed, we check entity type */

        /* For each variable to delete, we need to find its value in the MATCH results */
        /* AGE uses entity_position but we'll find by variable name */
        for (int row = 0; row < match_result->row_count; row++) {
            for (int col = 0; col < match_result->column_count; col++) {
                if (match_result->column_names[col] &&
                    strcmp(match_result->column_names[col], item->variable) == 0) {

                    /* Found the variable's column - get the entity */
                    if (match_result->agtype_data && match_result->agtype_data[row][col]) {
                        agtype_value *entity = match_result->agtype_data[row][col];

                        /* Extract entity following AGE's pattern */

                        if (entity->type == AGTV_VERTEX) {
                            /* For vertex, use the entity structure */
                            int64_t entity_id = entity->val.entity.id;

                            CYPHER_DEBUG("Deleting node '%s' with ID %lld", item->variable, entity_id);

                            int delete_result = delete_node_by_id(executor, entity_id, delete_clause->detach);
                            if (delete_result == 0) {
                                deleted_nodes++;
                            } else {
                                /* Failed to delete node - likely due to constraint violation */
                                set_result_error(result, "Cannot delete node - it still has relationships");
                                cypher_result_free(match_result);
                                cypher_transform_free_context(ctx);

                                /* Clean up synthetic return - ast_list_free handles freeing items */
                                if (synthetic_return->items) {
                                    ast_list_free(synthetic_return->items);
                                }
                                free(synthetic_return);

                                return -1;
                            }
                        } else if (entity->type == AGTV_EDGE) {
                            /* For edge, use the edge structure */
                            int64_t entity_id = entity->val.edge.id;

                            CYPHER_DEBUG("Deleting edge '%s' with ID %lld", item->variable, entity_id);

                            if (delete_edge_by_id(executor, entity_id) == 0) {
                                deleted_edges++;
                            }
                        }
                    }
                }
            }
        }
    }

    cypher_result_free(match_result);
    cypher_transform_free_context(ctx);

    /* Clean up synthetic return - ast_list_free handles freeing items */
    if (synthetic_return->items) {
        ast_list_free(synthetic_return->items);
    }
    free(synthetic_return);

    /* Set result with deletion counts */
    result->success = true;
    result->nodes_deleted = deleted_nodes;
    result->relationships_deleted = deleted_edges;

    return 0;
}

/* Delete an edge by ID */
int delete_edge_by_id(cypher_executor *executor, int64_t edge_id)
{
    if (!executor || !executor->db) {
        return -1;
    }

    CYPHER_DEBUG("Deleting edge with ID %lld", edge_id);

    /* Delete edge properties first */
    const char *prop_tables[] = {
        "edge_props_text", "edge_props_int", "edge_props_real", "edge_props_bool"
    };

    char sql[256];
    for (int i = 0; i < 4; i++) {
        snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE edge_id = %lld", prop_tables[i], edge_id);
        char *err_msg = NULL;
        int rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("Warning: Failed to delete from %s: %s", prop_tables[i], err_msg ? err_msg : "unknown error");
            if (err_msg) sqlite3_free(err_msg);
        }
    }

    /* Delete the edge itself */
    snprintf(sql, sizeof(sql), "DELETE FROM edges WHERE id = %lld", edge_id);
    char *err_msg = NULL;
    int rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to delete edge: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return -1;
    }

    return 0;
}

/* Delete a node by ID */
int delete_node_by_id(cypher_executor *executor, int64_t node_id, bool detach)
{
    if (!executor || !executor->db) {
        return -1;
    }

    CYPHER_DEBUG("Deleting node with ID %lld (detach: %s)", node_id, detach ? "true" : "false");

    if (detach) {
        /* DETACH DELETE: First delete all connected edges */
        char delete_edges_sql[256];
        snprintf(delete_edges_sql, sizeof(delete_edges_sql),
                 "DELETE FROM edges WHERE source_id = %lld OR target_id = %lld", node_id, node_id);

        char *err_msg = NULL;
        int rc = sqlite3_exec(executor->db, delete_edges_sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("Failed to delete connected edges for node %lld: %s", node_id, err_msg ? err_msg : "unknown error");
            if (err_msg) sqlite3_free(err_msg);
            return -1;
        }
        CYPHER_DEBUG("Deleted all connected edges for node %lld", node_id);
    } else {
        /* Regular DELETE: Check for connected edges (constraint enforcement) */
        char check_sql[256];
        snprintf(check_sql, sizeof(check_sql), "SELECT COUNT(*) FROM edges WHERE source_id = %lld OR target_id = %lld", node_id, node_id);

        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(executor->db, check_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int edge_count = sqlite3_column_int(stmt, 0);
                if (edge_count > 0) {
                    sqlite3_finalize(stmt);
                    CYPHER_DEBUG("Cannot delete node with ID %lld: has %d connected edges", node_id, edge_count);
                    return -1; /* Node has connected edges */
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    /* Delete node properties first */
    const char *prop_tables[] = {
        "node_props_text", "node_props_int", "node_props_real", "node_props_bool"
    };

    char sql[256];
    int rc;
    for (int i = 0; i < 4; i++) {
        snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE node_id = %lld", prop_tables[i], node_id);
        char *err_msg = NULL;
        rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("Warning: Failed to delete from %s: %s", prop_tables[i], err_msg ? err_msg : "unknown error");
            if (err_msg) sqlite3_free(err_msg);
        }
    }

    /* Delete node labels */
    snprintf(sql, sizeof(sql), "DELETE FROM node_labels WHERE node_id = %lld", node_id);
    char *err_msg = NULL;
    rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Warning: Failed to delete node labels: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
    }

    /* Delete the node itself */
    snprintf(sql, sizeof(sql), "DELETE FROM nodes WHERE id = %lld", node_id);
    err_msg = NULL;
    rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to delete node: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return -1;
    }

    return 0;
}
