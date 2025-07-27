/*
 * SET clause transformation
 * Converts SET patterns into SQL UPDATE queries for property updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_set_item(cypher_transform_context *ctx, cypher_set_item *item);
static int generate_property_update(cypher_transform_context *ctx, 
                                   const char *variable, const char *property_name, 
                                   ast_node *value_expr);
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
            append_sql(ctx, "; ");
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
    if (!is_variable_bound(ctx, variable)) {
        /* For now, assume the variable exists - in a real implementation
         * we'd need to handle unbound variables properly */
        CYPHER_DEBUG("Warning: Variable %s not bound, assuming it exists", variable);
    }
    
    /* Get the table alias for the variable */
    const char *table_alias = lookup_variable_alias(ctx, variable);
    if (!table_alias) {
        ctx->has_error = true;
        ctx->error_message = strdup("Unknown variable in SET clause");
        return -1;
    }
    
    /* Start a new statement if needed */
    if (ctx->sql_size > 0) {
        append_sql(ctx, "; ");
    }
    
    /* Determine the property type from the value expression */
    bool is_text = false;
    bool is_integer = false;
    bool is_real = false;
    
    if (value_expr->type == AST_NODE_LITERAL) {
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
    
    /* Choose the appropriate property table */
    const char *prop_table;
    if (is_integer) {
        prop_table = "node_props_int";
    } else if (is_real) {
        prop_table = "node_props_real";
    } else {
        prop_table = "node_props_text";
    }
    
    /* Generate UPDATE or INSERT statement */
    /* First, try to update existing property */
    append_sql(ctx, "INSERT OR REPLACE INTO %s (node_id, property_name, value) ", prop_table);
    append_sql(ctx, "VALUES (");
    
    /* Get node ID - for now assume the variable refers to a node ID */
    /* In a more complete implementation, this would depend on how variables are tracked */
    if (table_alias && strstr(table_alias, "nodes")) {
        append_sql(ctx, "%s.id", table_alias);
    } else {
        /* Fallback - assume the variable contains the node ID directly */
        append_sql(ctx, "%s", table_alias);
    }
    
    append_sql(ctx, ", ");
    append_string_literal(ctx, property_name);
    append_sql(ctx, ", ");
    
    /* Transform the value expression */
    if (transform_expression(ctx, value_expr) < 0) {
        return -1;
    }
    
    append_sql(ctx, ")");
    
    CYPHER_DEBUG("Generated property update SQL");
    return 0;
}

/* Generate SQL to add a label to a node */
static int generate_label_add(cypher_transform_context *ctx,
                             const char *variable, const char *label_name)
{
    CYPHER_DEBUG("Generating label add for %s:%s", variable, label_name);
    
    /* Get the table alias for the variable - if it doesn't exist, this is an error */
    const char *table_alias = lookup_variable_alias(ctx, variable);
    if (!table_alias) {
        /* Try to get entity alias if legacy lookup fails */
        transform_entity *entity = lookup_entity(ctx, variable);
        if (entity) {
            table_alias = entity->table_alias;
        } else {
            ctx->has_error = true;
            ctx->error_message = strdup("Unknown variable in SET label - variable must be defined in MATCH clause");
            return -1;
        }
    }
    
    /* Start a new statement if needed */
    if (ctx->sql_size > 0) {
        append_sql(ctx, "; ");
    }
    
    /* Generate INSERT OR IGNORE to add the label */
    append_sql(ctx, "INSERT OR IGNORE INTO node_labels (node_id, label) ");
    append_sql(ctx, "SELECT %s.id, ", table_alias);
    append_string_literal(ctx, label_name);
    append_sql(ctx, " FROM nodes AS %s", table_alias);
    
    /* Add WHERE clause to match the specific nodes if needed */
    /* The nodes are already filtered by the MATCH clause */
    
    CYPHER_DEBUG("Generated label add SQL");
    return 0;
}