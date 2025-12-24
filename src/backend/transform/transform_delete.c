/*
 * transform_delete.c
 *    Functions for transforming Cypher DELETE clauses into SQL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transform/cypher_transform.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int generate_delete_operations(cypher_transform_context *ctx, cypher_delete *delete_clause);
static int generate_node_delete(cypher_transform_context *ctx, const char *variable);
static int generate_edge_delete(cypher_transform_context *ctx, const char *variable);
static int check_node_constraints(cypher_transform_context *ctx, const char *variable);

/*
 * transform_delete_clause
 *    Transform a DELETE clause into SQL DELETE operations
 */
int transform_delete_clause(cypher_transform_context *ctx, cypher_delete *delete_clause)
{
    if (!ctx || !delete_clause) {
        return -1;
    }
    
    CYPHER_DEBUG("Transforming DELETE clause with %d items", delete_clause->items->count);
    
    /* Generate DELETE operations for each item */
    if (generate_delete_operations(ctx, delete_clause) < 0) {
        ctx->has_error = true;
        if (!ctx->error_message) {
            ctx->error_message = strdup("Failed to generate DELETE operations");
        }
        return -1;
    }
    
    return 0;
}

/*
 * generate_delete_operations
 *    Generate SQL DELETE statements for each variable in the DELETE clause
 */
static int generate_delete_operations(cypher_transform_context *ctx, cypher_delete *delete_clause)
{
    /* Process each delete item */
    for (int i = 0; i < delete_clause->items->count; i++) {
        cypher_delete_item *item = (cypher_delete_item*)delete_clause->items->items[i];
        if (!item || !item->variable) {
            continue;
        }
        
        CYPHER_DEBUG("Processing DELETE for variable: %s", item->variable);
        
        /* Check if variable exists and its type */
        bool is_edge = is_edge_variable(ctx, item->variable);
        const char *alias = lookup_variable_alias(ctx, item->variable);
        
        if (!alias) {
            ctx->has_error = true;
            char error[256];
            snprintf(error, sizeof(error), "Variable '%s' not defined in DELETE clause", item->variable);
            ctx->error_message = strdup(error);
            return -1;
        }
        
        if (i > 0) {
            append_sql(ctx, "; ");
        }
        
        if (is_edge) {
            /* Delete edge and its properties */
            if (generate_edge_delete(ctx, item->variable) < 0) {
                return -1;
            }
        } else {
            /* For DETACH DELETE, skip constraint checks and always cascade delete */
            if (!delete_clause->detach) {
                /* Check constraints before deleting node (only for regular DELETE) */
                if (check_node_constraints(ctx, item->variable) < 0) {
                    return -1;
                }
            }
            
            /* Delete node and cascade delete relationships */
            if (generate_node_delete(ctx, item->variable) < 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

/*
 * generate_node_delete
 *    Generate SQL to delete a node and its properties  
 */
static int generate_node_delete(cypher_transform_context *ctx, const char *variable)
{
    const char *alias = lookup_variable_alias(ctx, variable);
    if (!alias) {
        return -1;
    }
    
    CYPHER_DEBUG("Generating node DELETE for %s (alias: %s)", variable, alias);
    
    /* Delete node properties first */
    const char *prop_tables[] = {
        "node_props_text", "node_props_int", "node_props_real", "node_props_bool"
    };
    
    for (int i = 0; i < 4; i++) {
        append_sql(ctx, "DELETE FROM %s WHERE node_id = %s.id; ", 
                   prop_tables[i], alias);
    }
    
    /* Delete node labels */
    append_sql(ctx, "DELETE FROM node_labels WHERE node_id = %s.id; ", alias);
    
    /* Delete the node itself */
    append_sql(ctx, "DELETE FROM nodes WHERE id = %s.id", alias);
    
    return 0;
}

/*
 * generate_edge_delete
 *    Generate SQL to delete an edge and its properties
 */
static int generate_edge_delete(cypher_transform_context *ctx, const char *variable)
{
    const char *alias = lookup_variable_alias(ctx, variable);
    if (!alias) {
        return -1;
    }
    
    CYPHER_DEBUG("Generating edge DELETE for %s (alias: %s)", variable, alias);
    
    /* We need to reconstruct the MATCH subquery to get the edge IDs
     * This is a simplified approach - we'll delete edges by recreating the MATCH conditions */
    
    /* Delete edge properties first using IN subquery */
    const char *prop_tables[] = {
        "edge_props_text", "edge_props_int", "edge_props_real", "edge_props_bool"
    };
    
    for (int i = 0; i < 4; i++) {
        append_sql(ctx, "DELETE FROM %s WHERE edge_id IN (SELECT %s.id FROM edges AS %s); ", 
                   prop_tables[i], alias, alias);
    }
    
    /* Delete the edge itself using subquery */
    append_sql(ctx, "DELETE FROM edges WHERE id IN (SELECT %s.id FROM edges AS %s)", alias, alias);
    
    return 0;
}

/*
 * check_node_constraints
 *    Check if a node can be safely deleted (no connected edges)
 */
static int check_node_constraints(cypher_transform_context *ctx, const char *variable)
{
    const char *alias = lookup_variable_alias(ctx, variable);
    if (!alias) {
        return -1;
    }
    
    CYPHER_DEBUG("Checking constraints for node %s (alias: %s)", variable, alias);
    
    /* For now, we'll implement this as a runtime check in the executor
     * The SQL generation here would be too complex for constraint checking.
     * This function serves as a placeholder for future constraint validation.
     */
    
    return 0;
}