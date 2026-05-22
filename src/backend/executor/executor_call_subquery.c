/*
 * executor_call_subquery.c
 *    CALL { } subquery executor — handles outer-row iteration,
 *    WITH variable import, UNION branch dispatch.
 *    Moved from query_dispatch.c (I-0040 M1).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "executor/query_patterns.h"
#include "executor/executor_internal.h"
#include "parser/cypher_debug.h"

int handle_call_subquery(cypher_executor *executor, cypher_query *query,
                         cypher_result *result, clause_flags flags)
{
    (void)flags;
    CYPHER_DEBUG("Executing CALL {} subquery");

    if (!query || !query->clauses) {
        set_result_error(result, "Invalid query in CALL subquery handler");
        return -1;
    }

    /* Find the CALL clause position */
    int call_pos = -1;
    cypher_call_subquery *call_node = NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_CALL_SUBQUERY) {
            call_pos = i;
            call_node = (cypher_call_subquery*)query->clauses->items[i];
            break;
        }
    }

    if (call_pos < 0 || !call_node) {
        set_result_error(result, "CALL subquery handler: no CALL clause found");
        return -1;
    }

    /* Get the inner query from the CALL node */
    if (!call_node->branches || call_node->branches->count == 0) {
        set_result_error(result, "CALL subquery has no inner query");
        return -1;
    }

    ast_node *inner_ast = call_node->branches->items[0];

    /*
     * Case 1: Standalone CALL with no outer MATCH
     * e.g., CALL { MATCH (n) RETURN n }
     */
    if (call_pos == 0 && query->clauses->count == 1) {
        CYPHER_DEBUG("Standalone CALL — dispatching inner query directly");
        cypher_result *inner_result = cypher_executor_execute_ast(executor, inner_ast);
        if (!inner_result) {
            set_result_error(result, "Failed to execute CALL subquery");
            return -1;
        }

        /* Copy inner result to outer result */
        result->success = inner_result->success;
        if (inner_result->error_message) {
            result->error_message = strdup(inner_result->error_message);
        }
        result->row_count = inner_result->row_count;
        result->column_count = inner_result->column_count;
        result->nodes_created = inner_result->nodes_created;
        result->nodes_deleted = inner_result->nodes_deleted;
        result->relationships_created = inner_result->relationships_created;
        result->relationships_deleted = inner_result->relationships_deleted;
        result->properties_set = inner_result->properties_set;

        /* Transfer data ownership */
        result->column_names = inner_result->column_names;
        inner_result->column_names = NULL;
        result->data = inner_result->data;
        inner_result->data = NULL;
        result->data_types = inner_result->data_types;
        inner_result->data_types = NULL;
        result->agtype_data = inner_result->agtype_data;
        inner_result->agtype_data = NULL;
        result->use_agtype = inner_result->use_agtype;

        cypher_result_free(inner_result);
        return result->success ? 0 : -1;
    }

    /*
     * Case 2: Outer MATCH + CALL (with or without post-CALL RETURN)
     * e.g., MATCH (a) CALL { WITH a SET a.x = 1 }
     * e.g., MATCH (a) CALL { WITH a MATCH (b) RETURN b } RETURN a, b
     */

    /* Check if there are pre-CALL MATCH clauses */
    bool has_outer_match = false;
    for (int i = 0; i < call_pos; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MATCH) {
            has_outer_match = true;
            break;
        }
    }

    if (!has_outer_match) {
        /* No outer MATCH — just execute the inner query directly
         * This handles CALL { ... } RETURN ... (no outer context) */
        CYPHER_DEBUG("CALL without outer MATCH — executing inner directly");
        cypher_result *inner_result = cypher_executor_execute_ast(executor, inner_ast);
        if (!inner_result || !inner_result->success) {
            const char *err = (inner_result && inner_result->error_message) ?
                              inner_result->error_message : "Failed to execute CALL subquery";
            set_result_error(result, err);
            if (inner_result) cypher_result_free(inner_result);
            return -1;
        }
        result->success = true;
        result->nodes_created += inner_result->nodes_created;
        result->nodes_deleted += inner_result->nodes_deleted;
        result->relationships_created += inner_result->relationships_created;
        result->relationships_deleted += inner_result->relationships_deleted;
        result->properties_set += inner_result->properties_set;
        cypher_result_free(inner_result);
        return 0;
    }

    /*
     * Has outer MATCH — transform pre-CALL clauses to get outer rows,
     * then execute inner subquery per row.
     */
    CYPHER_DEBUG("CALL with outer MATCH at position %d", call_pos);

    /* Build a temporary query from pre-CALL clauses to get outer row data */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context for outer query");
        return -1;
    }

    /* Transform pre-CALL MATCH clauses */
    for (int i = 0; i < call_pos; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH) {
            if (transform_match_clause(ctx, (cypher_match*)clause) < 0) {
                set_result_error(result, ctx->error_message ?
                                ctx->error_message : "Failed to transform outer MATCH");
                cypher_transform_free_context(ctx);
                return -1;
            }
        }
    }

    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "Failed to finalize outer query SQL");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Replace SELECT * with SELECT of node variable IDs */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
        char *after_star = select_pos + strlen("SELECT *");
        char *rest = strdup(after_star);
        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';

        bool first = true;
        int var_count = transform_var_count(ctx->var_ctx);
        for (int vi = 0; vi < var_count; vi++) {
            transform_var *var = transform_var_at(ctx->var_ctx, vi);
            if (var && (var->kind == VAR_KIND_NODE || var->kind == VAR_KIND_EDGE)) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS \"%s_id\"", var->table_alias, var->name);
                first = false;
            }
        }
        if (first) {
            /* No node/edge variables — just select 1 as placeholder */
            append_sql(ctx, "1");
        }
        append_sql(ctx, " %s", rest);
        free(rest);
    }

    prepend_cte_to_sql(ctx);

    CYPHER_DEBUG("CALL outer query SQL: %s", ctx->sql_buffer);

    /* Prepare and execute outer query */
    sqlite3_stmt *outer_stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &outer_stmt, NULL);
    if (rc != SQLITE_OK) {
        set_result_error(result, "Failed to prepare outer query for CALL subquery");
        cypher_transform_free_context(ctx);
        return -1;
    }

    if (executor->params_json) {
        bind_params_from_json(outer_stmt, executor->params_json);
    }

    /* Iterate outer rows and execute inner subquery per row */
    int total_inner_rows = 0;
    result->success = true;

    /* Get the inner query and find its clauses */
    cypher_query *inner_query = NULL;
    if (inner_ast->type == AST_NODE_QUERY || inner_ast->type == AST_NODE_SINGLE_QUERY) {
        inner_query = (cypher_query*)inner_ast;
    }

    /* Check for inner RETURN clause and post-CALL RETURN to enable result accumulation */
    cypher_return *inner_return_clause = NULL;
    bool has_post_call_return = false;
    if (inner_query && inner_query->clauses) {
        for (int ci = 0; ci < inner_query->clauses->count; ci++) {
            if (inner_query->clauses->items[ci]->type == AST_NODE_RETURN) {
                inner_return_clause = (cypher_return*)inner_query->clauses->items[ci];
                break;
            }
        }
    }
    for (int i = call_pos + 1; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_RETURN) {
            has_post_call_return = true;
            break;
        }
    }

    /* Accumulator for combined outer+inner results */
    bool accumulating = (inner_return_clause != NULL && has_post_call_return);
    int accum_capacity = 16;
    int accum_count = 0;
    char ***accum_rows = accumulating ? calloc(accum_capacity, sizeof(char**)) : NULL;
    int accum_col_count = 0;
    char **accum_col_names = NULL;

    /* T-0316 (E7): hoist inner MATCH transform out of the per-outer-
     * row loop. The inner MATCH AST is the same on every outer row;
     * its transform produces id_sql that has no outer-row dependency.
     * Pre-compute the id_sql string per AST_NODE_MATCH clause now;
     * the inner loop just prepares and steps it per row. */
    char **inner_match_sql = NULL;
    int inner_match_count = 0;
    if (inner_query && inner_query->clauses) {
        inner_match_sql = calloc(inner_query->clauses->count, sizeof(char*));
        if (inner_match_sql) {
            for (int ci = 0; ci < inner_query->clauses->count; ci++) {
                ast_node *c = inner_query->clauses->items[ci];
                if (!c || c->type != AST_NODE_MATCH) continue;
                cypher_match *im = (cypher_match*)c;
                cypher_transform_context *mctx = cypher_transform_create_context(executor->db);
                if (!mctx) continue;
                if (transform_match_clause(mctx, im) == 0) {
                    sql_builder *sb = mctx->unified_builder;
                    int vcount = transform_var_count(mctx->var_ctx);
                    const char *from_s = sql_builder_get_from(sb);
                    const char *joins_s = sql_builder_get_joins(sb);
                    const char *where_s = sql_builder_get_where(sb);

                    char buf[4096]; size_t p = 0;
                    p += snprintf(buf + p, sizeof(buf) - p, "SELECT ");
                    bool first = true;
                    for (int vi = 0; vi < vcount; vi++) {
                        transform_var *tv = transform_var_at(mctx->var_ctx, vi);
                        if (tv && tv->kind == VAR_KIND_NODE) {
                            if (!first) p += snprintf(buf + p, sizeof(buf) - p, ", ");
                            p += snprintf(buf + p, sizeof(buf) - p,
                                          "%s.id AS \"%s_id\"", tv->table_alias, tv->name);
                            first = false;
                        }
                    }
                    if (!first) {
                        if (from_s && from_s[0])
                            p += snprintf(buf + p, sizeof(buf) - p, " FROM %s", from_s);
                        if (joins_s && joins_s[0])
                            p += snprintf(buf + p, sizeof(buf) - p, " %s", joins_s);
                        if (where_s && where_s[0])
                            p += snprintf(buf + p, sizeof(buf) - p, " WHERE %s", where_s);
                        inner_match_sql[ci] = strdup(buf);
                        inner_match_count++;
                    }
                }
                cypher_transform_free_context(mctx);
            }
        }
    }

    while (sqlite3_step(outer_stmt) == SQLITE_ROW) {
        CYPHER_DEBUG("CALL: processing outer row");

        /* Build variable map from outer row columns (format: varname_id) */
        variable_map *var_map = create_variable_map();
        if (!var_map) {
            set_result_error(result, "Failed to create variable map for CALL");
            sqlite3_finalize(outer_stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }

        int col_count = sqlite3_column_count(outer_stmt);
        for (int c = 0; c < col_count; c++) {
            const char *col_name = sqlite3_column_name(outer_stmt, c);
            if (col_name && sqlite3_column_type(outer_stmt, c) == SQLITE_INTEGER) {
                char var_name[128];
                strncpy(var_name, col_name, sizeof(var_name) - 1);
                var_name[sizeof(var_name) - 1] = '\0';
                char *suffix = strstr(var_name, "_id");
                if (suffix) {
                    *suffix = '\0';
                    int node_id = sqlite3_column_int(outer_stmt, c);
                    set_variable_node_id(var_map, var_name, node_id);
                    CYPHER_DEBUG("CALL: bound outer variable '%s' = node %d", var_name, node_id);
                }
            }
        }

        /* Build a scoped variable map: only include variables listed in the
         * leading WITH clause (scope isolation per openCypher spec).
         * If there is no leading WITH, no outer variables are accessible. */
        variable_map *scoped_map = create_variable_map();
        if (!scoped_map) {
            free_variable_map(var_map);
            set_result_error(result, "Failed to create scoped variable map");
            sqlite3_finalize(outer_stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }

        if (inner_query && inner_query->clauses && inner_query->clauses->count > 0 &&
            inner_query->clauses->items[0]->type == AST_NODE_WITH) {
            cypher_with *with = (cypher_with*)inner_query->clauses->items[0];
            if (with->items) {
                for (int wi = 0; wi < with->items->count; wi++) {
                    cypher_return_item *item = (cypher_return_item*)with->items->items[wi];
                    /* Simple identifier import: WITH a */
                    if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
                        const char *name = ((cypher_identifier*)item->expr)->name;
                        int node_id = get_variable_node_id(var_map, name);
                        if (node_id >= 0) {
                            const char *alias = item->alias ? item->alias : name;
                            set_variable_node_id(scoped_map, alias, node_id);
                            CYPHER_DEBUG("CALL WITH: imported '%s' as '%s' = node %d",
                                         name, alias, node_id);
                        }
                    }
                    /* WITH expressions (e.g., WITH a.name AS n) are not node IDs —
                     * they would need value-level binding which is a future enhancement.
                     * For now, property expressions are skipped with a debug note. */
                    else if (item->expr) {
                        CYPHER_DEBUG("CALL WITH: skipping non-identifier expression (type %s)",
                                     ast_node_type_name(item->expr->type));
                    }
                }
            }
        }

        /* Execute inner subquery clauses with scoped variable bindings.
         * Skip the leading WITH clause (already processed above). */
        if (inner_query && inner_query->clauses) {
            for (int ci = 0; ci < inner_query->clauses->count; ci++) {
                ast_node *inner_clause = inner_query->clauses->items[ci];

                if (inner_clause->type == AST_NODE_WITH) {
                    /* Already processed above for scope building */
                    continue;
                }

                if (inner_clause->type == AST_NODE_SET) {
                    rc = execute_set_operations(executor, (cypher_set*)inner_clause,
                                                scoped_map, result);
                    if (rc < 0) {
                        free_variable_map(scoped_map);
                        free_variable_map(var_map);
                        sqlite3_finalize(outer_stmt);
                        cypher_transform_free_context(ctx);
                        return -1;
                    }
                } else if (inner_clause->type == AST_NODE_MERGE) {
                    rc = execute_merge_clause(executor, (cypher_merge*)inner_clause, result, scoped_map, NULL);
                    if (rc < 0) {
                        free_variable_map(scoped_map);
                        free_variable_map(var_map);
                        sqlite3_finalize(outer_stmt);
                        cypher_transform_free_context(ctx);
                        return -1;
                    }
                } else if (inner_clause->type == AST_NODE_CREATE) {
                    rc = execute_create_clause(executor, (cypher_create*)inner_clause, result);
                    if (rc < 0) {
                        free_variable_map(scoped_map);
                        free_variable_map(var_map);
                        sqlite3_finalize(outer_stmt);
                        cypher_transform_free_context(ctx);
                        return -1;
                    }
                } else if (inner_clause->type == AST_NODE_MATCH) {
                    /* T-0316: use the pre-built inner MATCH SQL hoisted
                     * before the outer loop (was: per-row create
                     * match_ctx + transform_match_clause + build id_sql).
                     * The MATCH AST is the same across outer rows;
                     * id_sql has no outer-row dependency. Just prepare
                     * and step it per row to get the current matched
                     * IDs (DB state may have changed via per-row SET/
                     * CREATE so the prepared plan must be fresh). */
                    const char *id_sql = (inner_match_sql && ci < inner_query->clauses->count) ? inner_match_sql[ci] : NULL;
                    if (id_sql) {
                        CYPHER_DEBUG("CALL inner MATCH SQL (hoisted): %s", id_sql);
                        sqlite3_stmt *match_stmt;
                        if (sqlite3_prepare_v2(executor->db, id_sql, -1, &match_stmt, NULL) == SQLITE_OK) {
                                if (executor->params_json) {
                                    bind_params_from_json(match_stmt, executor->params_json);
                                }
                                while (sqlite3_step(match_stmt) == SQLITE_ROW) {
                                    int mcols = sqlite3_column_count(match_stmt);
                                    for (int mc = 0; mc < mcols; mc++) {
                                        const char *cname = sqlite3_column_name(match_stmt, mc);
                                        if (cname && sqlite3_column_type(match_stmt, mc) == SQLITE_INTEGER) {
                                            int node_id = sqlite3_column_int(match_stmt, mc);
                                            char var_name[128];
                                            strncpy(var_name, cname, sizeof(var_name) - 1);
                                            var_name[sizeof(var_name) - 1] = '\0';
                                            char *suffix = strstr(var_name, "_id");
                                            if (suffix) {
                                                *suffix = '\0';
                                                set_variable_node_id(scoped_map, var_name, node_id);
                                                CYPHER_DEBUG("CALL MATCH: resolved '%s' = node %d",
                                                             var_name, node_id);
                                            }
                                        }
                                    }
                                    /* Execute all post-MATCH clauses for this row */
                                    for (int cj = ci + 1; cj < inner_query->clauses->count; cj++) {
                                        ast_node *post_clause = inner_query->clauses->items[cj];
                                        if (post_clause->type == AST_NODE_SET) {
                                            rc = execute_set_operations(executor, (cypher_set*)post_clause,
                                                                       scoped_map, result);
                                        } else if (post_clause->type == AST_NODE_MERGE) {
                                            rc = execute_merge_clause(executor, (cypher_merge*)post_clause, result, scoped_map, NULL);
                                        } else if (post_clause->type == AST_NODE_CREATE) {
                                            rc = execute_create_clause(executor, (cypher_create*)post_clause, result);
                                        }
                                        if (rc < 0) break;
                                    }
                                    if (rc < 0) break;
                                }
                                sqlite3_finalize(match_stmt);
                            }
                    }
                    /* All post-MATCH clauses handled inside the while loop */
                    break;
                } else if (inner_clause->type == AST_NODE_RETURN) {
                    /* RETURN in inner query — handled after the loop
                     * via post-CALL return path */
                    continue;
                } else {
                    CYPHER_DEBUG("CALL: skipping unsupported inner clause type %s",
                                ast_node_type_name(inner_clause->type));
                }
            }
        } else if (inner_ast->type == AST_NODE_UNION) {
            /* Inner query is a UNION — execute each branch with scoped bindings.
             * Walk the UNION tree: left may be another UNION or a query,
             * right is always a single query. */
            ast_node *union_branches[32]; /* max 32 branches */
            int branch_count = 0;

            /* Flatten UNION tree into branch array */
            ast_node *cur = inner_ast;
            while (cur && cur->type == AST_NODE_UNION && branch_count < 31) {
                cypher_union *u = (cypher_union*)cur;
                union_branches[branch_count++] = u->right;
                cur = u->left;
            }
            if (cur && branch_count < 32) {
                union_branches[branch_count++] = cur;
            }

            /* Execute branches in reverse order (left-to-right) */
            for (int bi = branch_count - 1; bi >= 0; bi--) {
                ast_node *branch = union_branches[bi];
                if (branch->type == AST_NODE_QUERY || branch->type == AST_NODE_SINGLE_QUERY) {
                    cypher_query *bq = (cypher_query*)branch;
                    /* Build scoped map for this branch's WITH */
                    variable_map *branch_scope = create_variable_map();
                    if (branch_scope && bq->clauses && bq->clauses->count > 0 &&
                        bq->clauses->items[0]->type == AST_NODE_WITH) {
                        cypher_with *with = (cypher_with*)bq->clauses->items[0];
                        if (with->items) {
                            for (int wi = 0; wi < with->items->count; wi++) {
                                cypher_return_item *item = (cypher_return_item*)with->items->items[wi];
                                if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
                                    const char *name = ((cypher_identifier*)item->expr)->name;
                                    int node_id = get_variable_node_id(var_map, name);
                                    if (node_id >= 0) {
                                        const char *alias = item->alias ? item->alias : name;
                                        set_variable_node_id(branch_scope, alias, node_id);
                                    }
                                }
                            }
                        }
                    }

                    /* Execute branch clauses (skip WITH) */
                    for (int ci = 0; ci < bq->clauses->count; ci++) {
                        ast_node *bc = bq->clauses->items[ci];
                        if (bc->type == AST_NODE_WITH) continue;
                        if (bc->type == AST_NODE_SET) {
                            execute_set_operations(executor, (cypher_set*)bc, branch_scope, result);
                        } else if (bc->type == AST_NODE_MERGE) {
                            execute_merge_clause(executor, (cypher_merge*)bc, result, NULL, NULL);
                        } else if (bc->type == AST_NODE_CREATE) {
                            execute_create_clause(executor, (cypher_create*)bc, result);
                        }
                    }
                    free_variable_map(branch_scope);
                }
            }
        } else {
            /* Unknown inner type — try direct dispatch */
            cypher_result *inner_result = cypher_executor_execute_ast(executor, inner_ast);
            if (inner_result) {
                result->nodes_created += inner_result->nodes_created;
                result->properties_set += inner_result->properties_set;
                total_inner_rows += inner_result->row_count;
                cypher_result_free(inner_result);
            }
        }

        /* Execute inner RETURN and accumulate results if needed */
        if (accumulating && inner_return_clause && inner_return_clause->items) {
            cypher_return *post_ret = NULL;
            for (int i = call_pos + 1; i < query->clauses->count; i++) {
                if (query->clauses->items[i]->type == AST_NODE_RETURN) {
                    post_ret = (cypher_return*)query->clauses->items[i];
                    break;
                }
            }

            if (post_ret && post_ret->items) {
                /* Evaluate inner RETURN items by building a single SQL query
                 * that resolves all expressions against the scoped variables. */
                char inner_sql[4096];
                size_t ipos = 0;
                ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos, "SELECT ");

                cypher_transform_context *ret_ctx = cypher_transform_create_context(executor->db);
                if (ret_ctx) {
                    /* Register scoped_map variables with unique aliases */
                    for (int si = 0; si < scoped_map->count; si++) {
                        variable_mapping *m = &scoped_map->mappings[si];
                        char var_alias[64];
                        snprintf(var_alias, sizeof(var_alias), "_cv_%d", si);
                        if (m->type == VAR_MAP_TYPE_NODE) {
                            transform_var_register_node(ret_ctx->var_ctx, m->variable, var_alias, NULL);
                        } else {
                            transform_var_register_edge(ret_ctx->var_ctx, m->variable, var_alias, NULL);
                        }
                        transform_var_set_bound(ret_ctx->var_ctx, m->variable, true);
                    }

                    /* Build SELECT expressions for inner RETURN items */
                    bool inner_ok = true;
                    for (int ri = 0; ri < inner_return_clause->items->count; ri++) {
                        cypher_return_item *ret_item = (cypher_return_item*)inner_return_clause->items->items[ri];
                        if (ri > 0) ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos, ", ");

                        char *saved_buf = ret_ctx->sql_buffer;
                        int saved_size = ret_ctx->sql_size;
                        int saved_cap = ret_ctx->sql_capacity;
                        char temp_buf[2048] = {0};
                        ret_ctx->sql_buffer = temp_buf;
                        ret_ctx->sql_size = 0;
                        ret_ctx->sql_capacity = sizeof(temp_buf);

                        if (transform_expression(ret_ctx, ret_item->expr) == 0 && ret_ctx->sql_size > 0) {
                            const char *col_name = ret_item->alias ? ret_item->alias : temp_buf;
                            ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos, "%s AS \"%s\"",
                                             temp_buf, col_name);
                        } else {
                            inner_ok = false;
                        }

                        ret_ctx->sql_buffer = saved_buf;
                        ret_ctx->sql_size = saved_size;
                        ret_ctx->sql_capacity = saved_cap;
                    }

                    /* Build FROM/WHERE to pin variables to exact entity IDs */
                    if (inner_ok && scoped_map->count > 0) {
                        for (int si = 0; si < scoped_map->count; si++) {
                            variable_mapping *m = &scoped_map->mappings[si];
                            char var_alias[64];
                            snprintf(var_alias, sizeof(var_alias), "_cv_%d", si);
                            const char *table = m->type == VAR_MAP_TYPE_NODE ? "nodes" : "edges";
                            if (si == 0) {
                                ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos,
                                                 " FROM %s AS %s WHERE %s.id = %d",
                                                 table, var_alias, var_alias, m->entity_id);
                            } else {
                                ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos,
                                                 " JOIN %s AS %s ON %s.id = %d",
                                                 table, var_alias, var_alias, m->entity_id);
                            }
                        }
                    }

                    cypher_transform_free_context(ret_ctx);

                    /* Store inner RETURN name→value pairs */
                    int inner_col_count = 0;
                    char **inner_col_names_local = NULL;
                    char **inner_col_values = NULL;

                    if (inner_ok) {
                        CYPHER_DEBUG("CALL inner RETURN SQL: %s", inner_sql);
                        sqlite3_stmt *inner_stmt;
                        if (sqlite3_prepare_v2(executor->db, inner_sql, -1, &inner_stmt, NULL) == SQLITE_OK) {
                            if (executor->params_json) {
                                bind_params_from_json(inner_stmt, executor->params_json);
                            }
                            if (sqlite3_step(inner_stmt) == SQLITE_ROW) {
                                inner_col_count = sqlite3_column_count(inner_stmt);
                                inner_col_values = calloc(inner_col_count, sizeof(char*));
                                inner_col_names_local = calloc(inner_col_count, sizeof(char*));
                                for (int ic = 0; ic < inner_col_count; ic++) {
                                    const char *cn = sqlite3_column_name(inner_stmt, ic);
                                    const char *cv = (const char*)sqlite3_column_text(inner_stmt, ic);
                                    inner_col_names_local[ic] = cn ? strdup(cn) : strdup("");
                                    inner_col_values[ic] = cv ? strdup(cv) : NULL;
                                }
                            }
                            sqlite3_finalize(inner_stmt);
                        }
                    }

                    /* Build combined row for post-CALL RETURN */
                    if (inner_col_values) {
                        int total_cols = post_ret->items->count;

                        if (accum_col_names == NULL) {
                            accum_col_count = total_cols;
                            accum_col_names = calloc(total_cols, sizeof(char*));
                            for (int pi = 0; pi < total_cols; pi++) {
                                cypher_return_item *item = (cypher_return_item*)post_ret->items->items[pi];
                                if (item->alias) {
                                    accum_col_names[pi] = strdup(item->alias);
                                } else if (item->expr->type == AST_NODE_IDENTIFIER) {
                                    accum_col_names[pi] = strdup(((cypher_identifier*)item->expr)->name);
                                } else if (item->expr->type == AST_NODE_PROPERTY) {
                                    cypher_property *p = (cypher_property*)item->expr;
                                    cypher_identifier *base = (cypher_identifier*)p->expr;
                                    char col_buf[256];
                                    snprintf(col_buf, sizeof(col_buf), "%s.%s", base->name, p->property_name);
                                    accum_col_names[pi] = strdup(col_buf);
                                } else {
                                    accum_col_names[pi] = strdup("?column?");
                                }
                            }
                        }

                        char **row = calloc(total_cols, sizeof(char*));
                        for (int pi = 0; pi < total_cols; pi++) {
                            cypher_return_item *item = (cypher_return_item*)post_ret->items->items[pi];
                            bool resolved = false;

                            /* Check inner RETURN columns first */
                            if (item->expr->type == AST_NODE_IDENTIFIER) {
                                const char *ref = ((cypher_identifier*)item->expr)->name;
                                for (int ic = 0; ic < inner_col_count; ic++) {
                                    if (inner_col_names_local[ic] && strcmp(inner_col_names_local[ic], ref) == 0) {
                                        row[pi] = inner_col_values[ic] ? strdup(inner_col_values[ic]) : NULL;
                                        resolved = true;
                                        break;
                                    }
                                }
                            }

                            /* Evaluate outer expressions via SQL */
                            if (!resolved) {
                                cypher_transform_context *eval_ctx = cypher_transform_create_context(executor->db);
                                if (eval_ctx) {
                                    for (int si = 0; si < var_map->count; si++) {
                                        variable_mapping *m = &var_map->mappings[si];
                                        char va[64];
                                        snprintf(va, sizeof(va), "_ov_%d", si);
                                        if (m->type == VAR_MAP_TYPE_NODE)
                                            transform_var_register_node(eval_ctx->var_ctx, m->variable, va, NULL);
                                        else
                                            transform_var_register_edge(eval_ctx->var_ctx, m->variable, va, NULL);
                                        transform_var_set_bound(eval_ctx->var_ctx, m->variable, true);
                                    }

                                    char eval_sql[2048];
                                    size_t epos = 0;
                                    epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos, "SELECT ");

                                    char *sb = eval_ctx->sql_buffer;
                                    int ss = eval_ctx->sql_size;
                                    int sc = eval_ctx->sql_capacity;
                                    char tb[1024] = {0};
                                    eval_ctx->sql_buffer = tb;
                                    eval_ctx->sql_size = 0;
                                    eval_ctx->sql_capacity = sizeof(tb);

                                    if (transform_expression(eval_ctx, item->expr) == 0 && eval_ctx->sql_size > 0) {
                                        epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos, "%s", tb);
                                        eval_ctx->sql_buffer = sb;
                                        eval_ctx->sql_size = ss;
                                        eval_ctx->sql_capacity = sc;

                                        for (int si = 0; si < var_map->count; si++) {
                                            variable_mapping *m = &var_map->mappings[si];
                                            char va[64];
                                            snprintf(va, sizeof(va), "_ov_%d", si);
                                            const char *tbl = m->type == VAR_MAP_TYPE_NODE ? "nodes" : "edges";
                                            if (si == 0)
                                                epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos,
                                                                 " FROM %s AS %s WHERE %s.id = %d", tbl, va, va, m->entity_id);
                                            else
                                                epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos,
                                                                 " JOIN %s AS %s ON %s.id = %d", tbl, va, va, m->entity_id);
                                        }

                                        sqlite3_stmt *ev;
                                        if (sqlite3_prepare_v2(executor->db, eval_sql, -1, &ev, NULL) == SQLITE_OK) {
                                            if (executor->params_json) {
                                                bind_params_from_json(ev, executor->params_json);
                                            }
                                            if (sqlite3_step(ev) == SQLITE_ROW) {
                                                const char *val = (const char*)sqlite3_column_text(ev, 0);
                                                row[pi] = val ? strdup(val) : NULL;
                                            }
                                            sqlite3_finalize(ev);
                                        }
                                    } else {
                                        eval_ctx->sql_buffer = sb;
                                        eval_ctx->sql_size = ss;
                                        eval_ctx->sql_capacity = sc;
                                    }
                                    cypher_transform_free_context(eval_ctx);
                                }
                            }
                        }

                        if (accum_count >= accum_capacity) {
                            accum_capacity *= 2;
                            accum_rows = realloc(accum_rows, accum_capacity * sizeof(char**));
                        }
                        accum_rows[accum_count++] = row;

                        for (int ic = 0; ic < inner_col_count; ic++) {
                            free(inner_col_names_local[ic]);
                            free(inner_col_values[ic]);
                        }
                        free(inner_col_names_local);
                        free(inner_col_values);
                    }
                }
            }
        }

        free_variable_map(scoped_map);
        free_variable_map(var_map);
    }

    sqlite3_finalize(outer_stmt);
    cypher_transform_free_context(ctx);

    /* T-0316: free the hoisted inner MATCH SQL cache. */
    if (inner_match_sql) {
        if (inner_query && inner_query->clauses) {
            for (int ci = 0; ci < inner_query->clauses->count; ci++) {
                free(inner_match_sql[ci]);
            }
        }
        free(inner_match_sql);
    }

    CYPHER_DEBUG("CALL subquery complete: %d inner rows processed", total_inner_rows);

    /* Handle post-CALL RETURN clause if present */
    cypher_return *post_return = NULL;
    for (int i = call_pos + 1; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_RETURN) {
            post_return = (cypher_return*)query->clauses->items[i];
            break;
        }
    }

    /* If we accumulated inner+outer results, use them directly */
    if (accumulating && accum_count > 0 && accum_col_names) {
        result->column_count = accum_col_count;
        result->column_names = accum_col_names;
        result->row_count = accum_count;
        result->data = accum_rows;
        result->success = true;
        return 0;
    }

    /* Clean up accumulator if unused */
    if (accum_rows) {
        for (int i = 0; i < accum_count; i++) {
            if (accum_rows[i]) {
                for (int j = 0; j < accum_col_count; j++) free(accum_rows[i][j]);
                free(accum_rows[i]);
            }
        }
        free(accum_rows);
    }
    if (accum_col_names) {
        for (int i = 0; i < accum_col_count; i++) free(accum_col_names[i]);
        free(accum_col_names);
    }

    if (post_return) {
        /* Post-CALL RETURN — re-execute outer query through the generic transform
         * pipeline which knows how to handle MATCH+RETURN. The CALL side effects
         * have already been applied above. Build a temporary query without CALL. */
        ast_list *outer_clauses = ast_list_create();
        for (int i = 0; i < call_pos; i++) {
            ast_list_append(outer_clauses, query->clauses->items[i]);
        }
        for (int i = call_pos + 1; i < query->clauses->count; i++) {
            ast_list_append(outer_clauses, query->clauses->items[i]);
        }

        /* Temporarily swap clause list */
        ast_list *original_clauses = query->clauses;
        query->clauses = outer_clauses;

        int ret_rc = handle_generic_transform(executor, query, result, flags);

        /* Restore original clause list and free temporary (don't free items — they're borrowed) */
        query->clauses = original_clauses;
        free(outer_clauses->items);
        free(outer_clauses);

        return ret_rc;
    }

    return 0;
}
