/*
 * executor_merge_pipeline.c
 *    MERGE+WITH+SET pipeline executor — capture MERGE var_map,
 *    pipe through trailing WITH/SET, project final RETURN.
 *    Moved from query_dispatch.c (I-0040 M2).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "executor/query_patterns.h"
#include "executor/executor_internal.h"
#include "parser/cypher_debug.h"

/*
 * Execute a RETURN clause using known node IDs from a variable_map.
 * Sets up a transform context with synthetic FROM/WHERE targeting
 * the specific node IDs, then uses the standard transform pipeline
 * to generate SELECT projections and build results.
 */
static int merge_with_execute_return(cypher_executor *executor,
                                     cypher_return *ret,
                                     variable_map *var_map,
                                     cypher_result *result)
{
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "MERGE+WITH RETURN: failed to create transform context");
        return -1;
    }

    ctx->query_type = QUERY_TYPE_READ;

    /* Register each variable from var_map in the transform context
     * and set up FROM/WHERE to select by known entity IDs */
    bool first_var = true;
    for (int i = 0; i < var_map->count; i++) {
        variable_mapping *m = &var_map->mappings[i];

        char alias[64];
        snprintf(alias, sizeof(alias), "_gql_var_%d", i);
        char where_cond[128];
        snprintf(where_cond, sizeof(where_cond), "%s.id = %d", alias, m->entity_id);

        const char *table = NULL;
        if (m->type == VAR_MAP_TYPE_NODE) {
            transform_var_register_node(ctx->var_ctx, m->variable, alias, NULL);
            table = get_graph_table(ctx, "nodes");
        } else if (m->type == VAR_MAP_TYPE_EDGE) {
            transform_var_register_edge(ctx->var_ctx, m->variable, alias, NULL);
            table = get_graph_table(ctx, "edges");
        } else {
            continue;
        }
        transform_var_set_bound(ctx->var_ctx, m->variable, true);

        if (first_var) {
            sql_from(ctx->unified_builder, table, alias);
            first_var = false;
        } else {
            sql_join(ctx->unified_builder, SQL_JOIN_CROSS, table, alias, NULL);
        }
        sql_where(ctx->unified_builder, where_cond);
    }

    if (first_var) {
        cypher_transform_free_context(ctx);
        set_result_error(result, "MERGE+WITH RETURN: no node or edge variables to return");
        return -1;
    }

    /* Transform the RETURN clause (generates SELECT projections) */
    if (transform_return_clause(ctx, ret) < 0) {
        const char *msg = ctx->error_message ? ctx->error_message
                                             : "MERGE+WITH RETURN: failed to transform RETURN clause";
        set_result_error(result, msg);
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* T-0311 (E2): finalize was previously inside transform_return_clause;
     * now caller-side. */
    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "MERGE+WITH RETURN: failed to finalize SQL");
        cypher_transform_free_context(ctx);
        return -1;
    }

    prepend_cte_to_sql(ctx);
    CYPHER_DEBUG("MERGE+WITH RETURN SQL: %s", ctx->sql_buffer);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error[512];
        snprintf(error, sizeof(error), "MERGE+WITH RETURN: SQL prepare failed: %s",
                 sqlite3_errmsg(executor->db));
        set_result_error(result, error);
        cypher_transform_free_context(ctx);
        return -1;
    }

    if (executor->params_json) {
        if (bind_params_from_json(stmt, executor->params_json) < 0) {
            set_result_error(result, "MERGE+WITH RETURN: failed to bind parameters");
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }
    }

    if (build_query_results(executor, stmt, ret, result, ctx) < 0) {
        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);
        return -1;
    }

    result->success = true;
    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);
    return 0;
}

/* Pipeline handler for MERGE + WITH + subsequent clauses.
 * Splits execution at the WITH boundary:
 * 1. Execute pre-WITH clauses (MERGE + SET) as a standalone MERGE
 * 2. Re-resolve MERGE'd node IDs by pattern
 * 3. Dispatch post-WITH clauses as a new sub-query */
int handle_merge_with_pipeline(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags)
{
    (void)flags;
    CYPHER_DEBUG("Executing MERGE+WITH pipeline");

    if (!query || !query->clauses) {
        set_result_error(result, "Invalid query in MERGE+WITH pipeline");
        return -1;
    }

    /* Find the WITH clause position to split */
    int with_pos = -1;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_WITH) {
            with_pos = i;
            break;
        }
    }

    if (with_pos < 0) {
        set_result_error(result, "MERGE+WITH pipeline: no WITH clause found");
        return -1;
    }

    /* --- Phase 1: Execute pre-WITH clauses (MERGE + SET) --- */
    cypher_merge *merge = NULL;
    for (int i = 0; i < with_pos; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MERGE) {
            merge = (cypher_merge*)query->clauses->items[i];
            break;
        }
    }

    if (!merge) {
        /* No MERGE before WITH — this is a MATCH+WITH+...+MERGE pattern.
         * Execute all MATCH clauses to resolve variables, then execute
         * the post-WITH MERGE with those bindings. */
        CYPHER_DEBUG("MERGE+WITH pipeline: MERGE is after WITH, using match-then-merge strategy");

        /* Find the post-WITH MERGE */
        cypher_merge *post_merge = NULL;
        for (int i = with_pos + 1; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type == AST_NODE_MERGE) {
                post_merge = (cypher_merge*)query->clauses->items[i];
                break;
            }
        }
        if (!post_merge) {
            set_result_error(result, "MERGE+WITH pipeline: no MERGE clause found");
            return -1;
        }

        /* Execute all MATCH clauses (pre and post-WITH) to resolve variables */
        variable_map *resolved_vars = create_variable_map();
        if (!resolved_vars) {
            set_result_error(result, "Failed to create variable map");
            return -1;
        }

        for (int i = 0; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type != AST_NODE_MATCH) continue;
            cypher_match *m = (cypher_match*)query->clauses->items[i];

            cypher_transform_context *mctx = cypher_transform_create_context(executor->db);
            if (!mctx) continue;

            if (transform_match_clause(mctx, m) == 0) {
                /* Build SELECT alias.id AS varname_id FROM ... WHERE ... */
                int vcount = transform_var_count(mctx->var_ctx);
                const char *from_str = sql_builder_get_from(mctx->unified_builder);
                const char *joins_str = sql_builder_get_joins(mctx->unified_builder);
                const char *where_str = sql_builder_get_where(mctx->unified_builder);

                char id_sql[4096];
                size_t pos = 0;
                pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, "SELECT ");
                bool first_col = true;
                for (int vi = 0; vi < vcount; vi++) {
                    transform_var *tv = transform_var_at(mctx->var_ctx, vi);
                    if (tv && tv->kind == VAR_KIND_NODE) {
                        if (!first_col) pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, ", ");
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos,
                                        "%s.id AS \"%s_id\"", tv->table_alias, tv->name);
                        first_col = false;
                    }
                }
                if (!first_col) {
                    if (from_str && from_str[0])
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " FROM %s", from_str);
                    if (joins_str && joins_str[0])
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " %s", joins_str);
                    if (where_str && where_str[0])
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " WHERE %s", where_str);

                    sqlite3_stmt *match_stmt;
                    if (sqlite3_prepare_v2(executor->db, id_sql, -1, &match_stmt, NULL) == SQLITE_OK) {
                        if (sqlite3_step(match_stmt) == SQLITE_ROW) {
                            int mcols = sqlite3_column_count(match_stmt);
                            for (int mc = 0; mc < mcols; mc++) {
                                const char *cname = sqlite3_column_name(match_stmt, mc);
                                if (cname && sqlite3_column_type(match_stmt, mc) == SQLITE_INTEGER) {
                                    char var_name[128];
                                    strncpy(var_name, cname, sizeof(var_name) - 1);
                                    var_name[sizeof(var_name) - 1] = '\0';
                                    char *suffix = strstr(var_name, "_id");
                                    if (suffix) {
                                        *suffix = '\0';
                                        set_variable_node_id(resolved_vars, var_name,
                                            sqlite3_column_int(match_stmt, mc));
                                        CYPHER_DEBUG("WITH+MERGE: resolved '%s' = node %d",
                                                     var_name, sqlite3_column_int(match_stmt, mc));
                                    }
                                }
                            }
                        }
                        sqlite3_finalize(match_stmt);
                    }
                }
            }
            cypher_transform_free_context(mctx);
        }

        /* Execute MERGE with resolved variable bindings */
        int rc = execute_merge_clause(executor, post_merge, result, resolved_vars, NULL);

        /* Execute any post-WITH SET clauses */
        for (int i = with_pos + 1; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type == AST_NODE_SET) {
                execute_set_operations(executor, (cypher_set*)query->clauses->items[i],
                                       resolved_vars, result);
            }
        }

        free_variable_map(resolved_vars);
        if (rc < 0) return -1;
        result->success = true;
        return 0;
    }

    /* Execute the MERGE clause (handles ON CREATE SET internally) */
    int rc = execute_merge_clause(executor, merge, result, NULL, NULL);
    if (rc < 0) return -1;

    /* Execute any standalone SET clauses between MERGE and WITH */
    for (int i = 0; i < with_pos; i++) {
        if (query->clauses->items[i]->type == AST_NODE_SET) {
            cypher_set *set_clause = (cypher_set*)query->clauses->items[i];

            /* Build variable map from MERGE pattern by re-resolving node IDs */
            variable_map *var_map = create_variable_map();
            if (!var_map) {
                set_result_error(result, "Failed to create variable map for SET");
                return -1;
            }

            for (int p = 0; p < merge->pattern->count; p++) {
                ast_node *pat = merge->pattern->items[p];
                if (pat->type != AST_NODE_PATH) continue;
                cypher_path *path = (cypher_path*)pat;
                for (int j = 0; j < path->elements->count; j++) {
                    ast_node *elem = path->elements->items[j];
                    if (elem->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *np = (cypher_node_pattern*)elem;
                        if (np->variable) {
                            int node_id = find_node_by_pattern(executor, np);
                            if (node_id >= 0) {
                                set_variable_node_id(var_map, np->variable, node_id);
                            }
                        }
                    }
                }
            }

            rc = execute_set_operations(executor, set_clause, var_map, result);
            free_variable_map(var_map);
            if (rc < 0) return -1;
        }
    }

    /* --- Phase 2: Execute post-WITH clauses --- */

    /* Build a variable map with all MERGE'd node IDs for the post-WITH phase */
    variable_map *post_var_map = create_variable_map();
    if (!post_var_map) {
        set_result_error(result, "Failed to create post-WITH variable map");
        return -1;
    }

    for (int p = 0; p < merge->pattern->count; p++) {
        ast_node *pat = merge->pattern->items[p];
        if (pat->type != AST_NODE_PATH) continue;
        cypher_path *path = (cypher_path*)pat;
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *elem = path->elements->items[j];
            if (elem->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern*)elem;
                if (np->variable) {
                    int node_id = find_node_by_pattern(executor, np);
                    if (node_id >= 0) {
                        set_variable_node_id(post_var_map, np->variable, node_id);
                        CYPHER_DEBUG("MERGE+WITH pipeline: carrying variable '%s' = node %d", np->variable, node_id);
                    }
                }
            } else if (elem->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern*)elem;
                if (rp->variable && j > 0 && j + 1 < path->elements->count) {
                    ast_node *src_elem = path->elements->items[j - 1];
                    ast_node *tgt_elem = path->elements->items[j + 1];
                    if (src_elem->type == AST_NODE_NODE_PATTERN &&
                        tgt_elem->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *src_np = (cypher_node_pattern*)src_elem;
                        cypher_node_pattern *tgt_np = (cypher_node_pattern*)tgt_elem;
                        int src_id = src_np->variable ? get_variable_node_id(post_var_map, src_np->variable) : -1;
                        if (src_id < 0) src_id = find_node_by_pattern(executor, src_np);
                        int tgt_id = tgt_np->variable ? get_variable_node_id(post_var_map, tgt_np->variable) : -1;
                        if (tgt_id < 0) tgt_id = find_node_by_pattern(executor, tgt_np);
                        if (src_id >= 0 && tgt_id >= 0) {
                            int source_id = src_id, dest_id = tgt_id;
                            if (rp->left_arrow && !rp->right_arrow) {
                                source_id = tgt_id;
                                dest_id = src_id;
                            }
                            const char *rel_type = rp->type ? rp->type : "RELATED";
                            int edge_id = find_edge_by_pattern(executor, source_id, dest_id, rel_type, rp);
                            if (edge_id >= 0) {
                                set_variable_edge_id(post_var_map, rp->variable, edge_id);
                                CYPHER_DEBUG("MERGE+WITH pipeline: carrying edge variable '%s' = edge %d", rp->variable, edge_id);
                            }
                        }
                    }
                }
            }
        }
    }

    /* Find post-WITH clauses */
    cypher_match *post_match = NULL;
    cypher_merge *post_merge = NULL;
    cypher_return *post_return = NULL;
    bool has_post_set = false;
    for (int i = with_pos + 1; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH && !post_match) post_match = (cypher_match*)clause;
        if (clause->type == AST_NODE_MERGE && !post_merge) post_merge = (cypher_merge*)clause;
        if (clause->type == AST_NODE_RETURN && !post_return) post_return = (cypher_return*)clause;
        if (clause->type == AST_NODE_SET) has_post_set = true;
    }

    if (post_match && post_merge) {
        /* Execute MATCH to find additional variables, then MERGE with combined var_map */
        /* Transform MATCH to SQL and execute to get matched node IDs */
        cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
        if (!ctx) {
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to create transform context for post-WITH");
            return -1;
        }

        if (transform_match_clause(ctx, post_match) < 0) {
            cypher_transform_free_context(ctx);
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to transform post-WITH MATCH clause");
            return -1;
        }

        if (finalize_sql_generation(ctx) < 0) {
            cypher_transform_free_context(ctx);
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to finalize post-WITH SQL");
            return -1;
        }

        /* Build SELECT with node variable IDs */
        char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
        if (select_pos) {
            char *after_star = select_pos + strlen("SELECT *");
            char *temp = strdup(after_star);
            ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
            ctx->sql_buffer[ctx->sql_size] = '\0';

            bool first = true;
            int var_count = transform_var_count(ctx->var_ctx);
            for (int vi = 0; vi < var_count; vi++) {
                transform_var *var = transform_var_at(ctx->var_ctx, vi);
                if (var && var->kind == VAR_KIND_NODE) {
                    if (!first) append_sql(ctx, ", ");
                    append_sql(ctx, "%s.id AS \"%s_id\"", var->table_alias, var->name);
                    first = false;
                }
            }
            append_sql(ctx, " %s", temp);
            free(temp);
        }

        CYPHER_DEBUG("MERGE+WITH pipeline: post-WITH MATCH SQL: %s", ctx->sql_buffer);

        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            cypher_transform_free_context(ctx);
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to prepare post-WITH MATCH SQL");
            return -1;
        }

        if (executor->params_json) {
            if (bind_params_from_json(stmt, executor->params_json) < 0) {
                set_result_error(result, "MERGE+WITH pipeline: failed to bind parameters");
                sqlite3_finalize(stmt);
                cypher_transform_free_context(ctx);
                free_variable_map(post_var_map);
                return -1;
            }
        }

        /* Execute MATCH and add found variables to our map */
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int col_count = sqlite3_column_count(stmt);
            for (int c = 0; c < col_count; c++) {
                const char *col_name = sqlite3_column_name(stmt, c);
                if (col_name) {
                    /* Column names are "varname_id" */
                    char var_name[128];
                    strncpy(var_name, col_name, sizeof(var_name) - 1);
                    var_name[sizeof(var_name) - 1] = '\0';
                    char *suffix = strstr(var_name, "_id");
                    if (suffix) {
                        *suffix = '\0';
                        int node_id = sqlite3_column_int(stmt, c);
                        /* Only add if not already in map (MERGE variables take priority) */
                        if (get_variable_node_id(post_var_map, var_name) < 0) {
                            set_variable_node_id(post_var_map, var_name, node_id);
                            CYPHER_DEBUG("MERGE+WITH pipeline: MATCH found '%s' = node %d", var_name, node_id);
                        }
                    }
                }
            }
        }

        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);

        /* Now execute the post-WITH MERGE with the combined variable map */
        rc = execute_merge_with_variables(executor, post_merge, post_var_map, result);
    } else {
        /* Handle post-WITH SET and/or RETURN */

        /* Execute all post-WITH SET clauses */
        for (int i = with_pos + 1; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type == AST_NODE_SET) {
                cypher_set *set_clause = (cypher_set*)query->clauses->items[i];
                rc = execute_set_operations(executor, set_clause, post_var_map, result);
                if (rc < 0) {
                    free_variable_map(post_var_map);
                    return -1;
                }
            }
        }

        /* Execute post-WITH RETURN if present */
        if (post_return) {
            rc = merge_with_execute_return(executor, post_return, post_var_map, result);
            if (rc < 0) {
                free_variable_map(post_var_map);
                return -1;
            }
        } else if (has_post_set) {
            /* SET without RETURN: mark success */
            result->success = true;
            rc = 0;
        } else {
            set_result_error(result, "MERGE+WITH pipeline: unsupported post-WITH clause combination");
            rc = -1;
        }
    }

    free_variable_map(post_var_map);

    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

