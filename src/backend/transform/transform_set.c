/*
 * SET clause transformation
 * Converts SET patterns into SQL UPDATE queries for property updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_set_item(cypher_transform_context *ctx, cypher_set_item *item);
static int generate_property_update(cypher_transform_context *ctx,
                                   const char *variable, const char *property_name,
                                   ast_node *value_expr);
static int generate_bulk_property_update(cypher_transform_context *ctx,
                                        const char *variable, cypher_map *map,
                                        bool is_merge);
static int generate_label_add(cypher_transform_context *ctx,
                             const char *variable, const char *label_name);

/* Transform a SET clause into SQL */
/* Replace every occurrence of `from` in `*field` with `to` (in place). */
static void replace_all_in(char **field, const char *from, const char *to)
{
    if (!field || !*field || !strstr(*field, from)) return;
    size_t flen = strlen(from), tlen = strlen(to);
    dynamic_buffer out;
    dbuf_init(&out);
    const char *p = *field;
    for (;;) {
        const char *hit = strstr(p, from);
        if (!hit) { dbuf_append(&out, p); break; }
        dbuf_appendf(&out, "%.*s", (int)(hit - p), p);
        dbuf_append(&out, to);
        p = hit + flen;
        (void)tlen;
    }
    free(*field);
    *field = dbuf_finish(&out);
}

/* T-0370: snapshot the pipeline before a write.
 *
 * A SET that follows WITH/UNWIND is executed as pre-exec DML, and the
 * trailing RETURN is a separate statement that re-evaluates the pipeline's
 * CTEs AFTER the write. Any value computed before the SET — `WITH a,
 * a.name AS old SET a.name = 'x' RETURN old`, or the `collect(a)` entity
 * snapshots in List12 [1]/[2] — therefore saw the post-write state.
 *
 * When the pipeline at the SET is exactly one CTE (`FROM _with_N` /
 * `FROM _unwind_N`, no joins, no pending ORDER/LIMIT/GROUP), materialize
 * it into a temp table as the first pre-exec statement, point the builder
 * and every variable at that table, and let the DML and the RETURN both
 * read the frozen rows. cypher_transform_query defers the final prepare
 * so the table exists when the SELECT is compiled. */
static int set_snapshot_pipeline(cypher_transform_context *ctx)
{
    sql_builder *b = ctx->unified_builder;
    if (!b || !sql_builder_has_from(b)) return 0;
    if (b->select_count > 0 || b->order_count > 0 || b->group_count > 0 ||
        b->limit >= 0 || b->offset >= 0 || b->limit_expr || b->offset_expr || b->distinct)
        return 0;
    const char *joins = sql_builder_get_joins(b);
    if (joins && joins[0]) return 0;

    const char *from = sql_builder_get_from(b);
    if (strncmp(from, "_with_", 6) != 0 && strncmp(from, "_unwind_", 8) != 0) return 0;
    for (const char *p = from; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') return 0;  /* alias or join text */
    }

    char snap[32];
    snprintf(snap, sizeof(snap), "_gql_pipe_%d", ctx->pipe_snapshot_count++);
    const char *where = sql_builder_get_where(b);

    if (!dbuf_is_empty(&b->raw_output)) sql_raw(b, "; ");
    sql_raw(b, "DROP TABLE IF EXISTS temp.%s; CREATE TEMP TABLE %s AS SELECT * FROM %s", snap, snap, from);
    if (where && where[0]) sql_raw(b, " WHERE %s", where);

    /* Rebind every variable from `<cte>.` to `<snap>.` — aliases are either
     * a plain column ref (`_with_0.n`) or an expression over one
     * (`json_extract(_unwind_0.value, '$.id')`). */
    char from_dot[48], snap_dot[48];
    snprintf(from_dot, sizeof(from_dot), "%s.", from);
    snprintf(snap_dot, sizeof(snap_dot), "%s.", snap);
    int n = transform_var_count(ctx->var_ctx);
    for (int i = 0; i < n; i++) {
        transform_var *v = transform_var_at(ctx->var_ctx, i);
        if (!v) continue;
        replace_all_in(&v->table_alias, from_dot, snap_dot);
        replace_all_in(&v->source_expr, from_dot, snap_dot);
        replace_all_in(&v->tag_expr, from_dot, snap_dot);
    }

    dbuf_clear(&b->where);
    b->where_count = 0;
    sql_from(b, snap, NULL);
    CYPHER_DEBUG("SET: snapshotted pipeline %s into %s", from, snap);
    return 0;
}

int transform_set_clause(cypher_transform_context *ctx, cypher_set *set)
{
    CYPHER_DEBUG("Transforming SET clause");
    
    if (!ctx || !set) {
        return -1;
    }
    
    /* Mark this as a write query */
    if (ctx->query_type == QUERY_TYPE_UNKNOWN) {
        ctx->query_type = QUERY_TYPE_WRITE;
    } else if (ctx->query_type == QUERY_TYPE_READ) {
        ctx->query_type = QUERY_TYPE_MIXED;
    }

    /* T-0370: only a property SET is split into pre-exec DML (it emits
     * INSERT OR REPLACE); a label-only SET stays in the legacy compound,
     * where a snapshot table would never be created. */
    bool has_property_item = false;
    for (int i = 0; set->items && i < set->items->count; i++) {
        cypher_set_item *item = (cypher_set_item*)set->items->items[i];
        if (item && item->property && item->property->type != AST_NODE_LABEL_EXPR) {
            has_property_item = true;
            break;
        }
    }
    if (has_property_item && set_snapshot_pipeline(ctx) < 0) return -1;
    
    /* Process each SET item */
    for (int i = 0; i < set->items->count; i++) {
        cypher_set_item *item = (cypher_set_item*)set->items->items[i];
        
        if (transform_set_item(ctx, item) < 0) {
            return -1;
        }
        
        /* Add separator between SET items if not the last one */
        if (i < set->items->count - 1) {
            sql_raw(ctx->unified_builder, "; ");
        }
    }
    
    return 0;
}

/* Transform a single SET item (e.g., n.prop = value or n:Label) */
static int transform_set_item(cypher_transform_context *ctx, cypher_set_item *item)
{
    CYPHER_DEBUG("Transforming SET item");
    
    if (!item || !item->property) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid SET item");
        return -1;
    }
    
    /* Check if this is a label expression (SET n:Label) */
    if (item->property->type == AST_NODE_LABEL_EXPR) {
        cypher_label_expr *label_expr = (cypher_label_expr*)item->property;
        
        /* The base expression should be an identifier (the variable) */
        if (label_expr->expr->type != AST_NODE_IDENTIFIER) {
            ctx->has_error = true;
            ctx->error_message = strdup("SET label must be on a variable");
            return -1;
        }
        
        cypher_identifier *var_id = (cypher_identifier*)label_expr->expr;
        
        /* Generate the label add SQL */
        return generate_label_add(ctx, var_id->name, label_expr->label_name);
    }
    
    /* Check for bulk SET: SET n = {map} or SET n += {map} */
    if (item->property->type == AST_NODE_IDENTIFIER && item->expr &&
        item->expr->type == AST_NODE_MAP) {
        cypher_identifier *var_id = (cypher_identifier*)item->property;
        return generate_bulk_property_update(ctx, var_id->name,
                                            (cypher_map*)item->expr, item->is_merge);
    }

    /* Otherwise, it should be a property access expression (n.prop) */
    if (!item->expr) {
        ctx->has_error = true;
        ctx->error_message = strdup("SET property assignment requires a value");
        return -1;
    }

    if (item->property->type != AST_NODE_PROPERTY) {
        ctx->has_error = true;
        ctx->error_message = strdup("SET target must be a property (variable.property) or label (variable:Label)");
        return -1;
    }
    
    cypher_property *prop = (cypher_property*)item->property;
    
    /* The base expression should be an identifier (the variable) */
    if (prop->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("SET property must be on a variable");
        return -1;
    }
    
    cypher_identifier *var_id = (cypher_identifier*)prop->expr;
    
    /* Generate the property update SQL */
    return generate_property_update(ctx, var_id->name, prop->property_name, item->expr);
}

/* T-0370: the id expression for a SET target. A variable bound by MATCH has
 * a table alias (`n` -> `n.id`); a post-WITH / post-UNWIND entity has
 * alias_is_id set and its "alias" already IS the id expression (e.g.
 * `_with_0.n` or `json_extract(_unwind_0.value, '$.id')`), so appending
 * `.id` produced `no such column` / syntax errors (List12 [1]/[2]). */
static const char *set_entity_id_expr(cypher_transform_context *ctx, const char *variable,
                                      const char *table_alias, char *buf, size_t buflen)
{
    if (transform_var_alias_is_id(ctx->var_ctx, variable)) return table_alias;
    snprintf(buf, buflen, "%s.id", table_alias);
    return buf;
}

/* Generate SQL to update a property */
static int generate_property_update(cypher_transform_context *ctx, 
                                   const char *variable, const char *property_name, 
                                   ast_node *value_expr)
{
    CYPHER_DEBUG("Generating property update for %s.%s", variable, property_name);
    
    /* Check if variable is bound (from a previous MATCH) */
    if (!transform_var_is_bound(ctx->var_ctx, variable)) {
        /* For now, assume the variable exists - in a real implementation
         * we'd need to handle unbound variables properly */
        CYPHER_DEBUG("Warning: Variable %s not bound, assuming it exists", variable);
    }
    
    /* Get the table alias for the variable */
    const char *table_alias = transform_var_get_alias(ctx->var_ctx, variable);
    if (!table_alias) {
        ctx->has_error = true;
        ctx->error_message = strdup("Unknown variable in SET clause");
        return -1;
    }
    char id_buf[320];
    const char *id_expr = set_entity_id_expr(ctx, variable, table_alias, id_buf, sizeof(id_buf));

    /* Start a new statement if needed (I-0039 migration). */
    if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
        sql_raw(ctx->unified_builder, "; ");
    }
    
    /* GQLITE-T-0406: the value's type decides its table, at run time. The
     * value, the entity id and a type tag are staged in one pass over the
     * MATCH (so a WHERE on the property being set is read once, before the
     * write), then each row goes to the table of its type and is removed
     * from the others. A null removes the property. The tag comes from the
     * expression's shape when that is enough (literal, comparison, list or
     * map), else from _gql_typetag, which reads the boolean and JSON
     * subtypes. Before, every expression that was not a literal was stored
     * as text, booleans too. */
    bool is_edge = transform_var_is_edge(ctx->var_ctx, variable);
    const char *entity_col = is_edge ? "edge_id" : "node_id";
    const char *pfx = is_edge ? "edge_props_" : "node_props_";

    char *value_sql = cypher_transform_capture_expression(ctx, value_expr);
    if (!value_sql) return -1;

    const char *static_tag = NULL;
    if (ast_is_boolean_expr(value_expr)) {
        static_tag = "bool";
    } else if (value_expr->type == AST_NODE_MAP || value_expr->type == AST_NODE_LIST) {
        static_tag = "json";
    } else if (value_expr->type == AST_NODE_LITERAL) {
        switch (((cypher_literal *)value_expr)->literal_type) {
            case LITERAL_STRING:  static_tag = "text"; break;
            case LITERAL_INTEGER: static_tag = "int"; break;
            case LITERAL_DECIMAL: static_tag = "real"; break;
            case LITERAL_NULL:    static_tag = "null"; break;
            default: break;
        }
    }

    char *escaped_prop = escape_sql_string(property_name);
    if (!escaped_prop) escaped_prop = strdup(property_name ? property_name : "");

    const char *from_str = sql_builder_get_from(ctx->unified_builder);
    const char *joins_str = sql_builder_get_joins(ctx->unified_builder);
    const char *where_str = sql_builder_get_where(ctx->unified_builder);
    bool have_from = (from_str && from_str[0]);

    /* Statement separator before this SET item's emission. */
    if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
        sql_raw(ctx->unified_builder, "; ");
    }
    /* T-0370: a key first seen in this SET has no property_keys row yet;
     * the specialized MATCH+SET executor registers keys itself, the
     * generic transform path (SET after WITH/UNWIND) did not. */
    sql_raw(ctx->unified_builder,
        "CREATE TEMP TABLE IF NOT EXISTS _gql_set_stage (eid INTEGER, v, t TEXT); "
        "DELETE FROM temp._gql_set_stage; "
        "INSERT OR IGNORE INTO property_keys (key) VALUES ('%s'); ", escaped_prop);
    if (static_tag) {
        sql_raw(ctx->unified_builder,
            "INSERT INTO temp._gql_set_stage (eid, v, t) SELECT %s, %s, '%s'",
            id_expr, value_sql, static_tag);
    } else {
        sql_raw(ctx->unified_builder,
            "INSERT INTO temp._gql_set_stage (eid, v, t) SELECT %s, %s, COALESCE(_gql_typetag(%s), 'text')",
            id_expr, value_sql, value_sql);
    }
    free(value_sql);
    if (have_from) {
        sql_raw(ctx->unified_builder, " FROM %s", from_str);
        if (joins_str && joins_str[0]) {
            sql_raw(ctx->unified_builder, " %s", joins_str);
        }
        if (where_str && where_str[0]) {
            sql_raw(ctx->unified_builder, " WHERE %s", where_str);
        }
    } else {
        /* Fallback for non-builder mode - shouldn't happen after migration */
        sql_raw(ctx->unified_builder, " FROM nodes AS %s", table_alias);
    }
    /* _gql_typetag gives no tag for null: a null removes the property. */
    sql_raw(ctx->unified_builder, "; UPDATE temp._gql_set_stage SET t = 'null' WHERE v IS NULL");

    static const struct { const char *suffix; const char *tag; const char *value; } typed[] = {
        { "int",  "int",  "v" },
        { "real", "real", "v" },
        { "text", "text", "v" },
        { "bool", "bool", "CASE WHEN v = 'true' OR v = 1 THEN 1 ELSE 0 END" },
        { "json", "json", "v" },
    };
    for (size_t i = 0; i < sizeof(typed) / sizeof(typed[0]); i++) {
        sql_raw(ctx->unified_builder,
            "; INSERT OR REPLACE INTO %s%s (%s, key_id, value) "
            "SELECT eid, (SELECT id FROM property_keys WHERE key = '%s'), %s "
            "FROM temp._gql_set_stage WHERE t = '%s'",
            pfx, typed[i].suffix, entity_col, escaped_prop, typed[i].value, typed[i].tag);
        sql_raw(ctx->unified_builder,
            "; DELETE FROM %s%s WHERE key_id = (SELECT id FROM property_keys WHERE key = '%s') "
            "AND %s IN (SELECT eid FROM temp._gql_set_stage WHERE t <> '%s')",
            pfx, typed[i].suffix, escaped_prop, entity_col, typed[i].tag);
    }
    sql_raw(ctx->unified_builder, "; DELETE FROM temp._gql_set_stage");
    free(escaped_prop);

    CYPHER_DEBUG("Generated property update SQL");
    return 0;
}

/* Generate SQL for bulk property SET (SET n = {map} or SET n += {map}) */
static int generate_bulk_property_update(cypher_transform_context *ctx,
                                        const char *variable, cypher_map *map,
                                        bool is_merge)
{
    CYPHER_DEBUG("Generating bulk property %s for %s with %d pairs",
                 is_merge ? "+=" : "=", variable, map->pairs ? map->pairs->count : 0);

    /* Get the table alias for the variable */
    const char *table_alias = transform_var_get_alias(ctx->var_ctx, variable);
    if (!table_alias) {
        ctx->has_error = true;
        ctx->error_message = strdup("Unknown variable in bulk SET clause");
        return -1;
    }
    char id_buf[320];
    const char *id_expr = set_entity_id_expr(ctx, variable, table_alias, id_buf, sizeof(id_buf));

    bool is_edge = transform_var_is_edge(ctx->var_ctx, variable);

    /* For replace mode (=), delete all existing properties first */
    if (!is_merge) {
        const char *entity_col = is_edge ? "edge_id" : "node_id";
        const char *node_tables[] = {"node_props_text", "node_props_int", "node_props_real", "node_props_bool", "node_props_json"};
        const char *edge_tables[] = {"edge_props_text", "edge_props_int", "edge_props_real", "edge_props_bool", "edge_props_json"};
        const char **tables = is_edge ? edge_tables : node_tables;

        for (int i = 0; i < 5; i++) {
            if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
                sql_raw(ctx->unified_builder, "; ");
            }
            sql_raw(ctx->unified_builder, "DELETE FROM %s WHERE %s = ", tables[i], entity_col);

            /* Get entity ID — use subquery from unified builder */
            if (ctx->unified_builder && !dbuf_is_empty(&ctx->unified_builder->from)) {
                sql_raw(ctx->unified_builder, "(SELECT %s FROM %s", id_expr, dbuf_get(&ctx->unified_builder->from));
                if (!dbuf_is_empty(&ctx->unified_builder->joins)) {
                    sql_raw(ctx->unified_builder, " %s", dbuf_get(&ctx->unified_builder->joins));
                }
                if (!dbuf_is_empty(&ctx->unified_builder->where)) {
                    sql_raw(ctx->unified_builder, " WHERE %s", dbuf_get(&ctx->unified_builder->where));
                }
                sql_raw(ctx->unified_builder, ")");
            } else {
                sql_raw(ctx->unified_builder, "%s", id_expr);
            }
        }
    }

    /* Now INSERT each map pair into the appropriate property table */
    if (map->pairs) {
        for (int i = 0; i < map->pairs->count; i++) {
            cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
            if (!pair || !pair->key || !pair->value) continue;

            /* Use generate_property_update for each pair — it handles type routing */
            if (generate_property_update(ctx, variable, pair->key, pair->value) < 0) {
                return -1;
            }
        }
    }

    CYPHER_DEBUG("Generated bulk property %s SQL", is_merge ? "+=" : "=");
    return 0;
}

/* Generate SQL to add a label to a node */
static int generate_label_add(cypher_transform_context *ctx,
                             const char *variable, const char *label_name)
{
    CYPHER_DEBUG("Generating label add for %s:%s", variable, label_name);
    
    /* Get the table alias for the variable - if it doesn't exist, this is an error */
    const char *table_alias = transform_var_get_alias(ctx->var_ctx, variable);
    if (!table_alias) {
        ctx->has_error = true;
        ctx->error_message = strdup("Unknown variable in SET label - variable must be defined in MATCH clause");
        return -1;
    }
    char id_buf[320];
    
    /* Start a new statement if needed (I-0039 migration). */
    if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
        sql_raw(ctx->unified_builder, "; ");
    }
    
    {
        char *escaped_lbl = escape_sql_string(label_name);
        if (!escaped_lbl) escaped_lbl = strdup(label_name ? label_name : "");
        sql_raw(ctx->unified_builder,
            "INSERT OR IGNORE INTO node_labels (node_id, label) SELECT %s, '%s'",
            set_entity_id_expr(ctx, variable, table_alias, id_buf, sizeof(id_buf)), escaped_lbl);
        free(escaped_lbl);
    }

    /* Add FROM clause from unified builder if available */
    if (ctx->unified_builder && !dbuf_is_empty(&ctx->unified_builder->from)) {
        sql_raw(ctx->unified_builder, " FROM %s", dbuf_get(&ctx->unified_builder->from));
        if (!dbuf_is_empty(&ctx->unified_builder->joins)) {
            sql_raw(ctx->unified_builder, " %s", dbuf_get(&ctx->unified_builder->joins));
        }
        if (!dbuf_is_empty(&ctx->unified_builder->where)) {
            sql_raw(ctx->unified_builder, " WHERE %s", dbuf_get(&ctx->unified_builder->where));
        }
    } else {
        sql_raw(ctx->unified_builder, " FROM nodes AS %s", table_alias);
    }
    
    CYPHER_DEBUG("Generated label add SQL");
    return 0;
}