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

/* Perf review F5: entity JSON is passed through as text, so DELETE reads the
 * id and kind straight from the object. json_object() emits the id first and
 * then either "labels" (node) or "type" (edge), with no whitespace. */
static bool entity_ref_from_json(const char *txt, int64_t *id_out, bool *is_edge_out)
{
    if (!txt || txt[0] != '{') return false;
    const char *p = strstr(txt, "\"id\":");
    if (!p) return false;
    p += 5;
    char *end = NULL;
    long long v = strtoll(p, &end, 10);
    if (end == p) return false;
    *id_out = (int64_t)v;
    if (strncmp(end, ",\"labels\"", 9) == 0) { *is_edge_out = false; return true; }
    if (strncmp(end, ",\"type\"", 7) == 0) { *is_edge_out = true; return true; }
    return false;
}

/* Build a synthetic `RETURN v1, v2, ...` over the variables named by the
 * DELETE items, so the MATCH can be executed once and the bound entities read
 * from the result (entity JSON pass-through, perf review F5). Caller frees
 * with free_delete_synthetic_return(). */
static cypher_return *build_delete_synthetic_return(cypher_delete *delete_clause)
{
    cypher_return *synthetic_return = calloc(1, sizeof(cypher_return));
    if (!synthetic_return) return NULL;

    synthetic_return->base.type = AST_NODE_RETURN;
    synthetic_return->items = ast_list_create();
    synthetic_return->distinct = false;
    synthetic_return->order_by = NULL;
    synthetic_return->limit = NULL;
    synthetic_return->skip = NULL;

    for (int i = 0; i < delete_clause->items->count; i++) {
        cypher_delete_item *del_item = (cypher_delete_item*)delete_clause->items->items[i];
        if (del_item && del_item->variable) {
            cypher_identifier *id = calloc(1, sizeof(cypher_identifier));
            id->base.type = AST_NODE_IDENTIFIER;
            id->name = strdup(del_item->variable);

            cypher_return_item *ret_item = calloc(1, sizeof(cypher_return_item));
            ret_item->base.type = AST_NODE_RETURN_ITEM;
            ret_item->expr = (ast_node*)id;
            ret_item->alias = NULL;

            ast_list_append(synthetic_return->items, (ast_node*)ret_item);
        }
    }
    return synthetic_return;
}

static void free_delete_synthetic_return(cypher_return *synthetic_return)
{
    if (!synthetic_return) return;
    /* ast_list_free handles freeing items */
    if (synthetic_return->items) {
        ast_list_free(synthetic_return->items);
    }
    free(synthetic_return);
}

/* GQLITE-T-0253: does the MATCH bind at least one non-null entity for any
 * of the DELETE targets? Used before a RETURN that reads data of a deleted
 * variable: openCypher raises EntityNotFound: DeletedEntityAccess only when
 * an entity was actually deleted (a null from an OPTIONAL MATCH miss is just
 * null). Returns 1 if bound, 0 if nothing is bound, -1 on error. */
int delete_targets_bound(cypher_executor *executor, cypher_match *match,
                         cypher_delete *delete_clause)
{
    if (!executor || !match || !delete_clause || !delete_clause->items) return -1;

    cypher_return *synthetic_return = build_delete_synthetic_return(delete_clause);
    if (!synthetic_return) return -1;

    cypher_result *match_result = create_empty_result();
    int rc = execute_match_return_query(executor, match, synthetic_return, match_result);
    int bound = 0;
    if (rc < 0) {
        bound = -1;
    } else {
        for (int row = 0; row < match_result->row_count && !bound; row++) {
            for (int col = 0; col < match_result->column_count; col++) {
                bool non_null = false;
                if (match_result->agtype_data && match_result->agtype_data[row][col]) {
                    agtype_value *v = match_result->agtype_data[row][col];
                    non_null = (v->type == AGTV_VERTEX || v->type == AGTV_EDGE);
                } else if (match_result->data && match_result->data[row] &&
                           match_result->data[row][col]) {
                    int64_t id; bool is_edge;
                    non_null = entity_ref_from_json(match_result->data[row][col], &id, &is_edge);
                }
                if (non_null) { bound = 1; break; }
            }
        }
    }
    cypher_result_free(match_result);
    free_delete_synthetic_return(synthetic_return);
    return bound;
}

/* Execute MATCH+DELETE query combination */
int execute_match_delete_query(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause, cypher_result *result)
{
    if (!executor || !match || !delete_clause || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing MATCH+DELETE query");

    /* Following AGE's approach: execute MATCH first to get a result set,
     * then iterate through each row and delete the specified entities.
     * Note: We don't transform MATCH here - execute_match_return_query does that.
     * Transforming twice would mutate the AST (GQLITE-T-0092). */

    /* Create a synthetic RETURN clause for the variables to delete */
    cypher_return *synthetic_return = build_delete_synthetic_return(delete_clause);
    if (!synthetic_return) {
        set_result_error(result, "Failed to allocate memory for DELETE processing");
        return -1;
    }

    /* Execute the MATCH query to get entities */
    cypher_result *match_result = create_empty_result();
    if (execute_match_return_query(executor, match, synthetic_return, match_result) < 0) {
        set_result_error(result, "Failed to execute MATCH for DELETE");
        cypher_result_free(match_result);
        free_delete_synthetic_return(synthetic_return);
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

                    /* Found the variable's column - get the entity. The
                     * agtype cell is set only for legacy id-only values;
                     * entity JSON is passed through as text (perf review F5),
                     * so derive the id/kind from the JSON in that case. */
                    int64_t entity_id = -1;
                    bool entity_is_edge = false;
                    bool entity_known = false;
                    if (match_result->agtype_data && match_result->agtype_data[row][col]) {
                        agtype_value *entity = match_result->agtype_data[row][col];
                        if (entity->type == AGTV_VERTEX) {
                            entity_id = entity->val.entity.id;
                            entity_known = true;
                        } else if (entity->type == AGTV_EDGE) {
                            entity_id = entity->val.edge.id;
                            entity_is_edge = true;
                            entity_known = true;
                        }
                    } else if (match_result->data && match_result->data[row] &&
                               match_result->data[row][col]) {
                        entity_known = entity_ref_from_json(match_result->data[row][col],
                                                            &entity_id, &entity_is_edge);
                    }
                    if (entity_known) {
                        if (!entity_is_edge) {

                            CYPHER_DEBUG("Deleting node '%s' with ID %lld", item->variable, entity_id);

                            int detached = 0;
                            int delete_result = delete_node_by_id(executor, entity_id, delete_clause->detach, &detached);
                            if (delete_result == 0) {
                                deleted_nodes++;
                                deleted_edges += detached;
                            } else {
                                /* Failed to delete node - likely due to constraint violation */
                                set_result_error(result, "Cannot delete node - it still has relationships");
                                cypher_result_free(match_result);

                                free_delete_synthetic_return(synthetic_return);

                                return -1;
                            }
                        } else {

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

    free_delete_synthetic_return(synthetic_return);

    /* Set result with deletion counts */
    result->success = true;
    result->nodes_deleted = deleted_nodes;
    result->relationships_deleted = deleted_edges;

    return 0;
}

/* I-0042 E4: DELETE operations against a pre-captured variable map.
 * The caller is responsible for populating var_map (via
 * bind_match_clause_into_varmap or similar). This function only does
 * the per-id deletion side — no MATCH execution, no AST mutation, no
 * synthetic RETURN. Mirrors the shape of execute_set_operations /
 * execute_remove_operations so handle_match_delete can follow the
 * same two-pass pattern. */
int execute_delete_operations(cypher_executor *executor,
                              cypher_delete *del,
                              variable_map *var_map,
                              cypher_result *result)
{
    if (!executor || !del || !var_map || !result) return -1;
    if (!del->items) return 0;

    int deleted_nodes = 0, deleted_edges = 0;
    for (int i = 0; i < del->items->count; i++) {
        cypher_delete_item *item = (cypher_delete_item *)del->items->items[i];
        if (!item || !item->variable) continue;

        /* Try edge first — edges are deleted unconditionally; nodes
         * honor the DETACH flag and may fail on connected nodes. */
        if (is_variable_edge(var_map, item->variable)) {
            int edge_id = get_variable_edge_id(var_map, item->variable);
            if (edge_id < 0) continue;
            if (delete_edge_by_id(executor, (int64_t)edge_id) == 0) {
                deleted_edges++;
            }
        } else {
            int node_id = get_variable_node_id(var_map, item->variable);
            if (node_id < 0) continue;
            int detached = 0;
            int rc = delete_node_by_id(executor, (int64_t)node_id, del->detach, &detached);
            if (rc == 0) {
                deleted_nodes++;
                deleted_edges += detached;
            } else {
                set_result_error(result,
                    "Cannot delete node - it still has relationships");
                return -1;
            }
        }
    }

    result->nodes_deleted += deleted_nodes;
    result->relationships_deleted += deleted_edges;
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
int delete_node_by_id(cypher_executor *executor, int64_t node_id, bool detach,
                      int *detached_edges)
{
    if (detached_edges) *detached_edges = 0;
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
        if (detached_edges) *detached_edges = sqlite3_changes(executor->db);
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
