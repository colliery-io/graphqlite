/*
 * CREATE clause transformation
 * Converts CREATE patterns into SQL INSERT queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_create_pattern(cypher_transform_context *ctx, ast_node *pattern);
static int generate_node_create(cypher_transform_context *ctx, cypher_node_pattern *node);

/* Transform a CREATE clause into SQL */
int transform_create_clause(cypher_transform_context *ctx, cypher_create *create)
{
    CYPHER_DEBUG("Transforming CREATE clause");
    
    if (!ctx || !create) {
        return -1;
    }
    
    /* Mark this as a write query */
    if (ctx->query_type == QUERY_TYPE_UNKNOWN) {
        ctx->query_type = QUERY_TYPE_WRITE;
    } else if (ctx->query_type == QUERY_TYPE_READ) {
        ctx->query_type = QUERY_TYPE_MIXED;
    }
    
    /* Process each pattern in the CREATE */
    for (int i = 0; i < create->pattern->count; i++) {
        ast_node *pattern = create->pattern->items[i];
        
        if (pattern->type != AST_NODE_PATH) {
            ctx->has_error = true;
            ctx->error_message = strdup("Invalid pattern type in CREATE");
            return -1;
        }
        
        if (transform_create_pattern(ctx, pattern) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Transform a single CREATE pattern (path) */
static int transform_create_pattern(cypher_transform_context *ctx, ast_node *pattern)
{
    cypher_path *path = (cypher_path*)pattern;
    
    CYPHER_DEBUG("Transforming CREATE path with %d elements", path->elements->count);
    
    /* Process each element in the path */
    for (int i = 0; i < path->elements->count; i++) {
        ast_node *element = path->elements->items[i];
        
        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            
            if (generate_node_create(ctx, node) < 0) {
                return -1;
            }
            
        } else if (element->type == AST_NODE_REL_PATTERN) {
            /* TODO: Handle relationship creation */
            ctx->has_error = true;
            ctx->error_message = strdup("Relationship creation not yet implemented");
            return -1;
        }
    }
    
    return 0;
}

/* Generate SQL for creating a node */
static int generate_node_create(cypher_transform_context *ctx, cypher_node_pattern *node)
{
    CYPHER_DEBUG("Generating CREATE for node %s (label: %s)", 
                 node->variable ? node->variable : "<anonymous>",
                 node->label ? node->label : "<no label>");
    
    /* Start a new statement if needed */
    if (ctx->sql_size > 0) {
        append_sql(ctx, "; ");
    }
    
    /* Insert into nodes table */
    append_sql(ctx, "INSERT INTO nodes DEFAULT VALUES");
    
    /* If we need to track the node ID for labels or properties */
    if (node->label || node->properties || node->variable) {
        append_sql(ctx, "; ");
        
        /* Get the last inserted node ID */
        /* In a real implementation, we'd need to handle this better,
         * possibly using RETURNING clause or last_insert_rowid() */
        
        if (node->label) {
            /* Insert label */
            append_sql(ctx, "INSERT INTO node_labels (node_id, label) VALUES (last_insert_rowid(), ");
            append_string_literal(ctx, node->label);
            append_sql(ctx, ")");
        }
        
        if (node->properties) {
            /* TODO: Handle property creation */
            /* This would involve parsing the property map and creating
             * appropriate entries in the properties table */
            CYPHER_DEBUG("Property creation not yet implemented");
        }
        
        if (node->variable) {
            /* Register the variable for later use */
            /* In a real implementation, we'd need to track the created node ID */
            register_variable(ctx, node->variable, "last_insert_rowid()");
        }
    }
    
    return 0;
}