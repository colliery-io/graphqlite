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
static int generate_relationship_create(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                       cypher_node_pattern *source_node, cypher_node_pattern *target_node);

/* Helper to get label string from a label literal node */
static const char* get_label_string(ast_node *label_node)
{
    if (!label_node || label_node->type != AST_NODE_LITERAL) return NULL;
    cypher_literal *lit = (cypher_literal*)label_node;
    if (lit->literal_type != LITERAL_STRING) return NULL;
    return lit->value.string;
}

/* Helper to check if a node pattern has any labels */
static bool has_labels(cypher_node_pattern *node)
{
    return node && node->labels && node->labels->count > 0;
}

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
            /* Handle relationship creation - need source and target nodes */
            if (i == 0 || i >= path->elements->count - 1) {
                ctx->has_error = true;
                ctx->error_message = strdup("Relationship must be between two nodes");
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
            
            if (generate_relationship_create(ctx, rel, source_node, target_node) < 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

/* Generate SQL for creating a node */
static int generate_node_create(cypher_transform_context *ctx, cypher_node_pattern *node)
{
    const char *first_label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
    CYPHER_DEBUG("Generating CREATE for node %s (labels: %s, count: %d)",
                 node->variable ? node->variable : "<anonymous>",
                 first_label ? first_label : "<no label>",
                 node->labels ? node->labels->count : 0);

    /* Start a new statement if needed */
    if (ctx->sql_size > 0) {
        append_sql(ctx, "; ");
    }

    /* Insert into nodes table */
    append_sql(ctx, "INSERT INTO nodes DEFAULT VALUES");

    /* If we need to track the node ID for labels or properties */
    if (has_labels(node) || node->properties || node->variable) {
        append_sql(ctx, "; ");

        /* Get the last inserted node ID */
        /* In a real implementation, we'd need to handle this better,
         * possibly using RETURNING clause or last_insert_rowid() */

        /* Insert each label */
        if (has_labels(node)) {
            for (int i = 0; i < node->labels->count; i++) {
                const char *label = get_label_string(node->labels->items[i]);
                if (label) {
                    if (i > 0) append_sql(ctx, "; ");
                    append_sql(ctx, "INSERT INTO node_labels (node_id, label) VALUES (last_insert_rowid(), ");
                    append_string_literal(ctx, label);
                    append_sql(ctx, ")");
                }
            }
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

/* Generate SQL for creating a relationship */
static int generate_relationship_create(cypher_transform_context *ctx, cypher_rel_pattern *rel, 
                                       cypher_node_pattern *source_node, cypher_node_pattern *target_node)
{
    CYPHER_DEBUG("Generating CREATE for relationship %s between nodes %s and %s", 
                 rel->type ? rel->type : "<no type>",
                 source_node->variable ? source_node->variable : "<anonymous>",
                 target_node->variable ? target_node->variable : "<anonymous>");
    
    /* We need to know the IDs of the source and target nodes */
    /* For now, assume they were just created and use last_insert_rowid() approach */
    /* In a more sophisticated implementation, we'd track created node IDs properly */
    
    /* Start a new statement */
    append_sql(ctx, "; ");
    
    /* Create temporary variables for source and target node IDs */
    /* This is a simplified approach - in reality we'd need better ID tracking */
    char source_var[64], target_var[64];
    
    if (source_node->variable) {
        snprintf(source_var, sizeof(source_var), "@%s_id", source_node->variable);
    } else {
        snprintf(source_var, sizeof(source_var), "@source_id");
    }
    
    if (target_node->variable) {
        snprintf(target_var, sizeof(target_var), "@%s_id", target_node->variable);
    } else {
        snprintf(target_var, sizeof(target_var), "@target_id");
    }
    
    /* For a simple implementation, assume the nodes were just created in sequence */
    /* This needs improvement for real-world usage */
    
    /* Insert into edges table */
    append_sql(ctx, "INSERT INTO edges (source_id, target_id, type) VALUES (");
    
    /* Handle direction - if left arrow only, reverse source/target */
    if (rel->left_arrow && !rel->right_arrow) {
        /* <-[:TYPE]- means target -> source */
        append_sql(ctx, "(SELECT id FROM nodes WHERE rowid = last_insert_rowid() - 1), ");
        append_sql(ctx, "(SELECT id FROM nodes WHERE rowid = last_insert_rowid()), ");
    } else {
        /* -[:TYPE]-> or -[:TYPE]- (forward or undirected, treat as forward) */
        append_sql(ctx, "(SELECT id FROM nodes WHERE rowid = last_insert_rowid()), ");
        append_sql(ctx, "(SELECT id FROM nodes WHERE rowid = last_insert_rowid() - 1), ");
    }
    
    /* Add relationship type */
    if (rel->type) {
        append_string_literal(ctx, rel->type);
    } else {
        append_sql(ctx, "''"); /* Empty string for no type */
    }
    
    append_sql(ctx, ")");
    
    /* Register relationship variable if present */
    if (rel->variable) {
        register_variable(ctx, rel->variable, "last_insert_rowid()");
    }
    
    /* TODO: Handle relationship properties */
    if (rel->properties) {
        CYPHER_DEBUG("Relationship property creation not yet implemented");
    }
    
    return 0;
}