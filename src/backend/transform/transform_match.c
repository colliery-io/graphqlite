/*
 * MATCH clause transformation
 * Converts MATCH patterns into SQL SELECT queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern);
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias);
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel, 
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node, int rel_index);

/* Transform a MATCH clause into SQL */
int transform_match_clause(cypher_transform_context *ctx, cypher_match *match)
{
    CYPHER_DEBUG("Transforming MATCH clause");
    
    if (!ctx || !match) {
        return -1;
    }
    
    /* Mark this as a read query */
    if (ctx->query_type == QUERY_TYPE_UNKNOWN) {
        ctx->query_type = QUERY_TYPE_READ;
    } else if (ctx->query_type == QUERY_TYPE_WRITE) {
        ctx->query_type = QUERY_TYPE_MIXED;
    }
    
    /* Start SELECT clause if this is the first clause */
    if (ctx->sql_size == 0) {
        append_sql(ctx, "SELECT ");
        /* We'll fill in the column list later in RETURN */
        append_sql(ctx, "* ");
    }
    
    /* Process each pattern in the MATCH */
    bool first_pattern = true;
    for (int i = 0; i < match->pattern->count; i++) {
        ast_node *pattern = match->pattern->items[i];
        
        if (pattern->type != AST_NODE_PATH) {
            ctx->has_error = true;
            ctx->error_message = strdup("Invalid pattern type in MATCH");
            return -1;
        }
        
        if (!first_pattern) {
            append_sql(ctx, ", ");
        }
        first_pattern = false;
        
        if (transform_match_pattern(ctx, pattern) < 0) {
            return -1;
        }
    }
    
    /* Handle WHERE clause if present */
    if (match->where) {
        if (transform_where_clause(ctx, match->where) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Transform a single pattern (path) */
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern)
{
    cypher_path *path = (cypher_path*)pattern;
    
    CYPHER_DEBUG("Transforming path with %d elements", path->elements->count);
    
    /* For now, handle simple node patterns */
    /* TODO: Handle relationship patterns */
    
    bool first_table = (ctx->sql_size == 0 || strstr(ctx->sql_buffer, "FROM") == NULL);
    
    for (int i = 0; i < path->elements->count; i++) {
        ast_node *element = path->elements->items[i];
        
        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            
            /* Generate table alias */
            char alias[32];
            if (node->variable) {
                snprintf(alias, sizeof(alias), "n_%s", node->variable);
            } else {
                snprintf(alias, sizeof(alias), "n_%d", i);
            }
            
            /* Register variable if present */
            if (node->variable) {
                register_variable(ctx, node->variable, alias);
            }
            
            /* Generate SQL for this node */
            if (generate_node_match(ctx, node, alias) < 0) {
                return -1;
            }
            
        } else if (element->type == AST_NODE_REL_PATTERN) {
            /* Handle relationship patterns - need surrounding nodes */
            if (i == 0 || i + 1 >= path->elements->count) {
                ctx->has_error = true;
                ctx->error_message = strdup("Relationship pattern must be between nodes");
                return -1;
            }
            
            ast_node *prev_element = path->elements->items[i - 1];
            ast_node *next_element = path->elements->items[i + 1];
            
            if (prev_element->type != AST_NODE_NODE_PATTERN || next_element->type != AST_NODE_NODE_PATTERN) {
                ctx->has_error = true;
                ctx->error_message = strdup("Relationship must connect node patterns");
                return -1;
            }
            
            cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
            cypher_node_pattern *source_node = (cypher_node_pattern*)prev_element;
            cypher_node_pattern *target_node = (cypher_node_pattern*)next_element;
            
            /* Generate relationship match SQL */
            if (generate_relationship_match(ctx, rel, source_node, target_node, i) < 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

/* Generate SQL for matching a node pattern */
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias)
{
    CYPHER_DEBUG("Generating match for node %s (label: %s)", 
                 node->variable ? node->variable : "<anonymous>",
                 node->label ? node->label : "<no label>");
    
    /* Check if we need FROM clause */
    if (strstr(ctx->sql_buffer, "FROM") == NULL) {
        append_sql(ctx, "FROM ");
    } else {
        append_sql(ctx, ", ");
    }
    
    /* Join nodes table */
    append_sql(ctx, "nodes AS %s", alias);
    
    /* Add label constraint if specified */
    if (node->label) {
        /* Check if we need WHERE clause */
        if (strstr(ctx->sql_buffer, "WHERE") == NULL) {
            append_sql(ctx, " WHERE ");
        } else {
            append_sql(ctx, " AND ");
        }
        
        /* Add label check using EXISTS subquery */
        append_sql(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.id AND label = ", alias);
        append_string_literal(ctx, node->label);
        append_sql(ctx, ")");
    }
    
    /* TODO: Handle property constraints */
    if (node->properties) {
        CYPHER_DEBUG("Property constraints not yet implemented");
    }
    
    return 0;
}

/* Generate SQL for matching a relationship pattern */
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node, int rel_index)
{
    CYPHER_DEBUG("Generating match for relationship %s between nodes", 
                 rel->type ? rel->type : "<no type>");
    
    /* Generate aliases for source and target nodes */
    char source_alias[32], target_alias[32], edge_alias[32];
    
    if (source_node->variable) {
        snprintf(source_alias, sizeof(source_alias), "n_%s", source_node->variable);
    } else {
        snprintf(source_alias, sizeof(source_alias), "n_%d", rel_index - 1);
    }
    
    if (target_node->variable) {
        snprintf(target_alias, sizeof(target_alias), "n_%s", target_node->variable);
    } else {
        snprintf(target_alias, sizeof(target_alias), "n_%d", rel_index + 1);
    }
    
    if (rel->variable) {
        snprintf(edge_alias, sizeof(edge_alias), "e_%s", rel->variable);
    } else {
        snprintf(edge_alias, sizeof(edge_alias), "e_%d", rel_index);
    }
    
    /* Add edges table to FROM clause */
    append_sql(ctx, ", edges AS %s", edge_alias);
    
    /* Add relationship direction constraints */
    if (strstr(ctx->sql_buffer, "WHERE") == NULL) {
        append_sql(ctx, " WHERE ");
    } else {
        append_sql(ctx, " AND ");
    }
    
    /* Handle relationship direction */
    if (rel->left_arrow && !rel->right_arrow) {
        /* <-[:TYPE]- (reversed: target -> source) */
        append_sql(ctx, "%s.source_id = %s.id AND %s.target_id = %s.id", 
                  edge_alias, target_alias, edge_alias, source_alias);
    } else {
        /* -[:TYPE]-> or -[:TYPE]- (forward or undirected, treat as forward) */
        append_sql(ctx, "%s.source_id = %s.id AND %s.target_id = %s.id", 
                  edge_alias, source_alias, edge_alias, target_alias);
    }
    
    /* Add relationship type constraint if specified */
    if (rel->type) {
        append_sql(ctx, " AND %s.type = ", edge_alias);
        append_string_literal(ctx, rel->type);
    }
    
    /* Register relationship variable if present */
    if (rel->variable) {
        register_variable(ctx, rel->variable, edge_alias);
    }
    
    CYPHER_DEBUG("Generated relationship match: %s connects %s to %s", 
                 edge_alias, source_alias, target_alias);
    
    return 0;
}

/* Transform WHERE clause - stub for now */
int transform_where_clause(cypher_transform_context *ctx, ast_node *where)
{
    CYPHER_DEBUG("WHERE clause transformation not yet implemented");
    
    /* TODO: Implement WHERE clause transformation */
    /* This will need to handle:
     * - Property access (n.name)
     * - Comparisons (=, <>, <, >, etc.)
     * - Logical operators (AND, OR, NOT)
     * - Functions
     */
    
    return 0;
}