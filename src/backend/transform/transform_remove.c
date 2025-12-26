/*
 * REMOVE clause transformation
 * Converts REMOVE patterns into SQL DELETE queries for property/label removal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_remove_item(cypher_transform_context *ctx, cypher_remove_item *item);
static int generate_property_remove(cypher_transform_context *ctx,
                                   const char *variable, const char *property_name);
static int generate_label_remove(cypher_transform_context *ctx,
                                const char *variable, const char *label_name);

/* Transform a REMOVE clause into SQL */
int transform_remove_clause(cypher_transform_context *ctx, cypher_remove *remove)
{
    CYPHER_DEBUG("Transforming REMOVE clause");

    if (!ctx || !remove) {
        return -1;
    }

    /* Mark this as a write query */
    if (ctx->query_type == QUERY_TYPE_UNKNOWN) {
        ctx->query_type = QUERY_TYPE_WRITE;
    } else if (ctx->query_type == QUERY_TYPE_READ) {
        ctx->query_type = QUERY_TYPE_MIXED;
    }

    /* Process each REMOVE item */
    for (int i = 0; i < remove->items->count; i++) {
        cypher_remove_item *item = (cypher_remove_item*)remove->items->items[i];

        if (transform_remove_item(ctx, item) < 0) {
            return -1;
        }

        /* Add separator between REMOVE items if not the last one */
        if (i < remove->items->count - 1) {
            append_sql(ctx, "; ");
        }
    }

    return 0;
}

/* Transform a single REMOVE item (e.g., n.prop or n:Label) */
static int transform_remove_item(cypher_transform_context *ctx, cypher_remove_item *item)
{
    CYPHER_DEBUG("Transforming REMOVE item");

    if (!item || !item->target) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid REMOVE item");
        return -1;
    }

    /* Check if this is a label expression (REMOVE n:Label) */
    if (item->target->type == AST_NODE_LABEL_EXPR) {
        cypher_label_expr *label_expr = (cypher_label_expr*)item->target;

        /* The base expression should be an identifier (the variable) */
        if (label_expr->expr->type != AST_NODE_IDENTIFIER) {
            ctx->has_error = true;
            ctx->error_message = strdup("REMOVE label must be on a variable");
            return -1;
        }

        cypher_identifier *var_id = (cypher_identifier*)label_expr->expr;

        /* Generate the label remove SQL */
        return generate_label_remove(ctx, var_id->name, label_expr->label_name);
    }

    /* Otherwise, it should be a property access expression (n.prop) */
    if (item->target->type != AST_NODE_PROPERTY) {
        ctx->has_error = true;
        ctx->error_message = strdup("REMOVE target must be a property (variable.property) or label (variable:Label)");
        return -1;
    }

    cypher_property *prop = (cypher_property*)item->target;

    /* The base expression should be an identifier (the variable) */
    if (prop->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("REMOVE property must be on a variable");
        return -1;
    }

    cypher_identifier *var_id = (cypher_identifier*)prop->expr;

    /* Generate the property remove SQL */
    return generate_property_remove(ctx, var_id->name, prop->property_name);
}

/* Generate SQL to remove a property */
static int generate_property_remove(cypher_transform_context *ctx,
                                   const char *variable, const char *property_name)
{
    CYPHER_DEBUG("Generating property remove for %s.%s", variable, property_name);

    /* Get the table alias for the variable */
    const char *table_alias = transform_var_get_alias(ctx->var_ctx, variable);
    if (!table_alias) {
        /* Try to get entity alias if unified lookup fails */
        transform_entity *entity = lookup_entity(ctx, variable);
        if (entity) {
            table_alias = entity->table_alias;
        } else {
            ctx->has_error = true;
            ctx->error_message = strdup("Unknown variable in REMOVE clause - variable must be defined in MATCH clause");
            return -1;
        }
    }

    /* Start a new statement if needed */
    if (ctx->sql_size > 0) {
        append_sql(ctx, "; ");
    }

    /* Delete property from all property tables (text, int, real)
     * We need to delete from all tables since we don't know which type the property is */
    append_sql(ctx, "DELETE FROM node_props_text WHERE node_id = %s.id AND property_name = ", table_alias);
    append_string_literal(ctx, property_name);

    append_sql(ctx, "; DELETE FROM node_props_int WHERE node_id = %s.id AND property_name = ", table_alias);
    append_string_literal(ctx, property_name);

    append_sql(ctx, "; DELETE FROM node_props_real WHERE node_id = %s.id AND property_name = ", table_alias);
    append_string_literal(ctx, property_name);

    CYPHER_DEBUG("Generated property remove SQL");
    return 0;
}

/* Generate SQL to remove a label from a node */
static int generate_label_remove(cypher_transform_context *ctx,
                                const char *variable, const char *label_name)
{
    CYPHER_DEBUG("Generating label remove for %s:%s", variable, label_name);

    /* Get the table alias for the variable */
    const char *table_alias = transform_var_get_alias(ctx->var_ctx, variable);
    if (!table_alias) {
        /* Try to get entity alias if unified lookup fails */
        transform_entity *entity = lookup_entity(ctx, variable);
        if (entity) {
            table_alias = entity->table_alias;
        } else {
            ctx->has_error = true;
            ctx->error_message = strdup("Unknown variable in REMOVE label - variable must be defined in MATCH clause");
            return -1;
        }
    }

    /* Start a new statement if needed */
    if (ctx->sql_size > 0) {
        append_sql(ctx, "; ");
    }

    /* Generate DELETE statement to remove the label */
    append_sql(ctx, "DELETE FROM node_labels WHERE node_id = %s.id AND label = ", table_alias);
    append_string_literal(ctx, label_name);

    CYPHER_DEBUG("Generated label remove SQL");
    return 0;
}
