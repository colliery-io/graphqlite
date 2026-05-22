/*
 * SET clause transformation
 * Converts SET patterns into SQL UPDATE queries for property updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
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

    /* Start a new statement if needed (I-0039 migration). */
    if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
        sql_raw(ctx->unified_builder, "; ");
    }
    
    /* Determine the property type from the value expression */
    bool is_text = false;
    bool is_integer = false;
    bool is_real = false;
    bool is_json = false;

    if (value_expr->type == AST_NODE_MAP || value_expr->type == AST_NODE_LIST) {
        /* Map or list literal — store as JSON */
        is_json = true;
    } else if (value_expr->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal*)value_expr;
        switch (lit->literal_type) {
            case LITERAL_STRING:
                is_text = true;
                break;
            case LITERAL_INTEGER:
                is_integer = true;
                break;
            case LITERAL_DECIMAL:
                is_real = true;
                break;
            case LITERAL_BOOLEAN:
                is_text = true; /* Store booleans as text */
                break;
            case LITERAL_NULL:
                is_text = true; /* Default to text for NULL */
                break;
        }
    } else {
        /* For non-literal expressions, default to text */
        is_text = true;
    }

    /* T-0314: choose node vs edge property table based on variable
     * kind. Pre-T-0314, SET always emitted node_props_* + node_id
     * even for edge variables — SET on relationships silently wrote
     * to the wrong table (Set6 [19]/[21] family). */
    bool is_edge = transform_var_is_edge(ctx->var_ctx, variable);
    const char *prop_table;
    const char *entity_col = is_edge ? "edge_id" : "node_id";
    if (is_edge) {
        if (is_json)        prop_table = "edge_props_json";
        else if (is_integer) prop_table = "edge_props_int";
        else if (is_real)    prop_table = "edge_props_real";
        else                 prop_table = "edge_props_text";
    } else {
        if (is_json)        prop_table = "node_props_json";
        else if (is_integer) prop_table = "node_props_int";
        else if (is_real)    prop_table = "node_props_real";
        else                 prop_table = "node_props_text";
    }

    /* T-0314: capture the value expression with typed (non-CAST-to-TEXT)
     * property lookups. The default RETURN-context lookup wraps integer
     * properties in CAST AS TEXT — fine for projection but breaks
     * arithmetic (`r.num + 1` becomes string concat → "1" + "1" = "11"
     * instead of 2). Borrow the in_comparison flag so transform_property_
     * access picks the type-preserving branch. */
    bool saved_in_cmp = ctx->in_comparison;
    ctx->in_comparison = true;
    char *value_sql = cypher_transform_capture_expression(ctx, value_expr);
    ctx->in_comparison = saved_in_cmp;
    if (!value_sql) return -1;

    char *escaped_prop = escape_sql_string(property_name);
    if (!escaped_prop) escaped_prop = strdup(property_name ? property_name : "");

    /* Statement separator before this SET item's emission. */
    if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
        sql_raw(ctx->unified_builder, "; ");
    }

    /* T-0314: emit INSERT OR REPLACE INTO <prop_table> FIRST, while
     * the value expression's `n.num` lookup can still see the old
     * value across all typed tables. (Reversing this order — DELETE
     * other tables first, then INSERT — breaks because the value SQL
     * reads n.num through COALESCE across all tables, returning NULL
     * after the DELETE.) Then DELETE the same (entity_id, key_id)
     * row from every OTHER typed prop table so the next read returns
     * the just-written value (Set6 [7]/[21] aggregating-after-SET). */
    sql_raw(ctx->unified_builder,
        "INSERT OR REPLACE INTO %s (%s, key_id, value) SELECT %s.id, "
        "(SELECT id FROM property_keys WHERE key = '%s'), %s",
        prop_table, entity_col, table_alias, escaped_prop, value_sql);
    free(value_sql);

    /* Build the post-INSERT DELETE list for the other typed tables. */
    const char *node_tables[] = {
        "node_props_text", "node_props_int", "node_props_real",
        "node_props_bool", "node_props_json", NULL
    };
    const char *edge_tables[] = {
        "edge_props_text", "edge_props_int", "edge_props_real",
        "edge_props_bool", "edge_props_json", NULL
    };
    const char **all_tables = is_edge ? edge_tables : node_tables;
    const char *from_str = sql_builder_get_from(ctx->unified_builder);
    const char *joins_str = sql_builder_get_joins(ctx->unified_builder);
    const char *where_str = sql_builder_get_where(ctx->unified_builder);
    bool have_from = (from_str && from_str[0]);

    /* We need the INSERT's FROM/JOINs/WHERE to be emitted BEFORE we
     * append the DELETEs (so the INSERT's SELECT is well-formed).
     * The caller appends those right after this function returns
     * (see lines below). Defer DELETE emission until after the
     * caller-side FROM/JOIN/WHERE block — emit them now by capturing
     * the strings and appending after a final "; " separator. */
    dynamic_buffer post_dml;
    dbuf_init(&post_dml);
    for (int ti = 0; all_tables[ti]; ti++) {
        if (strcmp(all_tables[ti], prop_table) == 0) continue;
        if (!dbuf_is_empty(&post_dml)) {
            dbuf_append(&post_dml, "; ");
        }
        dbuf_appendf(&post_dml,
            "DELETE FROM %s WHERE key_id = (SELECT id FROM property_keys WHERE key = '%s')",
            all_tables[ti], escaped_prop);
        if (have_from) {
            dbuf_appendf(&post_dml,
                " AND %s IN (SELECT %s.id FROM %s",
                entity_col, table_alias, from_str);
            if (joins_str && joins_str[0]) {
                dbuf_appendf(&post_dml, " %s", joins_str);
            }
            if (where_str && where_str[0]) {
                dbuf_appendf(&post_dml, " WHERE %s", where_str);
            }
            dbuf_appendf(&post_dml, ")");
        }
    }
    free(escaped_prop);

    /* Append the INSERT's FROM / JOINs / WHERE so the statement is
     * one complete SQL unit. */
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

    /* Now emit the post-INSERT cross-table DELETEs. */
    const char *post = dbuf_get(&post_dml);
    if (post && *post) {
        sql_raw(ctx->unified_builder, "; %s", post);
    }
    dbuf_free(&post_dml);

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
                sql_raw(ctx->unified_builder, "(SELECT %s.id FROM %s", table_alias, dbuf_get(&ctx->unified_builder->from));
                if (!dbuf_is_empty(&ctx->unified_builder->joins)) {
                    sql_raw(ctx->unified_builder, " %s", dbuf_get(&ctx->unified_builder->joins));
                }
                if (!dbuf_is_empty(&ctx->unified_builder->where)) {
                    sql_raw(ctx->unified_builder, " WHERE %s", dbuf_get(&ctx->unified_builder->where));
                }
                sql_raw(ctx->unified_builder, ")");
            } else {
                sql_raw(ctx->unified_builder, "%s.id", table_alias);
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
    
    /* Start a new statement if needed (I-0039 migration). */
    if (!dbuf_is_empty(&ctx->unified_builder->raw_output)) {
        sql_raw(ctx->unified_builder, "; ");
    }
    
    {
        char *escaped_lbl = escape_sql_string(label_name);
        if (!escaped_lbl) escaped_lbl = strdup(label_name ? label_name : "");
        sql_raw(ctx->unified_builder,
            "INSERT OR IGNORE INTO node_labels (node_id, label) SELECT %s.id, '%s'",
            table_alias, escaped_lbl);
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