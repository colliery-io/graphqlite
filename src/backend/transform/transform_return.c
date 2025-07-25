/*
 * RETURN clause transformation
 * Converts RETURN items into SQL SELECT projections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_return_item(cypher_transform_context *ctx, cypher_return_item *item, bool first);

/* Transform a RETURN clause */
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret)
{
    CYPHER_DEBUG("Transforming RETURN clause");
    
    if (!ctx || !ret) {
        return -1;
    }
    
    /* For write queries, RETURN means we need to select the created data */
    if (ctx->query_type == QUERY_TYPE_WRITE) {
        /* TODO: Handle returning created nodes/relationships */
        ctx->has_error = true;
        ctx->error_message = strdup("RETURN after CREATE not yet implemented");
        return -1;
    }
    
    /* Find the SELECT clause and replace the * with actual columns */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (!select_pos) {
        /* No SELECT clause yet - create one */
        if (ctx->sql_size > 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("RETURN without MATCH not supported");
            return -1;
        }
        append_sql(ctx, "SELECT ");
    } else {
        /* Replace the * with actual column list */
        /* Find the end of "SELECT *" and skip any spaces */
        char *after_star = select_pos + strlen("SELECT *");
        while (*after_star == ' ') after_star++; /* Skip any extra spaces */
        char *temp = strdup(after_star);
        
        /* Truncate at SELECT */
        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';
        
        /* Add DISTINCT if needed */
        if (ret->distinct) {
            append_sql(ctx, "DISTINCT ");
        }
        
        /* Process return items */
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
            if (transform_return_item(ctx, item, i == 0) < 0) {
                free(temp);
                return -1;
            }
        }
        
        /* Append the rest of the query */
        append_sql(ctx, " %s", temp);
        free(temp);
        
        return 0;
    }
    
    /* Add DISTINCT if specified */
    if (ret->distinct) {
        append_sql(ctx, "DISTINCT ");
    }
    
    /* Process each return item */
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        if (transform_return_item(ctx, item, i == 0) < 0) {
            return -1;
        }
    }
    
    /* Handle ORDER BY */
    if (ret->order_by && ret->order_by->count > 0) {
        append_sql(ctx, " ORDER BY ");
        /* TODO: Implement ORDER BY transformation */
        CYPHER_DEBUG("ORDER BY not yet implemented");
    }
    
    /* Handle SKIP */
    if (ret->skip) {
        /* TODO: Implement SKIP transformation */
        CYPHER_DEBUG("SKIP not yet implemented");
    }
    
    /* Handle LIMIT */
    if (ret->limit) {
        /* TODO: Implement LIMIT transformation */
        CYPHER_DEBUG("LIMIT not yet implemented");
    }
    
    return 0;
}

/* Transform a single return item */
static int transform_return_item(cypher_transform_context *ctx, cypher_return_item *item, bool first)
{
    if (!first) {
        append_sql(ctx, ", ");
    }
    
    /* Special handling for node identifiers with aliases */
    if (item->alias && item->expr->type == AST_NODE_IDENTIFIER) {
        cypher_identifier *id = (cypher_identifier*)item->expr;
        const char *table_alias = lookup_variable_alias(ctx, id->name);
        if (table_alias) {
            /* For node variables with alias, select the ID and alias it */
            append_sql(ctx, "%s.id AS ", table_alias);
            append_identifier(ctx, item->alias);
            return 0;
        }
    }
    
    /* Transform the expression */
    if (transform_expression(ctx, item->expr) < 0) {
        return -1;
    }
    
    /* Add alias if specified (for non-wildcard expressions) */
    if (item->alias && item->expr->type != AST_NODE_IDENTIFIER) {
        append_sql(ctx, " AS ");
        append_identifier(ctx, item->alias);
    }
    
    return 0;
}

/* Transform an expression */
int transform_expression(cypher_transform_context *ctx, ast_node *expr)
{
    if (!expr) {
        return -1;
    }
    
    CYPHER_DEBUG("Transforming expression type %s", ast_node_type_name(expr->type));
    
    switch (expr->type) {
        case AST_NODE_IDENTIFIER:
            {
                cypher_identifier *id = (cypher_identifier*)expr;
                const char *alias = lookup_variable_alias(ctx, id->name);
                if (alias) {
                    /* This is a node variable - return all columns for now */
                    append_sql(ctx, "%s.*", alias);
                } else {
                    /* Unknown identifier */
                    ctx->has_error = true;
                    char error[256];
                    snprintf(error, sizeof(error), "Unknown variable: %s", id->name);
                    ctx->error_message = strdup(error);
                    return -1;
                }
            }
            break;
            
        case AST_NODE_PROPERTY:
            return transform_property_access(ctx, (cypher_property*)expr);
            
        case AST_NODE_LITERAL:
            {
                cypher_literal *lit = (cypher_literal*)expr;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER:
                        append_sql(ctx, "%d", lit->value.integer);
                        break;
                    case LITERAL_DECIMAL:
                        append_sql(ctx, "%f", lit->value.decimal);
                        break;
                    case LITERAL_STRING:
                        append_string_literal(ctx, lit->value.string);
                        break;
                    case LITERAL_BOOLEAN:
                        append_sql(ctx, "%d", lit->value.boolean ? 1 : 0);
                        break;
                    case LITERAL_NULL:
                        append_sql(ctx, "NULL");
                        break;
                }
            }
            break;
            
        default:
            ctx->has_error = true;
            char error[256];
            snprintf(error, sizeof(error), "Unsupported expression type: %s", 
                    ast_node_type_name(expr->type));
            ctx->error_message = strdup(error);
            return -1;
    }
    
    return 0;
}

/* Transform property access (e.g., n.name) */
int transform_property_access(cypher_transform_context *ctx, cypher_property *prop)
{
    CYPHER_DEBUG("Transforming property access");
    
    /* Get the base expression (should be an identifier) */
    if (prop->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("Complex property access not yet supported");
        return -1;
    }
    
    cypher_identifier *id = (cypher_identifier*)prop->expr;
    const char *alias = lookup_variable_alias(ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in property access: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }
    
    /* Generate property access query using our actual schema */
    /* We need to check multiple property tables based on type */
    append_sql(ctx, "(SELECT COALESCE(");
    append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = ", alias);
    append_string_literal(ctx, prop->property_name);
    append_sql(ctx, "), ");
    append_sql(ctx, "(SELECT CAST(npi.value AS TEXT) FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = ", alias);
    append_string_literal(ctx, prop->property_name);
    append_sql(ctx, "), ");
    append_sql(ctx, "(SELECT CAST(npr.value AS TEXT) FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = ", alias);
    append_string_literal(ctx, prop->property_name);
    append_sql(ctx, "), ");
    append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = ", alias);
    append_string_literal(ctx, prop->property_name);
    append_sql(ctx, ")))");
    
    return 0;
}