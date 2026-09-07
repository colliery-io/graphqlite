/*
 * entity_json_sql.h
 *
 * Perf review F5: the ONE definition of the SQL that renders a node or edge
 * as its canonical JSON object:
 *
 *   node: {"id":N,"labels":[...],"properties":{...}}
 *   edge: {"id":N,"type":"T","startNode":S,"endNode":E,"properties":{...}}
 *
 * The properties object is built from the five typed property tables by
 * entity id (a range scan on each (id, key_id) primary key) joined to
 * property_keys, instead of scanning all of property_keys and probing ten
 * indexes per key (O(nodes x global keys)). SQLite drops JSON subtypes across
 * the UNION ALL, so json() is re-applied at the aggregate: bool and json rows
 * carry a flag and are parsed as JSON, scalar rows go through json_quote()
 * so ints, reals and text keep their type. ORDER BY key_id preserves the
 * previous key order (property_keys rowid order).
 *
 * All functions return a malloc'd string (caller frees) or NULL on OOM.
 * `gprefix` is the multi-graph schema prefix ("" or "graph."), `id_expr` any
 * SQL expression yielding the entity id (an alias column or an integer).
 */
#ifndef GQL_ENTITY_JSON_SQL_H
#define GQL_ENTITY_JSON_SQL_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *gql_sql_vasprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = (char *)malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

/* COALESCE((SELECT json_group_object(...) ...), json('{}')) for one entity.
 * kind is "node" or "edge"; id_col is "node_id" or "edge_id". */
static inline char *gql_sql_props_expr(const char *kind, const char *id_col,
                                       const char *gprefix, const char *id_expr)
{
    return gql_sql_vasprintf(
        "COALESCE((SELECT json_group_object(pk.key, json(CASE WHEN p.j THEN p.v ELSE json_quote(p.v) END)) FROM ("
        "SELECT key_id, value AS v, 0 AS j FROM %s%s_props_text WHERE %s = %s "
        "UNION ALL SELECT key_id, value, 0 FROM %s%s_props_int WHERE %s = %s "
        "UNION ALL SELECT key_id, value, 0 FROM %s%s_props_real WHERE %s = %s "
        "UNION ALL SELECT key_id, CASE WHEN value THEN 'true' ELSE 'false' END, 1 FROM %s%s_props_bool WHERE %s = %s "
        "UNION ALL SELECT key_id, value, 1 FROM %s%s_props_json WHERE %s = %s "
        "ORDER BY 1) p JOIN %sproperty_keys pk ON pk.id = p.key_id), json('{}'))",
        gprefix, kind, id_col, id_expr,
        gprefix, kind, id_col, id_expr,
        gprefix, kind, id_col, id_expr,
        gprefix, kind, id_col, id_expr,
        gprefix, kind, id_col, id_expr,
        gprefix);
}

/* json_object('id', ..., 'labels', ..., 'properties', ...) for a node. */
static inline char *gql_sql_node_json_expr(const char *gprefix, const char *id_expr)
{
    char *props = gql_sql_props_expr("node", "node_id", gprefix, id_expr);
    if (!props) return NULL;
    char *out = gql_sql_vasprintf(
        "json_object('id', %s, "
        "'labels', COALESCE((SELECT json_group_array(label) FROM %snode_labels WHERE node_id = %s), json('[]')), "
        "'properties', %s)",
        id_expr, gprefix, id_expr, props);
    free(props);
    return out;
}

/* json_object('id', ..., 'type', ..., 'startNode', ..., 'endNode', ...,
 * 'properties', ...) for an edge. type/start/end are SQL expressions. */
static inline char *gql_sql_edge_json_expr(const char *gprefix, const char *id_expr,
                                           const char *type_expr, const char *start_expr,
                                           const char *end_expr)
{
    char *props = gql_sql_props_expr("edge", "edge_id", gprefix, id_expr);
    if (!props) return NULL;
    char *out = gql_sql_vasprintf(
        "json_object('id', %s, 'type', %s, 'startNode', %s, 'endNode', %s, 'properties', %s)",
        id_expr, type_expr, start_expr, end_expr, props);
    free(props);
    return out;
}

#endif /* GQL_ENTITY_JSON_SQL_H */
