/*
 * MERGE Clause Execution
 * Handles MERGE clause and MATCH+MERGE query execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"
#include "transform/transform_variables.h"

/* Parameter binding types for prepared statements */
typedef enum {
    BIND_TEXT,
    BIND_INT,
    BIND_DOUBLE
} bind_type;

typedef struct {
    bind_type type;
    union {
        const char *text;
        int64_t integer;     /* int truncated 64-bit ids/values */
        double real;
    } value;
} param_binding;

/* GQLITE-T-0402: a list or map literal of literals, serialized to JSON for a
 * MERGE property (match and create); NULL otherwise. Caller frees. */
static char *merge_json_value(ast_node *value)
{
    if (!value || (value->type != AST_NODE_LIST && value->type != AST_NODE_MAP)) return NULL;
    return serialize_ast_to_json(value);
}


#define MAX_BINDINGS 64

/* Set a node property from a $parameter value during the MERGE create
 * phase. Previously parameter-valued inline properties (e.g.
 * MERGE (n:L {id: $id})) were silently dropped at creation, leaving the
 * new node without the property. */
static void set_node_prop_from_param(cypher_executor *executor, int node_id,
                                     const char *key, cypher_parameter *param,
                                     cypher_result *result)
{
    if (!executor->params_json) return;
    property_type ptype;
    property_value pv;
    property_value_init(&pv);
    if (get_param_value(executor->params_json, param->name, &ptype, &pv) != 0) {
        property_value_free(&pv);
        return;
    }
    const void *prop_value = NULL;
    int64_t ibuf;
    double rbuf;
    int bbuf;
    switch (ptype) {
        case PROP_TYPE_TEXT:
        case PROP_TYPE_JSON:
            prop_value = pv.as_str;
            break;
        case PROP_TYPE_INTEGER:
            ibuf = pv.as_int;
            prop_value = &ibuf;
            break;
        case PROP_TYPE_REAL:
            rbuf = pv.as_real;
            prop_value = &rbuf;
            break;
        case PROP_TYPE_BOOLEAN:
            bbuf = pv.as_bool;
            prop_value = &bbuf;
            break;
        default:
            break;
    }
    if (prop_value &&
        cypher_schema_set_node_property(executor->schema_mgr, node_id, key, ptype, prop_value) == 0) {
        result->properties_set++;
    }
    property_value_free(&pv);
}

/* Helper to bind all parameters to a prepared statement */
static int bind_all_params(sqlite3_stmt *stmt, param_binding *bindings, int count)
{
    for (int i = 0; i < count; i++) {
        int rc;
        switch (bindings[i].type) {
            case BIND_TEXT:
                rc = sqlite3_bind_text(stmt, i + 1, bindings[i].value.text, -1, SQLITE_STATIC);
                break;
            case BIND_INT:
                rc = sqlite3_bind_int64(stmt, i + 1, bindings[i].value.integer);
                break;
            case BIND_DOUBLE:
                rc = sqlite3_bind_double(stmt, i + 1, bindings[i].value.real);
                break;
            default:
                return -1;
        }
        if (rc != SQLITE_OK) return -1;
    }
    return 0;
}

/* GQLITE-T-0517: does a MERGE property value need evaluating (an
 * expression, not a literal, a parameter or a list / map of literals)? */
static bool merge_value_needs_eval(ast_node *v)
{
    if (!v || v->type == AST_NODE_LITERAL || v->type == AST_NODE_PARAMETER) return false;
    if (v->type == AST_NODE_LIST || v->type == AST_NODE_MAP) {
        char *js = merge_json_value(v);
        free(js);
        return js == NULL;
    }
    return true;
}

/* GQLITE-T-0517: the value of a MERGE property expression, with the values
 * of an enclosing FOREACH / UNWIND in scope (`MERGE (b {x: x + 1})`; it was
 * ignored, so the MERGE matched any node and set nothing). 0 = value,
 * -2 = null, -1 = cannot evaluate. */
static int merge_eval_value(cypher_executor *executor, ast_node *v,
                            property_type *t, property_value *pv)
{
    if (g_foreach_ctx && (v->type == AST_NODE_PROPERTY || v->type == AST_NODE_IDENTIFIER) &&
        foreach_value(executor->db, g_foreach_ctx, v, t, pv) == 0)
        return 0;
    variable_map *vm = create_variable_map();
    if (!vm) return -1;
    int rc = executor_eval_value(executor, v, vm, t, pv);
    free_variable_map(vm);
    return rc;
}

/* GQLITE-T-0515: set the inline properties of a node a MERGE created. */
static void set_merge_node_props(cypher_executor *executor, int node_id,
                                 cypher_node_pattern *np, cypher_result *result)
{
    if (!np->properties || np->properties->type != AST_NODE_MAP) return;
    cypher_map *map = (cypher_map*)np->properties;
    for (int j = 0; map->pairs && j < map->pairs->count; j++) {
        cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
        if (!pair->key || !pair->value) continue;
        if ((pair->value->type == AST_NODE_PROPERTY || pair->value->type == AST_NODE_IDENTIFIER) &&
            g_foreach_ctx) {
            property_type t = PROP_TYPE_TEXT;
            property_value v;
            property_value_init(&v);
            if (foreach_value(executor->db, g_foreach_ctx, pair->value, &t, &v) == 0 &&
                cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, t,
                                                property_value_ptr(t, &v)) == 0)
                result->properties_set++;
            property_value_free(&v);
        } else if (pair->value->type == AST_NODE_PARAMETER) {
            set_node_prop_from_param(executor, node_id, pair->key, (cypher_parameter*)pair->value, result);
        } else if (pair->value->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)pair->value;
            int b = lit->value.boolean;
            property_type t;
            const void *v;
            switch (lit->literal_type) {
                case LITERAL_STRING:  t = PROP_TYPE_TEXT;    v = lit->value.string;   break;
                case LITERAL_INTEGER: t = PROP_TYPE_INTEGER; v = &lit->value.integer; break;
                case LITERAL_DECIMAL: t = PROP_TYPE_REAL;    v = &lit->value.decimal; break;
                case LITERAL_BOOLEAN: t = PROP_TYPE_BOOLEAN; v = &b;                  break;
                default: continue;
            }
            if (cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, t, v) == 0)
                result->properties_set++;
        } else if (merge_value_needs_eval(pair->value)) {
            property_type t = PROP_TYPE_TEXT;
            property_value v;
            property_value_init(&v);
            if (merge_eval_value(executor, pair->value, &t, &v) == 0 &&
                cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, t,
                                                property_value_ptr(t, &v)) == 0)
                result->properties_set++;
            property_value_free(&v);
        } else {
            char *js = merge_json_value(pair->value);
            if (js && cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key,
                                                      PROP_TYPE_JSON, js) == 0)
                result->properties_set++;
            free(js);
        }
    }
}

/* GQLITE-T-0515: set the inline properties of a relationship a MERGE created. */
static void set_merge_edge_props(cypher_executor *executor, int edge_id,
                                 cypher_rel_pattern *rp, cypher_result *result)
{
    if (!rp->properties || rp->properties->type != AST_NODE_MAP) return;
    cypher_map *map = (cypher_map*)rp->properties;
    for (int j = 0; map->pairs && j < map->pairs->count; j++) {
        cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
        if (!pair->key || !pair->value) continue;
        property_type t = PROP_TYPE_TEXT;
        property_value v;
        property_value_init(&v);
        const void *ptr = NULL;
        char *js = NULL;
        int b = 0;
        if (pair->value->type == AST_NODE_PARAMETER) {
            if (executor->params_json &&
                get_param_value(executor->params_json, ((cypher_parameter*)pair->value)->name, &t, &v) == 0)
                ptr = property_value_ptr(t, &v);
        } else if (pair->value->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)pair->value;
            switch (lit->literal_type) {
                case LITERAL_STRING:  t = PROP_TYPE_TEXT;    ptr = lit->value.string;   break;
                case LITERAL_INTEGER: t = PROP_TYPE_INTEGER; ptr = &lit->value.integer; break;
                case LITERAL_DECIMAL: t = PROP_TYPE_REAL;    ptr = &lit->value.decimal; break;
                case LITERAL_BOOLEAN: t = PROP_TYPE_BOOLEAN; b = lit->value.boolean; ptr = &b; break;
                default: break;
            }
        } else if ((js = merge_json_value(pair->value)) != NULL) {
            t = PROP_TYPE_JSON;
            ptr = js;
        } else if (merge_value_needs_eval(pair->value) &&
                   merge_eval_value(executor, pair->value, &t, &v) == 0) {
            ptr = property_value_ptr(t, &v);
        }
        if (ptr && cypher_schema_set_edge_property(executor->schema_mgr, edge_id, pair->key, t, ptr) == 0)
            result->properties_set++;
        property_value_free(&v);
        free(js);
    }
}

/* GQLITE-T-0515: the label and property filters of a MERGE node pattern, as
 * JOINs on the alias `n` ("SELECT n.id FROM nodes n" + these). Appends its
 * bindings at *bind_count (at most cap) and the strings they point to at
 * *owned_count; the caller frees those after sqlite3_step. */
static void append_node_filters(cypher_executor *executor, cypher_node_pattern *node_pattern,
                                sqlite3_str *sqls, param_binding *bindings, int *bind_count_p,
                                int cap, char **owned_strs, int *owned_count_p)
{
    int bind_count = *bind_count_p;
    int owned_count = *owned_count_p;


    /* Add label joins if specified - one join per label */
    if (has_labels(node_pattern)) {
        for (int li = 0; li < node_pattern->labels->count; li++) {
            const char *label = get_label_string(node_pattern->labels->items[li]);
            if (label && bind_count < cap) {
                sqlite3_str_appendf(sqls,
                                  " JOIN node_labels nl%d ON n.id = nl%d.node_id AND nl%d.label = ?",
                                  li, li, li);
                bindings[bind_count].type = BIND_TEXT;
                bindings[bind_count].value.text = label;
                bind_count++;
            }
        }
    }

    /* Add property joins if specified */
    if (node_pattern->properties && node_pattern->properties->type == AST_NODE_MAP) {
        cypher_map *map = (cypher_map*)node_pattern->properties;
        if (map->pairs) {
            for (int i = 0; i < map->pairs->count; i++) {
                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                if (pair->key && pair->value &&
                    (merge_value_needs_eval(pair->value) ||
                     (pair->value->type == AST_NODE_PARAMETER && executor->params_json))) {
                    /* Parameter-valued inline property, e.g.
                     * MERGE (n:L {id: $id}). Previously skipped, so the
                     * match phase ignored the filter entirely (same family
                     * as GitHub #96) and MERGE could match an unrelated
                     * node of the label. Resolve the parameter and add the
                     * same join a literal value would get. A FOREACH/UNWIND
                     * value (`item`, `row.id`) takes the same typed join
                     * (#124: it was always matched and stored as text). */
                    property_type ptype;
                    property_value pv;
                    property_value_init(&pv);
                    int vrc = (pair->value->type == AST_NODE_PARAMETER)
                        ? get_param_value(executor->params_json,
                                          ((cypher_parameter*)pair->value)->name, &ptype, &pv)
                        : merge_eval_value(executor, pair->value, &ptype, &pv);
                    if (vrc != 0) {
                        property_value_free(&pv);
                        continue;
                    }
                    if (bind_count + 2 > cap || owned_count >= cap) {
                        property_value_free(&pv);
                        break;
                    }
                    const char *prop_table = NULL;
                    param_binding val_bind;
                    switch (ptype) {
                        case PROP_TYPE_TEXT:
                            prop_table = "node_props_text";
                            val_bind.type = BIND_TEXT;
                            val_bind.value.text = pv.as_str;
                            owned_strs[owned_count++] = pv.as_str;
                            pv.as_str = NULL; /* ownership moved */
                            break;
                        case PROP_TYPE_INTEGER:
                            prop_table = "node_props_int";
                            val_bind.type = BIND_INT;
                            val_bind.value.integer = pv.as_int;
                            break;
                        case PROP_TYPE_REAL:
                            prop_table = "node_props_real";
                            val_bind.type = BIND_DOUBLE;
                            val_bind.value.real = pv.as_real;
                            break;
                        case PROP_TYPE_BOOLEAN:
                            prop_table = "node_props_bool";
                            val_bind.type = BIND_INT;
                            val_bind.value.integer = pv.as_bool ? 1 : 0;
                            break;
                        case PROP_TYPE_JSON:
                            prop_table = "node_props_json";
                            val_bind.type = BIND_TEXT;
                            val_bind.value.text = pv.as_str;
                            owned_strs[owned_count++] = pv.as_str;
                            pv.as_str = NULL; /* ownership moved */
                            break;
                        default:
                            break;
                    }
                    property_value_free(&pv);
                    if (!prop_table) continue;
                    sqlite3_str_appendf(sqls,
                                      " JOIN %s np%d ON n.id = np%d.node_id"
                                      " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = ?"
                                      " AND np%d.value = ?",
                                      prop_table, i, i, i, i, i, i, i);
                    bindings[bind_count].type = BIND_TEXT;
                    bindings[bind_count].value.text = pair->key;
                    bind_count++;
                    bindings[bind_count] = val_bind;
                    bind_count++;
                } else if (pair->key && (pair->value->type == AST_NODE_LIST ||
                                         pair->value->type == AST_NODE_MAP)) {
                    /* GQLITE-T-0402: a list or map matches by JSON value. */
                    char *js = merge_json_value(pair->value);
                    if (!js) continue;
                    if (bind_count + 2 > cap || owned_count >= cap) { free(js); break; }
                    owned_strs[owned_count++] = js;
                    sqlite3_str_appendf(sqls,
                                      " JOIN node_props_json np%d ON n.id = np%d.node_id"
                                      " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = ?"
                                      " AND json(np%d.value) = json(?)",
                                      i, i, i, i, i, i, i);
                    bindings[bind_count].type = BIND_TEXT;
                    bindings[bind_count].value.text = pair->key;
                    bind_count++;
                    bindings[bind_count].type = BIND_TEXT;
                    bindings[bind_count].value.text = js;
                    bind_count++;
                } else if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)pair->value;

                    if (bind_count + 2 > cap) break;

                    /* Determine which property table to join */
                    const char *prop_table = "node_props_text";

                    switch (lit->literal_type) {
                        case LITERAL_STRING:
                            prop_table = "node_props_text";
                            sqlite3_str_appendf(sqls,
                                              " JOIN %s np%d ON n.id = np%d.node_id"
                                              " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = ?"
                                              " AND np%d.value = ?",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = lit->value.string;
                            bind_count++;
                            break;
                        case LITERAL_INTEGER:
                            prop_table = "node_props_int";
                            sqlite3_str_appendf(sqls,
                                              " JOIN %s np%d ON n.id = np%d.node_id"
                                              " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = ?"
                                              " AND np%d.value = ?",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_INT;
                            bindings[bind_count].value.integer = lit->value.integer;
                            bind_count++;
                            break;
                        case LITERAL_DECIMAL:
                            prop_table = "node_props_real";
                            sqlite3_str_appendf(sqls,
                                              " JOIN %s np%d ON n.id = np%d.node_id"
                                              " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = ?"
                                              " AND np%d.value = ?",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_DOUBLE;
                            bindings[bind_count].value.real = lit->value.decimal;
                            bind_count++;
                            break;
                        case LITERAL_BOOLEAN:
                            prop_table = "node_props_bool";
                            sqlite3_str_appendf(sqls,
                                              " JOIN %s np%d ON n.id = np%d.node_id"
                                              " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = ?"
                                              " AND np%d.value = ?",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_INT;
                            bindings[bind_count].value.integer = lit->value.boolean ? 1 : 0;
                            bind_count++;
                            break;
                        default:
                            continue;
                    }
                }
            }
        }
    }

    *bind_count_p = bind_count;
    *owned_count_p = owned_count;
}

/* Find a node by label and properties, returns node_id or -1 if not found */
int find_node_by_pattern(cypher_executor *executor, cypher_node_pattern *node_pattern)
{
    if (!executor || !node_pattern) {
        return -1;
    }

    /* Build SQL query with parameterized values */
    sqlite3_str *sqls = sqlite3_str_new(executor->db);
    param_binding bindings[MAX_BINDINGS];
    int bind_count = 0;
    /* Strings resolved from $parameters; must stay alive until after
     * sqlite3_step (BIND_TEXT uses SQLITE_STATIC). */
    char *owned_strs[MAX_BINDINGS];
    int owned_count = 0;

    sqlite3_str_appendf(sqls,
                       "SELECT n.id FROM nodes n");
    append_node_filters(executor, node_pattern, sqls, bindings, &bind_count,
                        MAX_BINDINGS, owned_strs, &owned_count);

    sqlite3_str_appendf(sqls, " LIMIT 1");

    char *sql = sqlite3_str_finish(sqls);
    if (!sql) return -1;
    CYPHER_DEBUG("MERGE find query: %s (with %d bound params)", sql, bind_count);

    /* Execute the query with bound parameters */
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("MERGE find query prepare failed: %s", sqlite3_errmsg(executor->db));
        for (int k = 0; k < owned_count; k++) free(owned_strs[k]);
        return -1;
    }

    if (bind_all_params(stmt, bindings, bind_count) != 0) {
        CYPHER_DEBUG("MERGE find query bind failed");
        sqlite3_finalize(stmt);
        for (int k = 0; k < owned_count; k++) free(owned_strs[k]);
        return -1;
    }

    int node_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        node_id = sqlite3_column_int(stmt, 0);
        CYPHER_DEBUG("Found existing node %d", node_id);
    }

    sqlite3_finalize(stmt);
    for (int k = 0; k < owned_count; k++) free(owned_strs[k]);
    return node_id;
}

/* GQLITE-T-0515: the property filters of a MERGE relationship pattern, as
 * "AND EXISTS (...)" terms on the alias `e`. Same contract as
 * append_node_filters. */
static void append_edge_filters(cypher_executor *executor, cypher_rel_pattern *rel_pattern,
                                sqlite3_str *sqls, param_binding *bindings, int *bind_count_p,
                                int cap, char **owned_strs, int *owned_count_p)
{
    int bind_count = *bind_count_p;
    int owned_count = *owned_count_p;
    /* Add property joins if specified */
    if (rel_pattern && rel_pattern->properties && rel_pattern->properties->type == AST_NODE_MAP) {
        cypher_map *map = (cypher_map*)rel_pattern->properties;
        if (map->pairs) {
            for (int i = 0; i < map->pairs->count; i++) {
                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                if (pair->key && pair->value &&
                    ((pair->value->type == AST_NODE_PARAMETER && executor->params_json) ||
                     merge_value_needs_eval(pair->value))) {
                    /* Parameter-valued inline property, e.g.
                     * MERGE (a)-[r:T {id: $eid}]->(b). Previously skipped,
                     * so MERGE matched any existing edge on the triple and
                     * never created a second one (same family as GitHub
                     * #96; needed for #97-style caller-assigned edge ids). */
                    property_type ptype;
                    property_value pv;
                    property_value_init(&pv);
                    int vrc = (pair->value->type == AST_NODE_PARAMETER)
                        ? get_param_value(executor->params_json,
                                          ((cypher_parameter*)pair->value)->name, &ptype, &pv)
                        : merge_eval_value(executor, pair->value, &ptype, &pv);
                    if (vrc != 0) {
                        property_value_free(&pv);
                        continue;
                    }
                    if (bind_count + 2 > cap || owned_count >= cap) {
                        property_value_free(&pv);
                        break;
                    }
                    const char *prop_table = NULL;
                    param_binding val_bind;
                    switch (ptype) {
                        case PROP_TYPE_TEXT:
                            prop_table = "edge_props_text";
                            val_bind.type = BIND_TEXT;
                            val_bind.value.text = pv.as_str;
                            owned_strs[owned_count++] = pv.as_str;
                            pv.as_str = NULL; /* ownership moved */
                            break;
                        case PROP_TYPE_INTEGER:
                            prop_table = "edge_props_int";
                            val_bind.type = BIND_INT;
                            val_bind.value.integer = pv.as_int;
                            break;
                        case PROP_TYPE_REAL:
                            prop_table = "edge_props_real";
                            val_bind.type = BIND_DOUBLE;
                            val_bind.value.real = pv.as_real;
                            break;
                        case PROP_TYPE_BOOLEAN:
                            prop_table = "edge_props_bool";
                            val_bind.type = BIND_INT;
                            val_bind.value.integer = pv.as_bool ? 1 : 0;
                            break;
                        default:
                            break;
                    }
                    property_value_free(&pv);
                    if (!prop_table) continue;
                    sqlite3_str_appendf(sqls,
                                      " AND EXISTS (SELECT 1 FROM %s ep%d"
                                      " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                      " WHERE ep%d.edge_id = e.id AND pk%d.key = ? AND ep%d.value = ?)",
                                      prop_table, i, i, i, i, i, i, i);
                    bindings[bind_count].type = BIND_TEXT;
                    bindings[bind_count].value.text = pair->key;
                    bind_count++;
                    bindings[bind_count] = val_bind;
                    bind_count++;
                } else if (pair->key && (pair->value->type == AST_NODE_LIST ||
                                         pair->value->type == AST_NODE_MAP)) {
                    /* GQLITE-T-0402: a list or map matches by JSON value. */
                    char *js = merge_json_value(pair->value);
                    if (!js) continue;
                    if (bind_count + 2 > cap || owned_count >= cap) { free(js); break; }
                    owned_strs[owned_count++] = js;
                    sqlite3_str_appendf(sqls,
                                      " AND EXISTS (SELECT 1 FROM edge_props_json ep%d"
                                      " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                      " WHERE ep%d.edge_id = e.id AND pk%d.key = ? AND json(ep%d.value) = json(?))",
                                      i, i, i, i, i, i, i);
                    bindings[bind_count].type = BIND_TEXT;
                    bindings[bind_count].value.text = pair->key;
                    bind_count++;
                    bindings[bind_count].type = BIND_TEXT;
                    bindings[bind_count].value.text = js;
                    bind_count++;
                } else if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)pair->value;

                    if (bind_count + 2 > cap) break;

                    /* Determine which property table to join */
                    const char *prop_table = "edge_props_text";

                    switch (lit->literal_type) {
                        case LITERAL_STRING:
                            prop_table = "edge_props_text";
                            sqlite3_str_appendf(sqls,
                                              " AND EXISTS (SELECT 1 FROM %s ep%d"
                                              " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                              " WHERE ep%d.edge_id = e.id AND pk%d.key = ? AND ep%d.value = ?)",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = lit->value.string;
                            bind_count++;
                            break;
                        case LITERAL_INTEGER:
                            prop_table = "edge_props_int";
                            sqlite3_str_appendf(sqls,
                                              " AND EXISTS (SELECT 1 FROM %s ep%d"
                                              " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                              " WHERE ep%d.edge_id = e.id AND pk%d.key = ? AND ep%d.value = ?)",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_INT;
                            bindings[bind_count].value.integer = lit->value.integer;
                            bind_count++;
                            break;
                        case LITERAL_DECIMAL:
                            prop_table = "edge_props_real";
                            sqlite3_str_appendf(sqls,
                                              " AND EXISTS (SELECT 1 FROM %s ep%d"
                                              " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                              " WHERE ep%d.edge_id = e.id AND pk%d.key = ? AND ep%d.value = ?)",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_DOUBLE;
                            bindings[bind_count].value.real = lit->value.decimal;
                            bind_count++;
                            break;
                        case LITERAL_BOOLEAN:
                            prop_table = "edge_props_bool";
                            sqlite3_str_appendf(sqls,
                                              " AND EXISTS (SELECT 1 FROM %s ep%d"
                                              " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                              " WHERE ep%d.edge_id = e.id AND pk%d.key = ? AND ep%d.value = ?)",
                                              prop_table, i, i, i, i, i, i, i);
                            bindings[bind_count].type = BIND_TEXT;
                            bindings[bind_count].value.text = pair->key;
                            bind_count++;
                            bindings[bind_count].type = BIND_INT;
                            bindings[bind_count].value.integer = lit->value.boolean ? 1 : 0;
                            bind_count++;
                            break;
                        default:
                            continue;
                    }
                }
            }
        }
    }

    *bind_count_p = bind_count;
    *owned_count_p = owned_count;
}

#define MERGE_PATH_MAX 32

/* GQLITE-T-0515: MERGE of a path with relationships. openCypher matches the
 * whole pattern, with the variables bound before the MERGE held fixed; if no
 * match exists it creates every relationship and every node that is not
 * bound. Used to match node by node, so `MATCH (a) MERGE (a)-[:T]->()` took
 * any existing node as the anonymous end and made a self-loop. Binds the
 * pattern's variables in var_map and runs ON CREATE or ON MATCH once. */
static int merge_rel_path(cypher_executor *executor, cypher_merge *merge, cypher_path *path,
                          variable_map *var_map, cypher_result *result)
{
    int n_el = path->elements->count;
    if (n_el % 2 == 0 || n_el / 2 + 1 > MERGE_PATH_MAX) {
        set_result_error(result, "Invalid relationship pattern in MERGE");
        return -1;
    }
    int k = n_el / 2 + 1;                       /* nodes; k - 1 relationships */
    cypher_node_pattern *np[MERGE_PATH_MAX];
    cypher_rel_pattern *rp[MERGE_PATH_MAX];
    int bound[MERGE_PATH_MAX], same_as[MERGE_PATH_MAX], ids[MERGE_PATH_MAX], eids[MERGE_PATH_MAX];
    for (int i = 0; i < n_el; i++) {
        ast_node *el = path->elements->items[i];
        int want = (i % 2 == 0) ? AST_NODE_NODE_PATTERN : AST_NODE_REL_PATTERN;
        if ((int)el->type != want) {
            set_result_error(result, "Invalid relationship pattern in MERGE");
            return -1;
        }
        if (i % 2 == 0) np[i / 2] = (cypher_node_pattern*)el;
        else rp[i / 2] = (cypher_rel_pattern*)el;
    }
    for (int i = 0; i < k; i++) {
        bound[i] = np[i]->variable ? get_variable_node_id(var_map, np[i]->variable) : -1;
        same_as[i] = -1;
        if (bound[i] < 0 && np[i]->variable)
            for (int j = 0; j < i; j++)
                if (np[j]->variable && strcmp(np[j]->variable, np[i]->variable) == 0) {
                    same_as[i] = j;
                    break;
                }
    }

    /* Match: one join over the whole pattern. */
    enum { CAP = 4 * MAX_BINDINGS };
    param_binding bindings[CAP];
    char *owned[CAP];
    int bind_count = 0, owned_count = 0;
    sqlite3_str *q = sqlite3_str_new(executor->db);
    sqlite3_str_appendall(q, "SELECT ");
    for (int i = 0; i < k; i++) sqlite3_str_appendf(q, "%sn%d.id", i ? ", " : "", i);
    for (int r = 0; r < k - 1; r++) sqlite3_str_appendf(q, ", e%d.id", r);
    sqlite3_str_appendall(q, " FROM ");
    for (int i = 0; i < k; i++) sqlite3_str_appendf(q, "%snodes n%d", i ? ", " : "", i);
    for (int r = 0; r < k - 1; r++) sqlite3_str_appendf(q, ", edges e%d", r);
    sqlite3_str_appendall(q, " WHERE 1");
    for (int i = 0; i < k; i++) {
        if (bound[i] >= 0) {
            sqlite3_str_appendf(q, " AND n%d.id = %d", i, bound[i]);
        } else if (same_as[i] >= 0) {
            sqlite3_str_appendf(q, " AND n%d.id = n%d.id", i, same_as[i]);
        } else if (has_labels(np[i]) || np[i]->properties) {
            sqlite3_str_appendf(q, " AND n%d.id IN (SELECT n.id FROM nodes n", i);
            append_node_filters(executor, np[i], q, bindings, &bind_count, CAP, owned, &owned_count);
            sqlite3_str_appendall(q, ")");
        }
    }
    for (int r = 0; r < k - 1; r++) {
        int a = r, b = r + 1;
        if (rp[r]->left_arrow && !rp[r]->right_arrow) { a = r + 1; b = r; }
        if (!rp[r]->left_arrow && !rp[r]->right_arrow)
            sqlite3_str_appendf(q, " AND ((e%d.source_id = n%d.id AND e%d.target_id = n%d.id)"
                                   " OR (e%d.source_id = n%d.id AND e%d.target_id = n%d.id))",
                                r, a, r, b, r, b, r, a);
        else
            sqlite3_str_appendf(q, " AND e%d.source_id = n%d.id AND e%d.target_id = n%d.id", r, a, r, b);
        if (rp[r]->type && bind_count < CAP) {
            sqlite3_str_appendf(q, " AND e%d.type = ?", r);
            bindings[bind_count].type = BIND_TEXT;
            bindings[bind_count].value.text = rp[r]->type;
            bind_count++;
        }
        if (rp[r]->properties) {
            sqlite3_str_appendf(q, " AND e%d.id IN (SELECT e.id FROM edges e WHERE 1", r);
            append_edge_filters(executor, rp[r], q, bindings, &bind_count, CAP, owned, &owned_count);
            sqlite3_str_appendall(q, ")");
        }
        for (int s = 0; s < r; s++) sqlite3_str_appendf(q, " AND e%d.id <> e%d.id", r, s);
    }
    sqlite3_str_appendall(q, " LIMIT 1");
    char *sql = sqlite3_str_finish(q);
    if (!sql) {
        for (int i = 0; i < owned_count; i++) free(owned[i]);
        set_result_error(result, "Out of memory in MERGE");
        return -1;
    }
    CYPHER_DEBUG("MERGE path match: %s", sql);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);
    bool matched = false;
    if (rc == SQLITE_OK && bind_all_params(stmt, bindings, bind_count) == 0 &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        matched = true;
        for (int i = 0; i < k; i++) ids[i] = sqlite3_column_int(stmt, i);
        for (int r = 0; r < k - 1; r++) eids[r] = sqlite3_column_int(stmt, k + r);
    }
    char prep_err[256] = "";
    if (rc != SQLITE_OK)
        snprintf(prep_err, sizeof(prep_err), "MERGE match failed: %s", sqlite3_errmsg(executor->db));
    sqlite3_finalize(stmt);
    for (int i = 0; i < owned_count; i++) free(owned[i]);
    if (prep_err[0]) {
        set_result_error(result, prep_err);
        return -1;
    }

    if (!matched) {
        /* Create: each node that is not bound, then each relationship. */
        for (int i = 0; i < k; i++) {
            if (bound[i] >= 0) { ids[i] = bound[i]; continue; }
            if (same_as[i] >= 0) { ids[i] = ids[same_as[i]]; continue; }
            ids[i] = cypher_schema_create_node(executor->schema_mgr);
            if (ids[i] < 0) {
                set_result_error(result, "Failed to create node in MERGE");
                return -1;
            }
            result->nodes_created++;
            for (int li = 0; has_labels(np[i]) && li < np[i]->labels->count; li++) {
                const char *label = get_label_string(np[i]->labels->items[li]);
                if (label) cypher_schema_add_node_label(executor->schema_mgr, ids[i], label);
            }
            set_merge_node_props(executor, ids[i], np[i], result);
        }
        for (int r = 0; r < k - 1; r++) {
            int a = ids[r], b = ids[r + 1];
            if (rp[r]->left_arrow && !rp[r]->right_arrow) { a = ids[r + 1]; b = ids[r]; }
            eids[r] = cypher_schema_create_edge(executor->schema_mgr, a, b,
                                                rp[r]->type ? rp[r]->type : "RELATED");
            if (eids[r] < 0) {
                set_result_error(result, "Failed to create relationship in MERGE");
                return -1;
            }
            result->relationships_created++;
            set_merge_edge_props(executor, eids[r], rp[r], result);
        }
    }

    for (int i = 0; i < k; i++)
        if (np[i]->variable) set_variable_node_id(var_map, np[i]->variable, ids[i]);
    for (int r = 0; r < k - 1; r++)
        if (rp[r]->variable) set_variable_edge_id(var_map, rp[r]->variable, eids[r]);

    ast_list *set = matched ? merge->on_match : merge->on_create;
    if (set && set->count > 0 && execute_set_items(executor, set, var_map, result) < 0)
        return -1;
    return 0;
}

/* Execute MERGE clause — canonical signature (I-0041 C8–C10).
 *
 * external_vars may be NULL when MERGE is run without a preceding
 * MATCH-bound scope. out_var_map may be NULL when the caller doesn't
 * need the resulting variable bindings (e.g. terminal MERGE without
 * trailing RETURN/SET). */
int execute_merge_clause(cypher_executor *executor, cypher_merge *merge,
                         cypher_result *result, variable_map *external_vars,
                         variable_map **out_var_map)
{
    if (out_var_map) *out_var_map = NULL;
    if (!executor || !merge || !result) {
        return -1;
    }

    if (!merge->pattern) {
        set_result_error(result, "No pattern in MERGE clause");
        return -1;
    }

    CYPHER_DEBUG("Executing MERGE clause with %d patterns", merge->pattern->count);

    /* Create variable map to track node variables */
    variable_map *var_map = create_variable_map();
    if (!var_map) {
        set_result_error(result, "Failed to create variable map");
        return -1;
    }

    /* Pre-populate with external variables (e.g., from CALL subquery scope) */
    if (external_vars) {
        for (int ev = 0; ev < external_vars->count; ev++) {
            variable_mapping *m = &external_vars->mappings[ev];
            if (m->type == VAR_MAP_TYPE_NODE) {
                set_variable_node_id(var_map, m->variable, m->entity_id);
                CYPHER_DEBUG("MERGE: inherited variable '%s' = node %d from outer scope",
                             m->variable, m->entity_id);
            } else if (m->type == VAR_MAP_TYPE_EDGE) {
                set_variable_edge_id(var_map, m->variable, m->entity_id);
                CYPHER_DEBUG("MERGE: inherited variable '%s' = edge %d from outer scope",
                             m->variable, m->entity_id);
            }
        }
    }

    /* Track which nodes were created vs matched */
    int nodes_matched = 0;
    int nodes_created_in_merge = 0;

    /* Process each path pattern in the MERGE clause */
    for (int p = 0; p < merge->pattern->count; p++) {
        ast_node *pattern = merge->pattern->items[p];

        if (pattern->type != AST_NODE_PATH) {
            CYPHER_DEBUG("Unexpected pattern type in MERGE: %d", pattern->type);
            continue;
        }

        cypher_path *path = (cypher_path*)pattern;
        if (!path->elements) continue;
        if (path->elements->count > 1) {   /* GQLITE-T-0515 */
            if (merge_rel_path(executor, merge, path, var_map, result) < 0) { free_variable_map(var_map); return -1; }
            continue;
        }


        /* Process path elements: node, rel, node, rel, node, ... */
        for (int i = 0; i < path->elements->count; i++) {
            ast_node *element = path->elements->items[i];

            if (element->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *node_pattern = (cypher_node_pattern*)element;
                int node_id = -1;
                bool was_created = false;

                /* Check if this variable already exists in var_map */
                if (node_pattern->variable) {
                    node_id = get_variable_node_id(var_map, node_pattern->variable);
                    if (node_id >= 0) {
                        CYPHER_DEBUG("Reusing existing node %d for variable '%s'", node_id, node_pattern->variable);
                        continue;
                    }
                }

                /* Try to find existing node by label + properties */
                node_id = find_node_by_pattern(executor, node_pattern);

                if (node_id >= 0) {
                    /* Node exists - matched */
                    nodes_matched++;
                    CYPHER_DEBUG("MERGE matched existing node %d", node_id);
                } else {
                    /* Node doesn't exist - create it */
                    node_id = cypher_schema_create_node(executor->schema_mgr);
                    if (node_id < 0) {
                        set_result_error(result, "Failed to create node in MERGE");
                        free_variable_map(var_map);
                        return -1;
                    }

                    was_created = true;
                    nodes_created_in_merge++;
                    result->nodes_created++;
                    CYPHER_DEBUG("MERGE created new node %d", node_id);

                    /* Add labels if specified - supports multiple labels */
                    if (has_labels(node_pattern)) {
                        for (int li = 0; li < node_pattern->labels->count; li++) {
                            const char *label = get_label_string(node_pattern->labels->items[li]);
                            if (label) {
                                cypher_schema_add_node_label(executor->schema_mgr, node_id, label);
                            }
                        }
                    }

                    set_merge_node_props(executor, node_id, node_pattern, result);
                }

                /* Store variable mapping if present */
                if (node_pattern->variable) {
                    set_variable_node_id(var_map, node_pattern->variable, node_id);
                }


                /* Apply ON CREATE SET if node was created */
                if (was_created && merge->on_create && merge->on_create->count > 0) {
                    CYPHER_DEBUG("Applying ON CREATE SET for node %d", node_id);
                    if (execute_set_items(executor, merge->on_create, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

                /* Apply ON MATCH SET if node was matched */
                if (!was_created && merge->on_match && merge->on_match->count > 0) {
                    CYPHER_DEBUG("Applying ON MATCH SET for node %d", node_id);
                    if (execute_set_items(executor, merge->on_match, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

            }
        }
    }

    CYPHER_DEBUG("MERGE complete: %d nodes matched, %d nodes created", nodes_matched, nodes_created_in_merge);

    if (out_var_map) {
        *out_var_map = var_map;
    } else {
        free_variable_map(var_map);
    }
    return 0;
}
