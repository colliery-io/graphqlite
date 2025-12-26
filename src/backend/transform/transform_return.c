/*
 * RETURN clause transformation
 * Converts RETURN items into SQL SELECT projections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_internal.h"
#include "transform/transform_functions.h"
#include "transform/transform_helpers.h"
#include "parser/cypher_debug.h"

/*
 * Pending property JOINs buffer for aggregation optimization.
 * These are accumulated during RETURN item processing and injected
 * into the FROM clause before it's appended back.
 */
static char pending_prop_joins[16384] = "";
static size_t pending_prop_joins_len = 0;

static void reset_pending_prop_joins(void)
{
    pending_prop_joins[0] = '\0';
    pending_prop_joins_len = 0;
}

/* Used by transform_func_aggregate.c for optimized property aggregation */
void add_pending_prop_join(const char *join_sql)
{
    size_t len = strlen(join_sql);
    if (pending_prop_joins_len + len < sizeof(pending_prop_joins) - 1) {
        strcat(pending_prop_joins, join_sql);
        pending_prop_joins_len += len;
    }
}

/* Forward declarations */
static int transform_return_item(cypher_transform_context *ctx, cypher_return_item *item, bool first);

/* Transform a RETURN clause */
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret)
{
    CYPHER_DEBUG("Transforming RETURN clause");

    /* Reset pending property JOINs for this RETURN clause */
    reset_pending_prop_joins();

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
        /* No SELECT * - check if this is a standalone RETURN (no MATCH clause) */
        /* This happens for UNION queries or simple RETURN queries without MATCH */
        bool is_standalone = (ctx->sql_size == 0);

        /* Also check if we're after a UNION keyword - means we're starting a new sub-query */
        if (!is_standalone && ctx->sql_size >= 7) {
            /* Check if buffer ends with " UNION " or " UNION ALL " */
            if (strcmp(ctx->sql_buffer + ctx->sql_size - 7, " UNION ") == 0 ||
                (ctx->sql_size >= 11 && strcmp(ctx->sql_buffer + ctx->sql_size - 11, " UNION ALL ") == 0)) {
                is_standalone = true;
            }
        }

        if (is_standalone) {
            /* Standalone RETURN clause - generate simple SELECT for literals */
            CYPHER_DEBUG("Standalone RETURN clause - generating SELECT");
            append_sql(ctx, "SELECT ");

            /* Add DISTINCT if needed */
            if (ret->distinct) {
                append_sql(ctx, "DISTINCT ");
            }

            /* Process return items */
            for (int i = 0; i < ret->items->count; i++) {
                cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
                if (transform_return_item(ctx, item, i == 0) < 0) {
                    return -1;
                }
            }

            /* Register aliases for ORDER BY to reference */
            if (ret->order_by && ret->order_by->count > 0) {
                for (int i = 0; i < ret->items->count; i++) {
                    cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
                    if (item->alias) {
                        /* Register alias so ORDER BY can reference it */
                        register_projected_variable(ctx, item->alias, NULL, item->alias);
                    }
                }
            }

            /* Handle ORDER BY, LIMIT, SKIP */
            if (ret->order_by && ret->order_by->count > 0) {
                append_sql(ctx, " ORDER BY ");
                for (int i = 0; i < ret->order_by->count; i++) {
                    if (i > 0) {
                        append_sql(ctx, ", ");
                    }
                    cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
                    if (transform_expression(ctx, order_item->expr) < 0) {
                        return -1;
                    }
                    if (order_item->descending) {
                        append_sql(ctx, " DESC");
                    }
                }
            }
            if (ret->limit) {
                append_sql(ctx, " LIMIT ");
                if (transform_expression(ctx, ret->limit) < 0) {
                    return -1;
                }
            }
            if (ret->skip) {
                append_sql(ctx, " OFFSET ");
                if (transform_expression(ctx, ret->skip) < 0) {
                    return -1;
                }
            }

            return 0;
        }

        /* Check if we're after a WITH clause */
        if (ctx->sql_size > 0) {
            /* Check if there's already a SELECT from a CTE (from WITH clause) */
            char *existing_select = strstr(ctx->sql_buffer, "SELECT ");
            if (existing_select) {
                /* WITH already set up the SELECT - verify all RETURN items are simple projected identifiers */
                bool can_use_existing = true;
                for (int i = 0; i < ret->items->count; i++) {
                    cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
                    if (item->expr->type == AST_NODE_IDENTIFIER) {
                        cypher_identifier *id = (cypher_identifier*)item->expr;
                        if (!is_projected_variable(ctx, id->name)) {
                            can_use_existing = false;
                            break;
                        }
                    } else {
                        /* Property access, function calls, etc. need transformation */
                        can_use_existing = false;
                        break;
                    }
                }
                if (can_use_existing) {
                    /* WITH already built the correct SELECT - query is ready */
                    CYPHER_DEBUG("RETURN after WITH: all items are simple projected identifiers, query ready");
                    return 0;
                }

                /* Need to rebuild SELECT for RETURN items that need transformation */
                CYPHER_DEBUG("RETURN after WITH: rebuilding SELECT for complex items");

                /* Find the FROM clause position in existing SELECT */
                char *from_pos = strstr(existing_select, " FROM ");
                if (from_pos) {
                    /* Save the FROM clause and everything after before we modify the buffer */
                    char *from_clause = strdup(from_pos);
                    if (!from_clause) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Memory allocation failed");
                        return -1;
                    }

                    /* Truncate at SELECT and rebuild the column list */
                    ctx->sql_size = existing_select + strlen("SELECT ") - ctx->sql_buffer;
                    ctx->sql_buffer[ctx->sql_size] = '\0';

                    /* Add DISTINCT if needed */
                    if (ret->distinct) {
                        append_sql(ctx, "DISTINCT ");
                    }

                    /* Process return items */
                    for (int i = 0; i < ret->items->count; i++) {
                        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
                        if (transform_return_item(ctx, item, i == 0) < 0) {
                            free(from_clause);
                            return -1;
                        }
                    }

                    /* Append the FROM clause and everything after */
                    append_sql(ctx, "%s", from_clause);
                    free(from_clause);

                    /* Handle ORDER BY, LIMIT, SKIP */
                    if (ret->order_by && ret->order_by->count > 0) {
                        append_sql(ctx, " ORDER BY ");
                        for (int i = 0; i < ret->order_by->count; i++) {
                            if (i > 0) {
                                append_sql(ctx, ", ");
                            }
                            cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
                            if (transform_expression(ctx, order_item->expr) < 0) {
                                return -1;
                            }
                            if (order_item->descending) {
                                append_sql(ctx, " DESC");
                            }
                        }
                    }
                    if (ret->limit) {
                        append_sql(ctx, " LIMIT ");
                        if (transform_expression(ctx, ret->limit) < 0) {
                            return -1;
                        }
                    }
                    if (ret->skip) {
                        append_sql(ctx, " OFFSET ");
                        if (transform_expression(ctx, ret->skip) < 0) {
                            return -1;
                        }
                    }

                    return 0;
                }
            }
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

        /* Process return items (this may add to pending_prop_joins) */
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
            if (transform_return_item(ctx, item, i == 0) < 0) {
                free(temp);
                return -1;
            }
        }

        /*
         * If any property JOINs were accumulated during item processing,
         * inject them into temp before WHERE clause (or at end if no WHERE).
         */
        if (pending_prop_joins_len > 0) {
            char *where_pos = strstr(temp, " WHERE ");
            if (where_pos) {
                /* Insert JOINs before WHERE */
                size_t prefix_len = where_pos - temp;
                size_t suffix_len = strlen(where_pos);
                size_t new_len = strlen(temp) + pending_prop_joins_len + 1;
                char *new_temp = malloc(new_len);
                if (new_temp) {
                    memcpy(new_temp, temp, prefix_len);
                    memcpy(new_temp + prefix_len, pending_prop_joins, pending_prop_joins_len);
                    memcpy(new_temp + prefix_len + pending_prop_joins_len, where_pos, suffix_len + 1);
                    free(temp);
                    temp = new_temp;
                    CYPHER_DEBUG("Injected property JOINs before WHERE: %s", pending_prop_joins);
                }
            } else {
                /* No WHERE, append JOINs at end of FROM section */
                size_t new_len = strlen(temp) + pending_prop_joins_len + 1;
                char *new_temp = malloc(new_len);
                if (new_temp) {
                    strcpy(new_temp, temp);
                    strcat(new_temp, pending_prop_joins);
                    free(temp);
                    temp = new_temp;
                    CYPHER_DEBUG("Appended property JOINs: %s", pending_prop_joins);
                }
            }
            reset_pending_prop_joins();
        }

        /* Append the rest of the query */
        append_sql(ctx, " %s", temp);
        free(temp);

        /* Handle ORDER BY, LIMIT, SKIP for MATCH + RETURN queries */
        if (ret->order_by && ret->order_by->count > 0) {
            /* Register aliases so ORDER BY can reference them */
            for (int i = 0; i < ret->items->count; i++) {
                cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
                if (item->alias) {
                    register_projected_variable(ctx, item->alias, NULL, item->alias);
                }
            }

            append_sql(ctx, " ORDER BY ");
            for (int i = 0; i < ret->order_by->count; i++) {
                if (i > 0) {
                    append_sql(ctx, ", ");
                }
                cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
                if (transform_expression(ctx, order_item->expr) < 0) {
                    return -1;
                }
                if (order_item->descending) {
                    append_sql(ctx, " DESC");
                }
            }
        }
        if (ret->limit) {
            append_sql(ctx, " LIMIT ");
            if (transform_expression(ctx, ret->limit) < 0) {
                return -1;
            }
        } else if (ret->skip) {
            /* SQLite requires LIMIT before OFFSET - use -1 for unlimited */
            append_sql(ctx, " LIMIT -1");
        }
        if (ret->skip) {
            append_sql(ctx, " OFFSET ");
            if (transform_expression(ctx, ret->skip) < 0) {
                return -1;
            }
        }

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
        for (int i = 0; i < ret->order_by->count; i++) {
            if (i > 0) {
                append_sql(ctx, ", ");
            }
            cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
            if (transform_expression(ctx, order_item->expr) < 0) {
                return -1;
            }
            /* Add sort direction */
            if (order_item->descending) {
                append_sql(ctx, " DESC");
            } else {
                append_sql(ctx, " ASC");
            }
        }
    }
    
    /* Handle LIMIT */
    if (ret->limit) {
        append_sql(ctx, " LIMIT ");
        if (transform_expression(ctx, ret->limit) < 0) {
            return -1;
        }
    }
    
    /* Handle SKIP (using SQLite OFFSET) */
    if (ret->skip) {
        append_sql(ctx, " OFFSET ");
        if (transform_expression(ctx, ret->skip) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Transform a single return item */
static int transform_return_item(cypher_transform_context *ctx, cypher_return_item *item, bool first)
{
    if (!first) {
        append_sql(ctx, ", ");
    }

    /* Special handling for identifiers with aliases */
    if (item->alias && item->expr->type == AST_NODE_IDENTIFIER) {
        cypher_identifier *id = (cypher_identifier*)item->expr;
        const char *table_alias = lookup_variable_alias(ctx, id->name);
        if (table_alias) {
            /* For variables with alias, select the ID and alias it */
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
    } else if (!item->alias && item->expr->type == AST_NODE_PROPERTY) {
        /* Auto-generate alias for property expressions (e.g., n.name -> "n.name") */
        cypher_property *prop = (cypher_property*)item->expr;
        if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
            cypher_identifier *id = (cypher_identifier*)prop->expr;
            char auto_alias[256];
            snprintf(auto_alias, sizeof(auto_alias), "%s.%s", id->name, prop->property_name);
            append_sql(ctx, " AS \"%s\"", auto_alias);
        }
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
                    if (is_path_variable(ctx, id->name)) {
                        CYPHER_DEBUG("Processing path variable '%s' in RETURN", id->name);
                        /* This is a path variable - generate JSON with element IDs */
                        path_variable *path_var = get_path_variable(ctx, id->name);
                        if (path_var && path_var->elements) {
                            CYPHER_DEBUG("Found path variable metadata for '%s' with %d elements", id->name, path_var->elements->count);

                            /* Check if this is a variable-length path (shortestPath, etc.) */
                            bool has_varlen = false;
                            const char *varlen_alias = NULL;
                            for (int i = 0; i < path_var->elements->count; i++) {
                                ast_node *element = path_var->elements->items[i];
                                if (element->type == AST_NODE_REL_PATTERN) {
                                    cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                                    if (rel->varlen) {
                                        has_varlen = true;
                                        if (rel->variable) {
                                            varlen_alias = lookup_variable_alias(ctx, rel->variable);
                                        }
                                        break;
                                    }
                                }
                            }

                            if (has_varlen && varlen_alias) {
                                /* For variable-length paths, use the CTE's path_ids column */
                                append_sql(ctx, "'[' || %s.path_ids || ']'", varlen_alias);
                            } else {
                                /* Regular path - build from individual element IDs */
                                append_sql(ctx, "'[");
                                for (int i = 0; i < path_var->elements->count; i++) {
                                    if (i > 0) append_sql(ctx, ",");

                                    ast_node *element = path_var->elements->items[i];
                                    if (element->type == AST_NODE_NODE_PATTERN) {
                                        cypher_node_pattern *node = (cypher_node_pattern*)element;
                                        if (node->variable) {
                                            const char *node_alias = lookup_variable_alias(ctx, node->variable);
                                            if (node_alias) {
                                                append_sql(ctx, "' || %s.id || '", node_alias);
                                            } else {
                                                append_sql(ctx, "null");
                                            }
                                        } else {
                                            append_sql(ctx, "null");
                                        }
                                    } else if (element->type == AST_NODE_REL_PATTERN) {
                                        cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                                        if (rel->variable) {
                                            const char *rel_alias = lookup_variable_alias(ctx, rel->variable);
                                            if (rel_alias) {
                                                append_sql(ctx, "' || %s.id || '", rel_alias);
                                            } else {
                                                append_sql(ctx, "null");
                                            }
                                        } else {
                                            append_sql(ctx, "null");
                                        }
                                    }
                                }
                                append_sql(ctx, "]'");
                            }
                        } else {
                            append_sql(ctx, "'[]'");
                        }
                    } else if (is_projected_variable(ctx, id->name)) {
                        /* This is a projected variable from WITH - alias is the full column reference */
                        append_sql(ctx, "%s", alias);
                    } else if (is_edge_variable(ctx, id->name)) {
                        /* This is an edge variable - return full relationship object */
                        append_sql(ctx, "json_object("
                            "'id', %s.id, "
                            "'type', %s.type, "
                            "'startNodeId', %s.source_id, "
                            "'endNodeId', %s.target_id, "
                            "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = %s.id AND ept.key_id = pk.id), "
                                "(SELECT epi.value FROM edge_props_int epi WHERE epi.edge_id = %s.id AND epi.key_id = pk.id), "
                                "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = %s.id AND epr.key_id = pk.id), "
                                "(SELECT epb.value FROM edge_props_bool epb WHERE epb.edge_id = %s.id AND epb.key_id = pk.id))) "
                            "FROM property_keys pk WHERE "
                                "EXISTS (SELECT 1 FROM edge_props_text WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                "EXISTS (SELECT 1 FROM edge_props_int WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                "EXISTS (SELECT 1 FROM edge_props_real WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                "EXISTS (SELECT 1 FROM edge_props_bool WHERE edge_id = %s.id AND key_id = pk.id)"
                            "), json('{}'))"
                        ")",
                        alias, alias, alias, alias,
                        alias, alias, alias, alias,
                        alias, alias, alias, alias);
                    } else {
                        /* This is a node variable - return full node object */
                        append_sql(ctx, "json_object("
                            "'id', %s.id, "
                            "'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = %s.id), json('[]')), "
                            "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %s.id AND npt.key_id = pk.id), "
                                "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %s.id AND npi.key_id = pk.id), "
                                "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %s.id AND npr.key_id = pk.id), "
                                "(SELECT npb.value FROM node_props_bool npb WHERE npb.node_id = %s.id AND npb.key_id = pk.id))) "
                            "FROM property_keys pk WHERE "
                                "EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %s.id AND key_id = pk.id) OR "
                                "EXISTS (SELECT 1 FROM node_props_int WHERE node_id = %s.id AND key_id = pk.id) OR "
                                "EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %s.id AND key_id = pk.id) OR "
                                "EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %s.id AND key_id = pk.id)"
                            "), json('{}'))"
                        ")",
                        alias, alias,
                        alias, alias, alias, alias,
                        alias, alias, alias, alias);
                    }
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
            
        case AST_NODE_LABEL_EXPR:
            return transform_label_expression(ctx, (cypher_label_expr*)expr);
            
        case AST_NODE_NOT_EXPR:
            return transform_not_expression(ctx, (cypher_not_expr*)expr);

        case AST_NODE_NULL_CHECK:
            return transform_null_check(ctx, (cypher_null_check*)expr);

        case AST_NODE_BINARY_OP:
            return transform_binary_operation(ctx, (cypher_binary_op*)expr);
            
        case AST_NODE_FUNCTION_CALL:
            return transform_function_call(ctx, (cypher_function_call*)expr);
            
        case AST_NODE_EXISTS_EXPR:
            return transform_exists_expression(ctx, (cypher_exists_expr*)expr);

        case AST_NODE_LIST_PREDICATE:
            return transform_list_predicate(ctx, (cypher_list_predicate*)expr);

        case AST_NODE_REDUCE_EXPR:
            return transform_reduce_expr(ctx, (cypher_reduce_expr*)expr);

        case AST_NODE_SUBSCRIPT:
            {
                /* Transform list[index] to json_extract(list, '$[' || index || ']') */
                cypher_subscript *subscript = (cypher_subscript*)expr;
                append_sql(ctx, "json_extract(");
                if (transform_expression(ctx, subscript->expr) < 0) {
                    return -1;
                }
                append_sql(ctx, ", '$[' || (");
                if (transform_expression(ctx, subscript->index) < 0) {
                    return -1;
                }
                append_sql(ctx, ") || ']')");
            }
            break;

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

        case AST_NODE_PARAMETER:
            {
                /* Transform parameter $name to SQLite named parameter :name */
                cypher_parameter *param = (cypher_parameter*)expr;
                if (param->name) {
                    /* Register parameter for tracking */
                    register_parameter(ctx, param->name);
                    append_sql(ctx, ":%s", param->name);
                } else {
                    /* Unnamed parameter - use positional placeholder */
                    append_sql(ctx, "?");
                }
            }
            break;

        case AST_NODE_LIST:
            {
                /* Transform list to JSON array for SQLite */
                cypher_list *list = (cypher_list*)expr;
                append_sql(ctx, "json_array(");
                if (list->items) {
                    for (int i = 0; i < list->items->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        if (transform_expression(ctx, list->items->items[i]) < 0) {
                            return -1;
                        }
                    }
                }
                append_sql(ctx, ")");
            }
            break;

        case AST_NODE_CASE_EXPR:
            {
                /* Transform CASE WHEN ... THEN ... ELSE ... END */
                cypher_case_expr *case_expr = (cypher_case_expr*)expr;

                if (!case_expr->when_clauses || case_expr->when_clauses->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("CASE expression requires at least one WHEN clause");
                    return -1;
                }

                append_sql(ctx, "CASE");

                for (int i = 0; i < case_expr->when_clauses->count; i++) {
                    cypher_when_clause *when = (cypher_when_clause*)case_expr->when_clauses->items[i];

                    append_sql(ctx, " WHEN ");
                    if (transform_expression(ctx, when->condition) < 0) {
                        return -1;
                    }

                    append_sql(ctx, " THEN ");
                    if (transform_expression(ctx, when->result) < 0) {
                        return -1;
                    }
                }

                if (case_expr->else_expr) {
                    append_sql(ctx, " ELSE ");
                    if (transform_expression(ctx, case_expr->else_expr) < 0) {
                        return -1;
                    }
                }

                append_sql(ctx, " END");
            }
            break;

        case AST_NODE_MAP:
            {
                /* Transform map literal to SQLite json_object() */
                cypher_map *map = (cypher_map*)expr;
                append_sql(ctx, "json_object(");
                if (map->pairs) {
                    for (int i = 0; i < map->pairs->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                        /* Key as string */
                        append_sql(ctx, "'%s', ", pair->key);
                        /* Value expression */
                        if (transform_expression(ctx, pair->value) < 0) {
                            return -1;
                        }
                    }
                }
                append_sql(ctx, ")");
            }
            break;

        case AST_NODE_MAP_PROJECTION:
            {
                /* Transform map projection n{.prop1, .prop2} to json_object() */
                cypher_map_projection *proj = (cypher_map_projection*)expr;

                /* Get the base variable alias */
                const char *base_alias = NULL;
                const char *base_name = NULL;
                if (proj->base_expr && proj->base_expr->type == AST_NODE_IDENTIFIER) {
                    cypher_identifier *ident = (cypher_identifier*)proj->base_expr;
                    base_name = ident->name;
                    base_alias = lookup_variable_alias(ctx, ident->name);
                    if (!base_alias) {
                        ctx->has_error = true;
                        char error[256];
                        snprintf(error, sizeof(error), "Unknown variable in map projection: %s", ident->name);
                        ctx->error_message = strdup(error);
                        return -1;
                    }
                }

                bool is_projected = is_projected_variable(ctx, base_name);

                /* Check if we have .* (all properties) - use properties() function approach */
                bool has_all_props = false;
                if (proj->items && proj->items->count == 1) {
                    cypher_map_projection_item *item = (cypher_map_projection_item*)proj->items->items[0];
                    if (item->property && strcmp(item->property, "*") == 0) {
                        has_all_props = true;
                    }
                }

                if (has_all_props && base_alias) {
                    /* Use properties() function approach for n{.*} */
                    append_sql(ctx, "(SELECT json_group_object(pk.key, COALESCE("
                               "npt.value, "
                               "CAST(npi.value AS TEXT), "
                               "CAST(npr.value AS TEXT), "
                               "CASE WHEN npb.value THEN 'true' ELSE 'false' END"
                               ")) FROM property_keys pk "
                               "LEFT JOIN node_props_text npt ON npt.key_id = pk.id AND npt.node_id = %s%s "
                               "LEFT JOIN node_props_int npi ON npi.key_id = pk.id AND npi.node_id = %s%s "
                               "LEFT JOIN node_props_real npr ON npr.key_id = pk.id AND npr.node_id = %s%s "
                               "LEFT JOIN node_props_bool npb ON npb.key_id = pk.id AND npb.node_id = %s%s "
                               "WHERE npt.value IS NOT NULL OR npi.value IS NOT NULL OR npr.value IS NOT NULL OR npb.value IS NOT NULL)",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id");
                } else {
                    append_sql(ctx, "json_object(");
                    if (proj->items) {
                    for (int i = 0; i < proj->items->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        cypher_map_projection_item *item = (cypher_map_projection_item*)proj->items->items[i];

                        /* Output key name */
                        const char *key = item->key ? item->key : item->property;
                        append_sql(ctx, "'%s', ", key);

                        /* Output value */
                        if (item->property && base_alias) {
                            /* Property access using same logic as transform_property_access */
                            append_sql(ctx, "(SELECT COALESCE(");
                            append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT CAST(npi.value AS TEXT) FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT CAST(npr.value AS TEXT) FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, ")))");
                        } else if (item->expr) {
                            /* Computed expression */
                            if (transform_expression(ctx, item->expr) < 0) {
                                return -1;
                            }
                        }
                    }
                    }
                    append_sql(ctx, ")");
                }
            }
            break;

        case AST_NODE_LIST_COMPREHENSION:
            {
                /* Transform list comprehension: [x IN list WHERE cond | transform]
                 * to: (SELECT json_group_array(transform_expr) FROM json_each(list_expr) WHERE cond_expr)
                 */
                cypher_list_comprehension *comp = (cypher_list_comprehension*)expr;

                if (!comp->list_expr) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("List comprehension requires list expression");
                    return -1;
                }

                /* Store the comprehension variable for use in nested expressions */
                const char *comp_var = comp->variable;

                /* Save the old alias if this variable name already exists */
                const char *old_alias = lookup_variable_alias(ctx, comp_var);
                char *saved_alias = old_alias ? strdup(old_alias) : NULL;

                /* Register the comprehension variable to map to json_each.value */
                register_variable(ctx, comp_var, "json_each.value");
                /* Update the type to projected so it's treated as a direct value */
                for (int i = 0; i < ctx->variable_count; i++) {
                    if (strcmp(ctx->variables[i].name, comp_var) == 0) {
                        ctx->variables[i].type = VAR_TYPE_PROJECTED;
                        break;
                    }
                }

                /* Build the subquery */
                append_sql(ctx, "(SELECT json_group_array(");

                /* The result expression - either the transform or the element itself */
                if (comp->transform_expr) {
                    if (transform_expression(ctx, comp->transform_expr) < 0) {
                        if (saved_alias) free(saved_alias);
                        return -1;
                    }
                } else {
                    /* Just return the element */
                    append_sql(ctx, "json_each.value");
                }

                append_sql(ctx, ") FROM json_each(");

                /* The source list - transform BEFORE adding comprehension variable binding */
                if (transform_expression(ctx, comp->list_expr) < 0) {
                    if (saved_alias) free(saved_alias);
                    return -1;
                }

                append_sql(ctx, ")");

                /* Optional WHERE filter */
                if (comp->where_expr) {
                    append_sql(ctx, " WHERE ");
                    if (transform_expression(ctx, comp->where_expr) < 0) {
                        if (saved_alias) free(saved_alias);
                        return -1;
                    }
                }

                append_sql(ctx, ")");

                /* Restore the old alias if there was one, otherwise remove the variable */
                if (saved_alias) {
                    register_variable(ctx, comp_var, saved_alias);
                    free(saved_alias);
                }
                /* If there was no old alias, leave the variable as is - it won't conflict
                 * with anything since list comprehension creates a new scope */
            }
            break;

        case AST_NODE_PATTERN_COMPREHENSION:
            {
                /* Transform pattern comprehension: [(n)-[r]->(m) WHERE cond | expr]
                 * to: (SELECT json_group_array(expr) FROM nodes n, edges r, nodes m
                 *      WHERE r.source_id = n.id AND r.target_id = m.id [AND cond])
                 */
                cypher_pattern_comprehension *comp = (cypher_pattern_comprehension*)expr;

                if (!comp->pattern || comp->pattern->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension requires a pattern");
                    return -1;
                }

                if (!comp->collect_expr) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension requires a collect expression");
                    return -1;
                }

                /* Get the first (and only) path from the pattern */
                ast_node *pattern = comp->pattern->items[0];
                if (pattern->type != AST_NODE_PATH) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension requires a path pattern");
                    return -1;
                }

                cypher_path *path = (cypher_path*)pattern;
                if (!path->elements || path->elements->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension path is empty");
                    return -1;
                }

                /* Save current variable state to restore later */
                int saved_var_count = ctx->variable_count;

                /* Track node aliases and whether they're external */
                char node_aliases[10][32];
                char node_vars[10][32];  /* Variable names from the pattern */
                int node_count = 0;

                /* First pass: collect nodes and generate FROM clause */
                append_sql(ctx, "(SELECT json_group_array(");

                /* We'll add the collect expression after setting up variables */
                size_t collect_expr_pos = ctx->sql_size;

                /* Placeholder - we'll come back to fill in the collect expression */
                append_sql(ctx, "/*COLLECT*/");

                append_sql(ctx, ") FROM ");

                bool first_table = true;

                /* Process each element in the path */
                for (int i = 0; i < path->elements->count; i++) {
                    ast_node *element = path->elements->items[i];

                    if (element->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *node = (cypher_node_pattern*)element;

                        /* Check if this node variable exists in outer context */
                        const char *outer_alias = NULL;
                        if (node->variable) {
                            outer_alias = lookup_variable_alias(ctx, node->variable);
                        }

                        if (outer_alias) {
                            /* Use alias from outer query - don't add to FROM */
                            strncpy(node_aliases[node_count], outer_alias,
                                   sizeof(node_aliases[node_count]) - 1);
                            node_aliases[node_count][sizeof(node_aliases[node_count]) - 1] = '\0';
                        } else {
                            /* Generate new alias and add to FROM */
                            if (!first_table) {
                                append_sql(ctx, ", ");
                            }
                            snprintf(node_aliases[node_count], sizeof(node_aliases[node_count]),
                                    "_pc_n%d", node_count);
                            append_sql(ctx, "nodes AS %s", node_aliases[node_count]);
                            first_table = false;
                        }

                        /* Store variable name for later registration */
                        if (node->variable) {
                            strncpy(node_vars[node_count], node->variable,
                                   sizeof(node_vars[node_count]) - 1);
                            node_vars[node_count][sizeof(node_vars[node_count]) - 1] = '\0';
                        } else {
                            node_vars[node_count][0] = '\0';
                        }
                        node_count++;

                    } else if (element->type == AST_NODE_REL_PATTERN && i > 0) {
                        /* Relationship pattern: -[variable:TYPE]-> */
                        if (!first_table) {
                            append_sql(ctx, ", ");
                        }
                        append_sql(ctx, "edges AS _pc_e%d", i/2);
                        first_table = false;
                    }
                }

                /* Register pattern variables for use in expressions */
                for (int i = 0; i < node_count; i++) {
                    if (node_vars[i][0] != '\0') {
                        register_variable(ctx, node_vars[i], node_aliases[i]);
                    }
                }

                /* Add WHERE clause for joins and constraints */
                append_sql(ctx, " WHERE ");

                bool first_condition = true;
                int rel_index = 0;

                /* Generate join conditions between nodes and relationships */
                for (int i = 0; i < path->elements->count; i++) {
                    ast_node *element = path->elements->items[i];

                    if (element->type == AST_NODE_REL_PATTERN && i > 0 && i < path->elements->count - 1) {
                        cypher_rel_pattern *rel = (cypher_rel_pattern*)element;

                        if (!first_condition) {
                            append_sql(ctx, " AND ");
                        }

                        /* Join source node with relationship */
                        int source_node = i / 2;
                        int target_node = source_node + 1;

                        /* Handle direction */
                        if (rel->left_arrow) {
                            /* <-[r]- means target->source */
                            append_sql(ctx, "_pc_e%d.target_id = %s.id AND _pc_e%d.source_id = %s.id",
                                      rel_index, node_aliases[source_node],
                                      rel_index, node_aliases[target_node]);
                        } else {
                            /* -[r]-> or -[r]- means source->target */
                            append_sql(ctx, "_pc_e%d.source_id = %s.id AND _pc_e%d.target_id = %s.id",
                                      rel_index, node_aliases[source_node],
                                      rel_index, node_aliases[target_node]);
                        }

                        /* Add relationship type constraint if specified */
                        if (rel->type) {
                            append_sql(ctx, " AND _pc_e%d.type = ", rel_index);
                            append_string_literal(ctx, rel->type);
                        }

                        rel_index++;
                        first_condition = false;

                    } else if (element->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *node = (cypher_node_pattern*)element;

                        /* Add label constraints if specified - one condition per label */
                        if (has_labels(node)) {
                            for (int j = 0; j < node->labels->count; j++) {
                                const char *label = get_label_string(node->labels->items[j]);
                                if (label) {
                                    if (!first_condition) {
                                        append_sql(ctx, " AND ");
                                    }

                                    int current_node = (i == 0) ? 0 : i / 2;
                                    append_sql(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.id AND label = ",
                                              node_aliases[current_node]);
                                    append_string_literal(ctx, label);
                                    append_sql(ctx, ")");
                                    first_condition = false;
                                }
                            }
                        }
                    }
                }

                /* If there were no constraints (e.g., just a node pattern), add TRUE */
                if (first_condition) {
                    append_sql(ctx, "1=1");
                }

                /* Add WHERE clause from pattern comprehension */
                if (comp->where_expr) {
                    append_sql(ctx, " AND (");
                    if (transform_expression(ctx, comp->where_expr) < 0) {
                        return -1;
                    }
                    append_sql(ctx, ")");
                }

                append_sql(ctx, ")");

                /* Now we need to go back and fill in the collect expression */
                /* Create a new buffer for the collect expression */
                char collect_sql[4096];

                /* Save current buffer state */
                char *temp_buffer = ctx->sql_buffer;
                size_t temp_size = ctx->sql_size;
                size_t temp_capacity = ctx->sql_capacity;

                /* Switch to collect buffer */
                ctx->sql_buffer = collect_sql;
                ctx->sql_size = 0;
                ctx->sql_capacity = sizeof(collect_sql);

                /* Transform the collect expression */
                if (transform_expression(ctx, comp->collect_expr) < 0) {
                    ctx->sql_buffer = temp_buffer;
                    ctx->sql_size = temp_size;
                    ctx->sql_capacity = temp_capacity;
                    return -1;
                }

                /* Null terminate */
                collect_sql[ctx->sql_size] = '\0';

                /* Restore original buffer */
                ctx->sql_buffer = temp_buffer;
                ctx->sql_size = temp_size;
                ctx->sql_capacity = temp_capacity;

                /* Now replace the COLLECT placeholder with actual expression */
                char *placeholder = strstr(ctx->sql_buffer + collect_expr_pos, "/*COLLECT*/");
                if (placeholder) {
                    size_t collect_expr_len = strlen(collect_sql);
                    size_t placeholder_len = strlen("/*COLLECT*/");
                    size_t after_len = strlen(placeholder + placeholder_len);

                    /* Calculate required size after replacement */
                    size_t new_size = ctx->sql_size - placeholder_len + collect_expr_len;

                    /* Grow buffer if needed */
                    if (new_size >= ctx->sql_capacity) {
                        size_t new_capacity = ctx->sql_capacity * 2;
                        while (new_capacity <= new_size) {
                            new_capacity *= 2;
                        }

                        /* Calculate placeholder offset before realloc */
                        size_t placeholder_offset = placeholder - ctx->sql_buffer;

                        char *new_buffer = realloc(ctx->sql_buffer, new_capacity);
                        if (!new_buffer) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Out of memory expanding buffer for pattern comprehension");
                            return -1;
                        }

                        ctx->sql_buffer = new_buffer;
                        ctx->sql_capacity = new_capacity;

                        /* Update placeholder pointer after realloc */
                        placeholder = ctx->sql_buffer + placeholder_offset;
                    }

                    /* Now perform the replacement */
                    /* Shift content after placeholder */
                    memmove(placeholder + collect_expr_len,
                           placeholder + placeholder_len,
                           after_len + 1);
                    /* Copy collect expression */
                    memcpy(placeholder, collect_sql, collect_expr_len);
                    /* Update size */
                    ctx->sql_size = new_size;
                }

                /* Restore variable count (remove pattern-local variables) */
                ctx->variable_count = saved_var_count;
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
