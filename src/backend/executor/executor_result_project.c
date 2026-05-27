/*
 * executor_result_project.c
 *    RETURN-projection and aggregation helpers — project rows from
 *    variable_map, synthesize delete-count returns, compute aggregate
 *    cells, set column names.
 *    Moved from query_dispatch.c (I-0040 M3).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "executor/query_patterns.h"
#include "executor/executor_internal.h"
#include "parser/cypher_debug.h"

/* Check if a RETURN clause contains only COUNT aggregates and synthesize
 * the result from delete counts instead of re-querying the empty graph. */
bool synthesize_delete_return(cypher_return *ret, cypher_result *result)
{
    if (!ret || !ret->items || ret->items->count == 0) return false;

    int total_deleted = result->nodes_deleted + result->relationships_deleted;
    if (total_deleted == 0) return false;

    /* Classify every RETURN item. We can synthesize when:
     *   - all items are COUNT(...) calls  → one-row aggregate
     *   - all items are literal constants → N rows of the constants,
     *     where N = total_deleted (the matched row count). Honors
     *     SKIP / LIMIT clamps on the RETURN. */
    bool all_count = true;
    bool all_literal = true;
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        if (!item || !item->expr) return false;
        if (item->expr->type == AST_NODE_FUNCTION_CALL) {
            cypher_function_call *func = (cypher_function_call*)item->expr;
            if (!func->function_name || strcasecmp(func->function_name, "count") != 0) {
                all_count = false;
            }
            all_literal = false;
        } else if (item->expr->type == AST_NODE_LITERAL) {
            all_count = false;
        } else {
            all_count = false;
            all_literal = false;
        }
    }
    if (!all_count && !all_literal) return false;

    int col_count = ret->items->count;
    result->column_count = col_count;
    result->column_names = malloc(col_count * sizeof(char*));

    int rows = all_count ? 1 : total_deleted;
    /* Apply SKIP / LIMIT for the literal-row case. */
    int64_t skip_val = 0, limit_val = -1;
    if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal *)ret->skip;
        if (l->literal_type == LITERAL_INTEGER) skip_val = l->value.integer;
    }
    if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal *)ret->limit;
        if (l->literal_type == LITERAL_INTEGER) limit_val = l->value.integer;
    }
    if (all_literal) {
        int start = (skip_val > 0 && skip_val < rows) ? (int)skip_val
                                                       : (skip_val >= rows ? rows : 0);
        int end = rows;
        if (limit_val == 0) end = start;
        else if (limit_val > 0 && start + (int)limit_val < end) end = start + (int)limit_val;
        rows = (end - start > 0) ? (end - start) : 0;
    }

    result->row_count = rows;
    if (rows > 0) {
        result->data = malloc(rows * sizeof(char**));
        result->data_types = malloc(rows * sizeof(int*));
    } else {
        result->data = NULL;
        result->data_types = NULL;
    }

    for (int r = 0; r < rows; r++) {
        result->data[r] = calloc(col_count, sizeof(char*));
        result->data_types[r] = calloc(col_count, sizeof(int));
    }

    for (int i = 0; i < col_count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        if (item->alias) {
            result->column_names[i] = strdup(item->alias);
        } else if (all_count) {
            result->column_names[i] = strdup("count");
        } else {
            result->column_names[i] = strdup("?column?");
        }

        if (all_count) {
            if (rows > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", total_deleted);
                result->data[0][i] = strdup(buf);
                result->data_types[0][i] = SQLITE_INTEGER;
            }
        } else {
            /* Render the literal value once, then repeat per row. */
            cypher_literal *lit = (cypher_literal *)item->expr;
            char buf[64] = "";
            int sqltype = SQLITE_NULL;
            switch (lit->literal_type) {
                case LITERAL_INTEGER:
                    snprintf(buf, sizeof(buf), "%lld", (long long)lit->value.integer);
                    sqltype = SQLITE_INTEGER;
                    break;
                case LITERAL_DECIMAL:
                    snprintf(buf, sizeof(buf), "%g", lit->value.decimal);
                    sqltype = SQLITE_FLOAT;
                    break;
                case LITERAL_STRING:
                    snprintf(buf, sizeof(buf), "%s", lit->value.string ? lit->value.string : "");
                    sqltype = SQLITE_TEXT;
                    break;
                case LITERAL_BOOLEAN:
                    snprintf(buf, sizeof(buf), "%s", lit->value.boolean ? "true" : "false");
                    sqltype = SQLITE_TEXT;
                    break;
                default: break;
            }
            for (int r = 0; r < rows; r++) {
                result->data[r][i] = (buf[0] != '\0' || sqltype == SQLITE_TEXT) ? strdup(buf) : NULL;
                result->data_types[r][i] = sqltype;
            }
        }
    }

    result->success = true;
    return true;
}

/* Return lowercased agg-function name if expr is an aggregating call, else NULL.
 * Caller does NOT own the string (static buffer per call). */
const char *aggregating_call_name(ast_node *expr) {
    static char buf[64];
    if (!expr || expr->type != AST_NODE_FUNCTION_CALL) return NULL;
    cypher_function_call *fc = (cypher_function_call*)expr;
    if (!fc->function_name) return NULL;
    size_t n = strlen(fc->function_name);
    if (n >= sizeof(buf)) return NULL;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)fc->function_name[i]);
    buf[n] = 0;
    if (!strcmp(buf, "count") || !strcmp(buf, "sum") || !strcmp(buf, "avg") ||
        !strcmp(buf, "min") || !strcmp(buf, "max") || !strcmp(buf, "collect") ||
        !strcmp(buf, "stdev") || !strcmp(buf, "stdevp") ||
        !strcmp(buf, "percentilecont") || !strcmp(buf, "percentiledisc"))
        return buf;
    return NULL;
}

/* True if any RETURN item is an aggregating call. */
bool return_has_aggregation(cypher_return *ret) {
    if (!ret || !ret->items) return false;
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
        if (aggregating_call_name(it->expr)) return true;
    }
    return false;
}

/* Fetch a node's property value as int64. Returns true on success. */
static bool fetch_node_prop_int(cypher_executor *executor, int node_id,
                                const char *prop_name, int64_t *out) {
    const char *type_tables[] = {"node_props_int", "node_props_real", "node_props_text", NULL};
    for (int t = 0; type_tables[t]; t++) {
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT value FROM %s WHERE node_id = %d AND key_id = "
            "(SELECT id FROM property_keys WHERE key = '%s')",
            type_tables[t], node_id, prop_name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            int rc = sqlite3_step(stmt);
            bool got = false;
            if (rc == SQLITE_ROW) {
                *out = sqlite3_column_int64(stmt, 0);
                got = true;
            }
            sqlite3_finalize(stmt);
            if (got) return true;
        }
    }
    return false;
}

static bool fetch_node_prop_double(cypher_executor *executor, int node_id,
                                   const char *prop_name, double *out) {
    const char *type_tables[] = {"node_props_real", "node_props_int", NULL};
    for (int t = 0; type_tables[t]; t++) {
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT value FROM %s WHERE node_id = %d AND key_id = "
            "(SELECT id FROM property_keys WHERE key = '%s')",
            type_tables[t], node_id, prop_name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            int rc = sqlite3_step(stmt);
            bool got = false;
            if (rc == SQLITE_ROW) {
                *out = sqlite3_column_double(stmt, 0);
                got = true;
            }
            sqlite3_finalize(stmt);
            if (got) return true;
        }
    }
    return false;
}

static bool fetch_edge_prop_int(cypher_executor *executor, int edge_id,
                                const char *prop_name, int64_t *out) {
    const char *type_tables[] = {"edge_props_int", "edge_props_real", "edge_props_text", NULL};
    for (int t = 0; type_tables[t]; t++) {
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT value FROM %s WHERE edge_id = %d AND key_id = "
            "(SELECT id FROM property_keys WHERE key = '%s')",
            type_tables[t], edge_id, prop_name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            int rc = sqlite3_step(stmt);
            bool got = false;
            if (rc == SQLITE_ROW) { *out = sqlite3_column_int64(stmt, 0); got = true; }
            sqlite3_finalize(stmt);
            if (got) return true;
        }
    }
    return false;
}

static bool fetch_edge_prop_double(cypher_executor *executor, int edge_id,
                                   const char *prop_name, double *out) {
    const char *type_tables[] = {"edge_props_real", "edge_props_int", NULL};
    for (int t = 0; type_tables[t]; t++) {
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT value FROM %s WHERE edge_id = %d AND key_id = "
            "(SELECT id FROM property_keys WHERE key = '%s')",
            type_tables[t], edge_id, prop_name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            int rc = sqlite3_step(stmt);
            bool got = false;
            if (rc == SQLITE_ROW) { *out = sqlite3_column_double(stmt, 0); got = true; }
            sqlite3_finalize(stmt);
            if (got) return true;
        }
    }
    return false;
}

/* Compute an aggregate over a list of var_maps for a single RETURN item.
 * Writes a result cell into result->data[0][col_idx]. */
void project_aggregate_cell(cypher_executor *executor,
                                   cypher_return_item *item,
                                   variable_map **maps, int n_maps,
                                   cypher_result *result, int col_idx)
{
    const char *agg = aggregating_call_name(item->expr);
    cypher_function_call *fc = (cypher_function_call*)item->expr;
    ast_node *arg = (fc->args && fc->args->count > 0) ? fc->args->items[0] : NULL;

    if (!strcmp(agg, "count")) {
        /* count(*) → n_maps; count(n) → n_maps where n bound; count(n.prop) → non-null count */
        int64_t cnt = 0;
        if (!arg) {
            cnt = n_maps;
        } else if (arg->type == AST_NODE_IDENTIFIER) {
            const char *var_name = ((cypher_identifier*)arg)->name;
            for (int m = 0; m < n_maps; m++) {
                if (get_variable_node_id(maps[m], var_name) >= 0 ||
                    get_variable_edge_id(maps[m], var_name) >= 0) cnt++;
            }
        } else if (arg->type == AST_NODE_PROPERTY) {
            cypher_property *prop = (cypher_property*)arg;
            const char *var_name = NULL;
            if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER)
                var_name = ((cypher_identifier*)prop->expr)->name;
            if (var_name) {
                for (int m = 0; m < n_maps; m++) {
                    int nid = get_variable_node_id(maps[m], var_name);
                    int eid = (nid < 0) ? get_variable_edge_id(maps[m], var_name) : -1;
                    int64_t tmp;
                    if (nid >= 0 && fetch_node_prop_int(executor, nid, prop->property_name, &tmp))
                        cnt++;
                    else if (eid >= 0 && fetch_edge_prop_int(executor, eid, prop->property_name, &tmp))
                        cnt++;
                }
            }
        } else {
            cnt = n_maps;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)cnt);
        result->data[0][col_idx] = strdup(buf);
        result->data_types[0][col_idx] = SQLITE_INTEGER;
        return;
    }

    /* For sum/avg/min/max we need a property access argument */
    if (!arg || arg->type != AST_NODE_PROPERTY) {
        result->data[0][col_idx] = NULL;
        return;
    }
    cypher_property *prop = (cypher_property*)arg;
    const char *var_name = NULL;
    if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER)
        var_name = ((cypher_identifier*)prop->expr)->name;
    if (!var_name) { result->data[0][col_idx] = NULL; return; }

    double dsum = 0, dmin = 0, dmax = 0;
    int seen = 0;
    bool all_int = true;
    int64_t isum = 0, imin = 0, imax = 0;
    for (int m = 0; m < n_maps; m++) {
        int nid = get_variable_node_id(maps[m], var_name);
        int eid = (nid < 0) ? get_variable_edge_id(maps[m], var_name) : -1;
        if (nid < 0 && eid < 0) continue;
        int64_t iv;
        double dv;
        bool got_int = (nid >= 0)
            ? fetch_node_prop_int(executor, nid, prop->property_name, &iv)
            : fetch_edge_prop_int(executor, eid, prop->property_name, &iv);
        if (got_int) {
            dv = (double)iv;
        } else {
            bool got_dbl = (nid >= 0)
                ? fetch_node_prop_double(executor, nid, prop->property_name, &dv)
                : fetch_edge_prop_double(executor, eid, prop->property_name, &dv);
            if (got_dbl) { all_int = false; iv = (int64_t)dv; }
            else continue;
        }
        if (seen == 0) {
            isum = iv; imin = iv; imax = iv;
            dsum = dv; dmin = dv; dmax = dv;
        } else {
            isum += iv;
            if (iv < imin) imin = iv;
            if (iv > imax) imax = iv;
            dsum += dv;
            if (dv < dmin) dmin = dv;
            if (dv > dmax) dmax = dv;
        }
        seen++;
    }

    char buf[64];
    if (!strcmp(agg, "sum")) {
        if (all_int) { snprintf(buf, sizeof(buf), "%lld", (long long)isum); result->data_types[0][col_idx] = SQLITE_INTEGER; }
        else { snprintf(buf, sizeof(buf), "%g", dsum); result->data_types[0][col_idx] = SQLITE_FLOAT; }
        result->data[0][col_idx] = strdup(buf);
    } else if (!strcmp(agg, "min")) {
        if (seen == 0) { result->data[0][col_idx] = NULL; return; }
        if (all_int) { snprintf(buf, sizeof(buf), "%lld", (long long)imin); result->data_types[0][col_idx] = SQLITE_INTEGER; }
        else { snprintf(buf, sizeof(buf), "%g", dmin); result->data_types[0][col_idx] = SQLITE_FLOAT; }
        result->data[0][col_idx] = strdup(buf);
    } else if (!strcmp(agg, "max")) {
        if (seen == 0) { result->data[0][col_idx] = NULL; return; }
        if (all_int) { snprintf(buf, sizeof(buf), "%lld", (long long)imax); result->data_types[0][col_idx] = SQLITE_INTEGER; }
        else { snprintf(buf, sizeof(buf), "%g", dmax); result->data_types[0][col_idx] = SQLITE_FLOAT; }
        result->data[0][col_idx] = strdup(buf);
    } else if (!strcmp(agg, "avg")) {
        if (seen == 0) { result->data[0][col_idx] = NULL; return; }
        snprintf(buf, sizeof(buf), "%g", dsum / seen);
        result->data_types[0][col_idx] = SQLITE_FLOAT;
        result->data[0][col_idx] = strdup(buf);
    } else {
        result->data[0][col_idx] = NULL;
    }
}

/* Project a single var_map into result->data[row_idx] using the RETURN items.
 * Caller must have allocated result->data[row_idx] and result->data_types[row_idx]
 * with col_count slots. */
void project_return_row_from_var_map(cypher_executor *executor,
                                            cypher_return *ret,
                                            variable_map *var_map,
                                            cypher_result *result,
                                            int row_idx)
{
    int col_count = ret->items->count;
    for (int i = 0; i < col_count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        ast_node *expr = item->expr;
        result->data[row_idx][i] = NULL;

        if (expr && expr->type == AST_NODE_PROPERTY) {
            cypher_property *prop = (cypher_property*)expr;
            const char *prop_name = prop->property_name;
            const char *var_name = NULL;
            if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
                var_name = ((cypher_identifier*)prop->expr)->name;
            }
            if (var_name && prop_name) {
                int node_id = get_variable_node_id(var_map, var_name);
                int edge_id = (node_id < 0) ? get_variable_edge_id(var_map, var_name) : -1;
                const char *id_col = node_id >= 0 ? "node_id" : "edge_id";
                int ent_id = node_id >= 0 ? node_id : edge_id;
                if (ent_id >= 0) {
                    const char *node_tables[] = {
                        "node_props_text", "node_props_int",
                        "node_props_real", "node_props_bool", "node_props_json", NULL
                    };
                    const char *edge_tables[] = {
                        "edge_props_text", "edge_props_int",
                        "edge_props_real", "edge_props_bool", "edge_props_json", NULL
                    };
                    const char **type_tables = node_id >= 0 ? node_tables : edge_tables;
                    for (int t = 0; type_tables[t]; t++) {
                        char sql[512];
                        snprintf(sql, sizeof(sql),
                            "SELECT value FROM %s WHERE %s = %d AND key_id = "
                            "(SELECT id FROM property_keys WHERE key = '%s')",
                            type_tables[t], id_col, ent_id, prop_name);
                        sqlite3_stmt *stmt;
                        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                            if (sqlite3_step(stmt) == SQLITE_ROW) {
                                int sql_type = sqlite3_column_type(stmt, 0);
                                const char *val = (const char*)sqlite3_column_text(stmt, 0);
                                if (val) {
                                    if (strstr(type_tables[t], "_props_bool")) {
                                        result->data[row_idx][i] = strdup(atoi(val) ? "true" : "false");
                                        result->data_types[row_idx][i] = SQLITE_TEXT;
                                    } else {
                                        result->data[row_idx][i] = strdup(val);
                                        result->data_types[row_idx][i] = sql_type;
                                    }
                                }
                            }
                            sqlite3_finalize(stmt);
                        }
                        if (result->data[row_idx][i]) break;
                    }
                }
            }
        } else if (expr && expr->type == AST_NODE_IDENTIFIER) {
            /* Project a bare node/edge variable as the full JSON form the
             * MATCH+RETURN path would have produced. Required so MATCH
             * (n) ... SET ... RETURN n returns (:Label {props}) and not
             * a bare id. */
            const char *var_name = ((cypher_identifier*)expr)->name;
            int node_id = get_variable_node_id(var_map, var_name);
            int edge_id = (node_id < 0) ? get_variable_edge_id(var_map, var_name) : -1;
            if (node_id >= 0) {
                char sql[2048];
                snprintf(sql, sizeof(sql),
                    "SELECT json_object('id', %d, "
                    "'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = %d), json('[]')), "
                    "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                    "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %d AND npt.key_id = pk.id), "
                    "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %d AND npi.key_id = pk.id), "
                    "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %d AND npr.key_id = pk.id), "
                    "(SELECT json(CASE WHEN npb.value THEN 'true' ELSE 'false' END) FROM node_props_bool npb WHERE npb.node_id = %d AND npb.key_id = pk.id), "
                    "(SELECT json(npj.value) FROM node_props_json npj WHERE npj.node_id = %d AND npj.key_id = pk.id))) "
                    "FROM property_keys pk WHERE EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM node_props_int WHERE node_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM node_props_json WHERE node_id = %d AND key_id = pk.id)), json('{}')))",
                    node_id, node_id, node_id, node_id, node_id, node_id, node_id,
                    node_id, node_id, node_id, node_id, node_id);
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *val = (const char *)sqlite3_column_text(stmt, 0);
                        if (val) {
                            result->data[row_idx][i] = strdup(val);
                            result->data_types[row_idx][i] = SQLITE_TEXT;
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            } else if (edge_id >= 0) {
                char sql[2048];
                snprintf(sql, sizeof(sql),
                    "SELECT json_object('id', %d, "
                    "'type', (SELECT type FROM edges WHERE id = %d), "
                    "'startNode', (SELECT source_id FROM edges WHERE id = %d), "
                    "'endNode', (SELECT target_id FROM edges WHERE id = %d), "
                    "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                    "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = %d AND ept.key_id = pk.id), "
                    "(SELECT epi.value FROM edge_props_int epi WHERE epi.edge_id = %d AND epi.key_id = pk.id), "
                    "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = %d AND epr.key_id = pk.id), "
                    "(SELECT json(CASE WHEN epb.value THEN 'true' ELSE 'false' END) FROM edge_props_bool epb WHERE epb.edge_id = %d AND epb.key_id = pk.id))) "
                    "FROM property_keys pk WHERE EXISTS (SELECT 1 FROM edge_props_text WHERE edge_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM edge_props_int WHERE edge_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM edge_props_real WHERE edge_id = %d AND key_id = pk.id) "
                    "OR EXISTS (SELECT 1 FROM edge_props_bool WHERE edge_id = %d AND key_id = pk.id)), json('{}')))",
                    edge_id, edge_id, edge_id, edge_id, edge_id, edge_id, edge_id, edge_id,
                    edge_id, edge_id, edge_id, edge_id);
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *val = (const char *)sqlite3_column_text(stmt, 0);
                        if (val) {
                            result->data[row_idx][i] = strdup(val);
                            result->data_types[row_idx][i] = SQLITE_TEXT;
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            }
        } else if (expr && expr->type == AST_NODE_FUNCTION_CALL) {
            /* Handle entity-introspection functions on var_map entries:
             * labels(n), id(n), type(r). Build a JSON-array string for
             * labels so the extension's JSON formatter passes it
             * through verbatim. */
            cypher_function_call *fc = (cypher_function_call*)expr;
            if (!fc->function_name || !fc->args || fc->args->count != 1) continue;
            ast_node *arg = fc->args->items[0];
            if (!arg || arg->type != AST_NODE_IDENTIFIER) continue;
            const char *var_name = ((cypher_identifier*)arg)->name;
            int node_id = get_variable_node_id(var_map, var_name);
            int edge_id = (node_id < 0) ? get_variable_edge_id(var_map, var_name) : -1;
            if (strcasecmp(fc->function_name, "labels") == 0 && node_id >= 0) {
                char sql[256];
                snprintf(sql, sizeof(sql),
                    "SELECT COALESCE(json_group_array(label), json('[]')) "
                    "FROM node_labels WHERE node_id = %d", node_id);
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *val = (const char*)sqlite3_column_text(stmt, 0);
                        if (val) {
                            result->data[row_idx][i] = strdup(val);
                            result->data_types[row_idx][i] = SQLITE_TEXT;
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            } else if (strcasecmp(fc->function_name, "id") == 0) {
                int id = (node_id >= 0) ? node_id : edge_id;
                if (id >= 0) {
                    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
                    result->data[row_idx][i] = strdup(buf);
                    result->data_types[row_idx][i] = SQLITE_INTEGER;
                }
            } else if (strcasecmp(fc->function_name, "type") == 0 && edge_id >= 0) {
                char sql[128];
                snprintf(sql, sizeof(sql),
                    "SELECT type FROM edges WHERE id = %d", edge_id);
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *val = (const char*)sqlite3_column_text(stmt, 0);
                        if (val) {
                            result->data[row_idx][i] = strdup(val);
                            result->data_types[row_idx][i] = SQLITE_TEXT;
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }
}

void set_return_column_names(cypher_return *ret, cypher_result *result) {
    int col_count = ret->items->count;
    result->column_count = col_count;
    result->column_names = malloc(col_count * sizeof(char*));
    for (int i = 0; i < col_count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        const char *alias = item->alias;
        ast_node *expr = item->expr;
        if (alias) {
            result->column_names[i] = strdup(alias);
        } else if (expr && expr->type == AST_NODE_PROPERTY) {
            cypher_property *prop = (cypher_property*)expr;
            const char *prop_name = prop->property_name;
            const char *var_name = NULL;
            if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
                var_name = ((cypher_identifier*)prop->expr)->name;
            }
            if (var_name && prop_name) {
                char col_name[256];
                snprintf(col_name, sizeof(col_name), "%s.%s", var_name, prop_name);
                result->column_names[i] = strdup(col_name);
            } else {
                result->column_names[i] = strdup("?column?");
            }
        } else if (expr && expr->type == AST_NODE_IDENTIFIER) {
            result->column_names[i] = strdup(((cypher_identifier*)expr)->name);
        } else {
            result->column_names[i] = strdup("?column?");
        }
    }
}

