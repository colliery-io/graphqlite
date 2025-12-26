/*
 * MATCH clause transformation
 * Converts MATCH patterns into SQL SELECT queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern, bool optional);
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional);
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node,
                                     int rel_index, bool optional, path_type ptype);

/* Transform a MATCH clause into SQL */
int transform_match_clause(cypher_transform_context *ctx, cypher_match *match)
{
    CYPHER_DEBUG("Transforming %s MATCH clause", match->optional ? "OPTIONAL" : "regular");
    
    if (!ctx || !match) {
        return -1;
    }
    
    /* Mark this as a read query */
    if (ctx->query_type == QUERY_TYPE_UNKNOWN) {
        ctx->query_type = QUERY_TYPE_READ;
    } else if (ctx->query_type == QUERY_TYPE_WRITE) {
        ctx->query_type = QUERY_TYPE_MIXED;
    }
    
    /* SQL builder mode is now determined at query level */

    /* Start SELECT clause if this is the first clause and not using builder */
    /* Also start a new SELECT if we're after a UNION keyword */
    bool needs_select = (ctx->sql_size == 0);
    if (!needs_select && ctx->sql_size >= 7) {
        /* Check if buffer ends with " UNION " or " UNION ALL " */
        if (strcmp(ctx->sql_buffer + ctx->sql_size - 7, " UNION ") == 0 ||
            (ctx->sql_size >= 11 && strcmp(ctx->sql_buffer + ctx->sql_size - 11, " UNION ALL ") == 0)) {
            needs_select = true;
        }
    }

    if (!ctx->sql_builder.using_builder && needs_select) {
        append_sql(ctx, "SELECT ");
        /* We'll fill in the column list later in RETURN */
        append_sql(ctx, "* ");
    }
    
    /* Process each pattern in the MATCH - this only adds table joins */
    for (int i = 0; i < match->pattern->count; i++) {
        ast_node *pattern = match->pattern->items[i];
        
        if (pattern->type != AST_NODE_PATH) {
            ctx->has_error = true;
            ctx->error_message = strdup("Invalid pattern type in MATCH");
            return -1;
        }
        
        if (transform_match_pattern(ctx, pattern, match->optional) < 0) {
            return -1;
        }
    }
    
    /* Now add WHERE constraints for all patterns */
    /* Determine constraint logic based on SQL builder mode */
    bool first_constraint;
    if (ctx->sql_builder.using_builder) {
        /* In SQL builder mode, check if builder has WHERE clauses */
        first_constraint = (ctx->sql_builder.where_size == 0);
    } else {
        /* In traditional mode, check SQL buffer for WHERE clause */
        bool has_where_clause = (strstr(ctx->sql_buffer, " WHERE ") != NULL);
        first_constraint = !has_where_clause;
    }
    
    /* For OPTIONAL MATCH, skip pattern constraint generation (constraints are in JOIN ON clauses) */
    if (match->optional) {
        goto handle_where_clause;
    }
    for (int i = 0; i < match->pattern->count; i++) {
        ast_node *pattern = match->pattern->items[i];
        cypher_path *path = (cypher_path*)pattern;
        
        /* Add constraints for each node in this pattern */
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *element = path->elements->items[j];
            
            if (element->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *node = (cypher_node_pattern*)element;
                
                /* Use entity system for node variables (AGE-style) */
                const char *alias;
                if (node->variable) {
                    transform_entity *entity = lookup_entity(ctx, node->variable);
                    if (!entity) {
                        /* New entity - add it */
                        if (add_entity(ctx, node->variable, ENTITY_TYPE_VERTEX, true) < 0) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to add node entity");
                            return -1;
                        }
                        entity = lookup_entity(ctx, node->variable);
                    }
                    alias = entity->table_alias;
                    /* Also register in legacy system for compatibility */
                    register_node_variable(ctx, node->variable, alias);
                    /* Register in unified system */
                    const char *label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                    transform_var_register_node(ctx->var_ctx, node->variable, alias, label);
                } else {
                    /* Anonymous node - use legacy approach for now */
                    char temp_alias[32];
                    snprintf(temp_alias, sizeof(temp_alias), "n_%d", j);
                    alias = temp_alias;
                }
                
                /* Labels are now handled via JOIN in generate_node_match - skip EXISTS */

                /* Add property VALUE constraints (the JOINs are in generate_node_match) */
                if (node->properties && node->properties->type == AST_NODE_MAP) {
                    cypher_map *map = (cypher_map*)node->properties;
                    if (map->pairs) {
                        for (int k = 0; k < map->pairs->count; k++) {
                            cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[k];
                            /* Skip if key is NULL (already handled in JOIN) */
                            if (!pair->key) continue;
                            if (pair->value && pair->value->type == AST_NODE_LITERAL) {
                                if (first_constraint) {
                                    append_sql(ctx, " WHERE ");
                                    first_constraint = false;
                                } else {
                                    append_sql(ctx, " AND ");
                                }

                                cypher_literal *lit = (cypher_literal*)pair->value;
                                /* Property value constraint - _prop_<alias> was added in generate_node_match */
                                switch (lit->literal_type) {
                                    case LITERAL_STRING:
                                        append_sql(ctx, "_prop_%s.value = ", alias);
                                        append_string_literal(ctx, lit->value.string);
                                        break;
                                    case LITERAL_INTEGER:
                                        append_sql(ctx, "_prop_%s.value = %d", alias, lit->value.integer);
                                        break;
                                    case LITERAL_DECIMAL:
                                        append_sql(ctx, "_prop_%s.value = %f", alias, lit->value.decimal);
                                        break;
                                    case LITERAL_BOOLEAN:
                                        append_sql(ctx, "_prop_%s.value = %d", alias, lit->value.boolean ? 1 : 0);
                                        break;
                                    case LITERAL_NULL:
                                        /* NULL check - property should not exist */
                                        append_sql(ctx, "_prop_%s.node_id IS NULL", alias);
                                        break;
                                }
                            }
                        }
                    }
                }
            } else if (element->type == AST_NODE_REL_PATTERN) {
                /* Handle relationship patterns - need surrounding nodes */
                if (j == 0 || j + 1 >= path->elements->count) {
                    continue; /* Skip invalid relationship positions */
                }
                
                ast_node *prev_element = path->elements->items[j - 1];
                ast_node *next_element = path->elements->items[j + 1];
                
                if (prev_element->type != AST_NODE_NODE_PATTERN || next_element->type != AST_NODE_NODE_PATTERN) {
                    continue; /* Skip if not properly connected to nodes */
                }
                
                cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                cypher_node_pattern *source_node = (cypher_node_pattern*)prev_element;
                cypher_node_pattern *target_node = (cypher_node_pattern*)next_element;

                /* Skip variable-length relationships - they're handled in generate_relationship_match */
                if (rel->varlen) {
                    continue;
                }

                /* Get aliases using entity system (AGE-style) */
                const char *source_alias, *target_alias, *edge_alias;
                
                /* Source node alias */
                if (source_node->variable) {
                    transform_entity *entity = lookup_entity(ctx, source_node->variable);
                    if (!entity) {
                        /* This should have been added already, but add if missing */
                        if (add_entity(ctx, source_node->variable, ENTITY_TYPE_VERTEX, true) < 0) {
                            continue;
                        }
                        entity = lookup_entity(ctx, source_node->variable);
                    }
                    source_alias = entity->table_alias;
                } else {
                    static char temp_source[32];
                    snprintf(temp_source, sizeof(temp_source), "n_%d", j - 1);
                    source_alias = temp_source;
                }
                
                /* Target node alias */
                if (target_node->variable) {
                    transform_entity *entity = lookup_entity(ctx, target_node->variable);
                    if (!entity) {
                        /* This should have been added already, but add if missing */
                        if (add_entity(ctx, target_node->variable, ENTITY_TYPE_VERTEX, true) < 0) {
                            continue;
                        }
                        entity = lookup_entity(ctx, target_node->variable);
                    }
                    target_alias = entity->table_alias;
                } else {
                    static char temp_target[32];
                    snprintf(temp_target, sizeof(temp_target), "n_%d", j + 1);
                    target_alias = temp_target;
                }
                
                /* Edge alias */
                if (rel->variable) {
                    transform_entity *entity = lookup_entity(ctx, rel->variable);
                    if (!entity) {
                        /* New relationship entity */
                        if (add_entity(ctx, rel->variable, ENTITY_TYPE_EDGE, true) < 0) {
                            continue;
                        }
                        entity = lookup_entity(ctx, rel->variable);
                    }
                    edge_alias = entity->table_alias;
                    /* Register in legacy system for compatibility */
                    register_edge_variable(ctx, rel->variable, edge_alias);
                    /* Register in unified system */
                    transform_var_register_edge(ctx->var_ctx, rel->variable, edge_alias, rel->type);
                } else {
                    /* This shouldn't happen with AGE pattern - anonymous rels get names assigned */
                    ctx->has_error = true;
                    ctx->error_message = strdup("Internal error: anonymous relationship without assigned name");
                    return -1;
                }
                
                /* Add relationship direction constraints */
                if (first_constraint) {
                    append_sql(ctx, " WHERE ");
                    first_constraint = false;
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
                    /* Single type (legacy support) */
                    append_sql(ctx, " AND %s.type = ", edge_alias);
                    append_string_literal(ctx, rel->type);
                } else if (rel->types && rel->types->count > 0) {
                    /* Multiple types - generate OR conditions */
                    append_sql(ctx, " AND (");
                    for (int t = 0; t < rel->types->count; t++) {
                        if (t > 0) {
                            append_sql(ctx, " OR ");
                        }
                        /* Type names are stored as string literals in the list */
                        cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                        append_sql(ctx, "%s.type = ", edge_alias);
                        append_string_literal(ctx, type_lit->value.string);
                    }
                    append_sql(ctx, ")");
                }
            }
        }
    }
    
handle_where_clause:
    /* Handle WHERE clause if present */
    if (match->where) {
        if (ctx->sql_builder.using_builder) {
            /* In SQL builder mode, capture WHERE expression to builder's where_clauses */
            /* Save current sql_buffer state */
            char *saved_buffer = NULL;
            size_t saved_size = ctx->sql_size;
            if (saved_size > 0) {
                saved_buffer = strdup(ctx->sql_buffer);
                if (!saved_buffer) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Memory allocation failed");
                    return -1;
                }
            }

            /* Clear sql_buffer temporarily */
            ctx->sql_size = 0;
            if (ctx->sql_buffer) {
                ctx->sql_buffer[0] = '\0';
            }

            /* Transform the WHERE expression - appends to sql_buffer */
            if (transform_expression(ctx, match->where) < 0) {
                free(saved_buffer);
                return -1;
            }

            /* Move the expression to builder's where_clauses */
            if (ctx->sql_size > 0) {
                /* Add AND if there's already content in where_clauses */
                if (ctx->sql_builder.where_size > 0) {
                    append_where_clause(ctx, " AND ");
                }
                append_where_clause(ctx, "%s", ctx->sql_buffer);
            }

            /* Restore sql_buffer */
            ctx->sql_size = saved_size;
            if (saved_buffer) {
                strcpy(ctx->sql_buffer, saved_buffer);
                free(saved_buffer);
            } else if (ctx->sql_buffer) {
                ctx->sql_buffer[0] = '\0';
            }
        } else {
            /* Traditional mode - append directly to SQL buffer */
            bool needs_where = true;
            if (match->optional) {
                needs_where = (strstr(ctx->sql_buffer, " WHERE ") == NULL);
            } else {
                needs_where = first_constraint;
            }

            if (needs_where) {
                append_sql(ctx, " WHERE ");
            } else {
                append_sql(ctx, " AND ");
            }

            if (transform_expression(ctx, match->where) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

/* Transform a single pattern (path) */
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern, bool optional)
{
    cypher_path *path = (cypher_path*)pattern;
    
    CYPHER_DEBUG("Transforming %s path with %d elements", optional ? "OPTIONAL" : "regular", path->elements->count);
    
    /* If path has a variable name, register it as a path variable */
    if (path->var_name) {
        CYPHER_DEBUG("Registering path variable: %s with %d elements", path->var_name, path->elements->count);
        if (register_path_variable(ctx, path->var_name, path) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to register path variable");
            return -1;
        }
        CYPHER_DEBUG("Successfully registered path variable: %s", path->var_name);
    } else {
        CYPHER_DEBUG("Path has no variable name - skipping registration");
    }
    
    /* For now, handle simple node patterns */
    /* TODO: Handle relationship patterns */

    for (int i = 0; i < path->elements->count; i++) {
        ast_node *element = path->elements->items[i];
        
        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            
            /* Use entity system (AGE-style) */
            const char *alias;
            bool need_from_clause = false;
            
            if (node->variable) {
                /* Check if entity already exists */
                transform_entity *entity = lookup_entity(ctx, node->variable);
                if (entity && entity->is_current_clause) {
                    /* Entity exists and is from current clause, reuse alias but check if we need FROM clause */
                    alias = entity->table_alias;
                    /* Check if this alias is already in the FROM clause (check both sql_buffer and sql_builder) */
                    bool alias_in_sql = strstr(ctx->sql_buffer, entity->table_alias) != NULL;
                    bool alias_in_builder_from = ctx->sql_builder.from_clause && strstr(ctx->sql_builder.from_clause, entity->table_alias) != NULL;
                    bool alias_in_builder_joins = ctx->sql_builder.join_clauses && strstr(ctx->sql_builder.join_clauses, entity->table_alias) != NULL;
                    if (!alias_in_sql && !alias_in_builder_from && !alias_in_builder_joins) {
                        need_from_clause = true;
                    }
                } else if (entity && !entity->is_current_clause) {
                    /* Entity from previous clause - reuse but may need FROM clause */
                    alias = entity->table_alias;
                    entity->is_current_clause = true; /* Mark as used in current clause */
                    /* Check if this alias is already in the FROM clause (check both sql_buffer and sql_builder) */
                    bool alias_in_sql = strstr(ctx->sql_buffer, entity->table_alias) != NULL;
                    bool alias_in_builder_from = ctx->sql_builder.from_clause && strstr(ctx->sql_builder.from_clause, entity->table_alias) != NULL;
                    bool alias_in_builder_joins = ctx->sql_builder.join_clauses && strstr(ctx->sql_builder.join_clauses, entity->table_alias) != NULL;
                    if (!alias_in_sql && !alias_in_builder_from && !alias_in_builder_joins) {
                        need_from_clause = true;
                    }
                } else {
                    /* New entity, add it */
                    if (add_entity(ctx, node->variable, ENTITY_TYPE_VERTEX, true) < 0) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to add node entity in pattern");
                        return -1;
                    }
                    entity = lookup_entity(ctx, node->variable);
                    alias = entity->table_alias;
                    need_from_clause = true;
                    /* Register in legacy system for compatibility */
                    register_node_variable(ctx, node->variable, alias);
                    /* Register in unified system */
                    const char *label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                    transform_var_register_node(ctx->var_ctx, node->variable, alias, label);
                }
            } else {
                /* Anonymous node - use generated alias */
                static char temp_alias[32];
                snprintf(temp_alias, sizeof(temp_alias), "n_%d", i);
                alias = temp_alias;
                need_from_clause = true;
            }
            
            /* Generate SQL for this node if needed */
            if (need_from_clause) {
                if (generate_node_match(ctx, node, alias, optional) < 0) {
                    return -1;
                }
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
            
            /* AGE pattern: Assign default name to anonymous relationships */
            if (!rel->variable) {
                char *default_name = get_next_default_alias(ctx);
                rel->variable = default_name;
                /* Note: This modifies the AST, ensuring consistent naming across passes */
            }
            
            /* Generate relationship match SQL */
            if (generate_relationship_match(ctx, rel, source_node, target_node, i, optional, path->type) < 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

/* Generate SQL for matching a node pattern
 *
 * Optimized approach: Use explicit JOINs for labels and properties instead of
 * EXISTS subqueries. This allows SQLite's optimizer to choose efficient join order.
 *
 * For a node (a:Label {prop: value}), we generate:
 *   JOIN node_labels nl_a ON nl_a.node_id = a.id AND nl_a.label = 'Label'
 *   JOIN node_props_int npi_a ON npi_a.node_id = a.id
 *   JOIN property_keys pk_a ON pk_a.id = npi_a.key_id AND pk_a.key = 'prop'
 *   WHERE npi_a.value = value
 */
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional)
{
    const char *first_label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
    CYPHER_DEBUG("Generating %s match for node %s (labels: %s, count: %d)",
                 optional ? "OPTIONAL" : "regular",
                 node->variable ? node->variable : "<anonymous>",
                 first_label ? first_label : "<no label>",
                 node->labels ? node->labels->count : 0);

    /* Check if there's already a FROM clause in the current query.
     * For UNION queries, we need to check only after the most recent UNION keyword. */
    bool has_from = false;
    char *from_pos = strstr(ctx->sql_buffer, "FROM");
    if (from_pos) {
        /* Check if there's a UNION after this FROM - if so, FROM is from previous query */
        char *union_pos = strstr(from_pos, " UNION ");
        if (!union_pos) {
            union_pos = strstr(from_pos, " UNION\n");  /* Edge case */
        }
        if (!union_pos) {
            /* No UNION after FROM, so FROM is in current query */
            has_from = true;
        } else {
            /* There's a UNION after FROM - check if there's another FROM after UNION */
            char *from_after_union = strstr(union_pos, "FROM");
            has_from = (from_after_union != NULL);
        }
    }
    const char *join_type = optional ? " LEFT JOIN " : " JOIN ";

    /* Check if node has properties - if so, start from property table for better selectivity */
    bool has_properties = (node->properties && node->properties->type == AST_NODE_MAP);
    cypher_map *prop_map = has_properties ? (cypher_map*)node->properties : NULL;
    bool has_prop_pairs = has_properties && prop_map->pairs && prop_map->pairs->count > 0;

    if (!has_from) {
        /* First table in query - use FROM */
        if (has_prop_pairs) {
            /* Start with property table for better selectivity */
            cypher_map_pair *first_pair = (cypher_map_pair*)prop_map->pairs->items[0];
            if (first_pair->key && first_pair->value && first_pair->value->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)first_pair->value;
                const char *prop_table = NULL;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: prop_table = "node_props_int"; break;
                    case LITERAL_STRING:  prop_table = "node_props_text"; break;
                    case LITERAL_DECIMAL: prop_table = "node_props_real"; break;
                    case LITERAL_BOOLEAN: prop_table = "node_props_bool"; break;
                    default: break;
                }
                if (prop_table) {
                    /* FROM property_table JOIN property_keys JOIN nodes
                     * Include value filter in JOIN for maximum selectivity */
                    append_sql(ctx, "FROM %s AS _prop_%s", prop_table, alias);
                    append_sql(ctx, " JOIN property_keys AS _pk_%s ON _pk_%s.id = _prop_%s.key_id AND _pk_%s.key = ",
                               alias, alias, alias, alias);
                    append_string_literal(ctx, first_pair->key);
                    /* Add value filter to property table constraint */
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            append_sql(ctx, " AND _prop_%s.value = %d", alias, lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            append_sql(ctx, " AND _prop_%s.value = ", alias);
                            append_string_literal(ctx, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            append_sql(ctx, " AND _prop_%s.value = %f", alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            append_sql(ctx, " AND _prop_%s.value = %d", alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    append_sql(ctx, " JOIN nodes AS %s ON %s.id = _prop_%s.node_id", alias, alias, alias);

                    /* Mark first property as handled */
                    first_pair->key = NULL;  /* Signal to skip in WHERE clause */
                } else {
                    append_sql(ctx, "FROM nodes AS %s", alias);
                }
            } else {
                append_sql(ctx, "FROM nodes AS %s", alias);
            }
        } else {
            append_sql(ctx, "FROM nodes AS %s", alias);
        }
    } else {
        /* Subsequent tables - use JOIN */
        if (has_prop_pairs) {
            /* Join via property table for better selectivity */
            cypher_map_pair *first_pair = (cypher_map_pair*)prop_map->pairs->items[0];
            if (first_pair->key && first_pair->value && first_pair->value->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)first_pair->value;
                const char *prop_table = NULL;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: prop_table = "node_props_int"; break;
                    case LITERAL_STRING:  prop_table = "node_props_text"; break;
                    case LITERAL_DECIMAL: prop_table = "node_props_real"; break;
                    case LITERAL_BOOLEAN: prop_table = "node_props_bool"; break;
                    default: break;
                }
                if (prop_table) {
                    append_sql(ctx, "%s%s AS _prop_%s ON 1=1", join_type, prop_table, alias);
                    append_sql(ctx, " JOIN property_keys AS _pk_%s ON _pk_%s.id = _prop_%s.key_id AND _pk_%s.key = ",
                               alias, alias, alias, alias);
                    append_string_literal(ctx, first_pair->key);
                    /* Add value filter */
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            append_sql(ctx, " AND _prop_%s.value = %d", alias, lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            append_sql(ctx, " AND _prop_%s.value = ", alias);
                            append_string_literal(ctx, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            append_sql(ctx, " AND _prop_%s.value = %f", alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            append_sql(ctx, " AND _prop_%s.value = %d", alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    append_sql(ctx, " JOIN nodes AS %s ON %s.id = _prop_%s.node_id", alias, alias, alias);
                    first_pair->key = NULL;
                } else {
                    append_sql(ctx, "%snodes AS %s ON 1=1", join_type, alias);
                }
            } else {
                append_sql(ctx, "%snodes AS %s ON 1=1", join_type, alias);
            }
        } else {
            if (optional) {
                append_sql(ctx, " LEFT JOIN nodes AS %s ON 1=1", alias);
            } else {
                append_sql(ctx, ", nodes AS %s", alias);
            }
        }
    }

    /* Add label JOINs if specified - one JOIN per label for multi-label support */
    if (has_labels(node)) {
        for (int i = 0; i < node->labels->count; i++) {
            const char *label = get_label_string(node->labels->items[i]);
            if (label) {
                append_sql(ctx, " JOIN node_labels AS _nl_%s_%d ON _nl_%s_%d.node_id = %s.id AND _nl_%s_%d.label = ",
                           alias, i, alias, i, alias, alias, i);
                append_string_literal(ctx, label);
            }
        }
    }

    return 0;
}

/* Generate SQL for matching a relationship pattern */
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node,
                                     int rel_index, bool optional, path_type ptype)
{
    CYPHER_DEBUG("Generating %s match for relationship %s between nodes (varlen=%s)",
                 optional ? "OPTIONAL" : "regular",
                 rel->type ? rel->type : "<no type>",
                 rel->varlen ? "yes" : "no");

    /* Get aliases using entity system (AGE-style) */
    const char *source_alias, *target_alias, *edge_alias;
    
    /* Source node */
    if (source_node->variable) {
        transform_entity *entity = lookup_entity(ctx, source_node->variable);
        if (!entity) {
            /* Add missing entity */
            if (add_entity(ctx, source_node->variable, ENTITY_TYPE_VERTEX, true) < 0) {
                return -1;
            }
            entity = lookup_entity(ctx, source_node->variable);
        }
        source_alias = entity->table_alias;
    } else {
        static char temp_source[32];
        snprintf(temp_source, sizeof(temp_source), "n_%d", rel_index - 1);
        source_alias = temp_source;
    }
    
    /* Target node */
    if (target_node->variable) {
        transform_entity *entity = lookup_entity(ctx, target_node->variable);
        if (!entity) {
            /* Add missing entity */
            if (add_entity(ctx, target_node->variable, ENTITY_TYPE_VERTEX, true) < 0) {
                return -1;
            }
            entity = lookup_entity(ctx, target_node->variable);
        }
        target_alias = entity->table_alias;
        /* Register in legacy system for compatibility */
        register_node_variable(ctx, target_node->variable, target_alias);
        /* Register in unified system */
        const char *label = has_labels(target_node) ? get_label_string(target_node->labels->items[0]) : NULL;
        transform_var_register_node(ctx->var_ctx, target_node->variable, target_alias, label);
    } else {
        static char temp_target[32];
        snprintf(temp_target, sizeof(temp_target), "n_%d", rel_index + 1);
        target_alias = temp_target;
    }

    /* Edge */
    if (rel->variable) {
        transform_entity *entity = lookup_entity(ctx, rel->variable);
        if (!entity) {
            /* Add new edge entity */
            if (add_entity(ctx, rel->variable, ENTITY_TYPE_EDGE, true) < 0) {
                return -1;
            }
            entity = lookup_entity(ctx, rel->variable);
        }
        edge_alias = entity->table_alias;
    } else {
        /* With AGE pattern, anonymous relationships should have been assigned names */
        /* But handle legacy case or if called from other contexts */
        char *default_name = get_next_default_alias(ctx);
        if (add_entity(ctx, default_name, ENTITY_TYPE_EDGE, true) < 0) {
            free(default_name);
            return -1;
        }
        transform_entity *entity = lookup_entity(ctx, default_name);
        edge_alias = entity->table_alias;
        /* Clean up the allocated name since entity has its own copy */
        free(default_name);
    }
    
    /* Handle variable-length relationships differently - use recursive CTE */
    if (rel->varlen) {
        CYPHER_DEBUG("Handling variable-length relationship");

        /* Generate unique CTE name */
        char cte_name[64];
        snprintf(cte_name, sizeof(cte_name), "_varlen_path_%d", rel_index);

        /* Generate the recursive CTE (appended to cte_prefix) */
        if (generate_varlen_cte(ctx, rel, source_alias, target_alias, cte_name) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to generate variable-length CTE");
            return -1;
        }

        /* Get min/max hops for filtering */
        cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
        int min_hops = range->min_hops > 0 ? range->min_hops : 1;

        /* Join the main query with the CTE result */
        /* The CTE gives us (start_id, end_id, depth, path_ids, visited) */
        append_sql(ctx, ", %s AS %s", cte_name, edge_alias);

        /* Add target node to FROM clause - needed for the CTE join */
        /* Must handle target node properties and labels properly */
        bool target_has_properties = (target_node->properties && target_node->properties->type == AST_NODE_MAP);
        cypher_map *target_prop_map = target_has_properties ? (cypher_map*)target_node->properties : NULL;
        bool target_has_prop_pairs = target_has_properties && target_prop_map->pairs && target_prop_map->pairs->count > 0;

        if (target_has_prop_pairs) {
            /* Target node has properties - need to join via property table */
            cypher_map_pair *first_pair = (cypher_map_pair*)target_prop_map->pairs->items[0];
            if (first_pair->key && first_pair->value && first_pair->value->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)first_pair->value;
                const char *prop_table = NULL;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: prop_table = "node_props_int"; break;
                    case LITERAL_STRING:  prop_table = "node_props_text"; break;
                    case LITERAL_DECIMAL: prop_table = "node_props_real"; break;
                    case LITERAL_BOOLEAN: prop_table = "node_props_bool"; break;
                    default: break;
                }
                if (prop_table) {
                    /* Join property table, property_keys, and nodes for target */
                    append_sql(ctx, ", %s AS _prop_%s", prop_table, target_alias);
                    append_sql(ctx, " JOIN property_keys AS _pk_%s ON _pk_%s.id = _prop_%s.key_id AND _pk_%s.key = ",
                               target_alias, target_alias, target_alias, target_alias);
                    append_string_literal(ctx, first_pair->key);
                    /* Add value filter */
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            append_sql(ctx, " AND _prop_%s.value = %d", target_alias, lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            append_sql(ctx, " AND _prop_%s.value = ", target_alias);
                            append_string_literal(ctx, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            append_sql(ctx, " AND _prop_%s.value = %f", target_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            append_sql(ctx, " AND _prop_%s.value = %d", target_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    append_sql(ctx, " JOIN nodes AS %s ON %s.id = _prop_%s.node_id", target_alias, target_alias, target_alias);
                    /* Mark first property as handled to skip in WHERE clause */
                    first_pair->key = NULL;
                } else {
                    append_sql(ctx, ", nodes AS %s", target_alias);
                }
            } else {
                append_sql(ctx, ", nodes AS %s", target_alias);
            }
        } else {
            append_sql(ctx, ", nodes AS %s", target_alias);
        }

        /* Add label constraints for target node if specified - one JOIN per label */
        if (has_labels(target_node)) {
            for (int i = 0; i < target_node->labels->count; i++) {
                const char *label = get_label_string(target_node->labels->items[i]);
                if (label) {
                    append_sql(ctx, " JOIN node_labels AS _nl_%s_%d ON _nl_%s_%d.node_id = %s.id AND _nl_%s_%d.label = ",
                               target_alias, i, target_alias, i, target_alias, target_alias, i);
                    append_string_literal(ctx, label);
                }
            }
        }

        CYPHER_DEBUG("Added varlen CTE join: %s for relationship between %s and %s",
                     cte_name, source_alias, target_alias);

        /* Store info for WHERE clause generation later */
        /* We'll add the join conditions in the relationship constraint phase */
        if (rel->variable) {
            register_edge_variable(ctx, rel->variable, edge_alias);
            /* Register in unified system */
            transform_var_register_edge(ctx->var_ctx, rel->variable, edge_alias, rel->type);
        }

        /* We need to add WHERE constraints for the CTE join */
        /* Check if this is the first constraint */
        bool has_where = (strstr(ctx->sql_buffer, " WHERE ") != NULL);
        if (!has_where) {
            append_sql(ctx, " WHERE ");
        } else {
            append_sql(ctx, " AND ");
        }
        append_sql(ctx, "%s.start_id = %s.id AND %s.end_id = %s.id",
                   edge_alias, source_alias, edge_alias, target_alias);

        /* Add minimum depth constraint if > 1 */
        if (min_hops > 1) {
            append_sql(ctx, " AND %s.depth >= %d", edge_alias, min_hops);
        }

        /* Add shortest path filtering based on path type */
        if (ptype == PATH_TYPE_SHORTEST || ptype == PATH_TYPE_ALL_SHORTEST) {
            CYPHER_DEBUG("Adding shortest path filtering (type=%d)", ptype);
            /* Filter to only paths with minimum depth between the source and target */
            /* This ensures we get the shortest path(s) for the specific pair being matched */
            append_sql(ctx, " AND %s.depth = (SELECT MIN(sp.depth) FROM %s sp WHERE sp.start_id = %s.id AND sp.end_id = %s.id)",
                       edge_alias, cte_name, source_alias, target_alias);
        }

        return 0; /* Skip the rest of the relationship handling */
    }
    /* Add edges table - use LEFT JOIN for optional relationships */
    else if (ctx->sql_builder.using_builder) {
        /* Using SQL builder */
        if (optional) {
            /* For OPTIONAL MATCH, we LEFT JOIN edges first, then target through edge */
            /* This ensures we get NULLs for unmatched patterns, not cartesian products */

            /* First, LEFT JOIN the edges table from the source node */
            append_join_clause(ctx, " LEFT JOIN edges AS %s ON %s.source_id = %s.id",
                              edge_alias, edge_alias, source_alias);

            /* Add relationship type constraint to edge JOIN */
            if (rel->type) {
                /* Single type (legacy support) */
                append_join_clause(ctx, " AND %s.type = '%s'", edge_alias, rel->type);
            } else if (rel->types && rel->types->count > 0) {
                /* Multiple types - generate OR conditions */
                append_join_clause(ctx, " AND (");
                for (int t = 0; t < rel->types->count; t++) {
                    if (t > 0) {
                        append_join_clause(ctx, " OR ");
                    }
                    /* Type names are stored as string literals in the list */
                    cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                    append_join_clause(ctx, "%s.type = '%s'", edge_alias, type_lit->value.string);
                }
                append_join_clause(ctx, ")");
            }

            /* Then, LEFT JOIN target node through the edge's target_id */
            /* Check if target node is already added to avoid duplicates */
            bool target_already_added = false;
            if (ctx->sql_builder.from_clause && strstr(ctx->sql_builder.from_clause, target_alias)) {
                target_already_added = true;
            }
            if (!target_already_added && ctx->sql_builder.join_clauses && strstr(ctx->sql_builder.join_clauses, target_alias)) {
                target_already_added = true;
            }

            if (!target_already_added) {
                append_join_clause(ctx, " LEFT JOIN nodes AS %s ON %s.id = %s.target_id",
                                  target_alias, target_alias, edge_alias);
            }
        } else {
            append_from_clause(ctx, ", edges AS %s", edge_alias);
        }
    } else {
        /* Traditional SQL generation */
        if (optional) {
            /* For OPTIONAL MATCH, we LEFT JOIN edges first, then target through edge */
            /* This ensures we get NULLs for unmatched patterns, not cartesian products */

            /* First, LEFT JOIN the edges table from the source node */
            append_sql(ctx, " LEFT JOIN edges AS %s ON %s.source_id = %s.id",
                       edge_alias, edge_alias, source_alias);

            /* Add relationship type constraint to edge JOIN */
            if (rel->type) {
                /* Single type (legacy support) */
                append_sql(ctx, " AND %s.type = ", edge_alias);
                append_string_literal(ctx, rel->type);
            } else if (rel->types && rel->types->count > 0) {
                /* Multiple types - generate OR conditions */
                append_sql(ctx, " AND (");
                for (int t = 0; t < rel->types->count; t++) {
                    if (t > 0) {
                        append_sql(ctx, " OR ");
                    }
                    /* Type names are stored as string literals in the list */
                    cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                    append_sql(ctx, "%s.type = ", edge_alias);
                    append_string_literal(ctx, type_lit->value.string);
                }
                append_sql(ctx, ")");
            }

            /* Then, LEFT JOIN target node through the edge's target_id */
            append_sql(ctx, " LEFT JOIN nodes AS %s ON %s.id = %s.target_id",
                       target_alias, target_alias, edge_alias);
        } else {
            append_sql(ctx, ", edges AS %s", edge_alias);
        }
    }
    
    /* Note: Relationship constraints will be added later in the WHERE clause phase */
    
    /* Register relationship variable if present */
    if (rel->variable) {
        register_edge_variable(ctx, rel->variable, edge_alias);
        /* Register in unified system */
        transform_var_register_edge(ctx->var_ctx, rel->variable, edge_alias, rel->type);
    } else {
        /* For unnamed relationships, we need a way to track them */
        /* Create a synthetic variable name based on position for tracking */
        char synthetic_var[32];
        snprintf(synthetic_var, sizeof(synthetic_var), "__unnamed_rel_%d", rel_index);
        register_edge_variable(ctx, synthetic_var, edge_alias);
        /* Register in unified system */
        transform_var_register_edge(ctx->var_ctx, synthetic_var, edge_alias, rel->type);
    }
    
    CYPHER_DEBUG("Generated relationship match: %s connects %s to %s", 
                 edge_alias, source_alias, target_alias);
    
    return 0;
}

/* Transform WHERE clause expression (used by other modules) */
int transform_where_clause(cypher_transform_context *ctx, ast_node *where)
{
    CYPHER_DEBUG("Transforming WHERE clause expression, type: %s", 
                 where ? ast_node_type_name(where->type) : "NULL");
    
    if (!where) {
        return 0;
    }
    
    /* Debug the WHERE AST structure */
    if (where->type == AST_NODE_BINARY_OP) {
        cypher_binary_op *binop = (cypher_binary_op*)where;
        CYPHER_DEBUG("WHERE contains binary op: op_type=%d, left=%s, right=%s",
                     binop->op_type,
                     binop->left ? ast_node_type_name(binop->left->type) : "NULL",
                     binop->right ? ast_node_type_name(binop->right->type) : "NULL");
    }
    
    /* Transform the WHERE expression - caller handles WHERE/AND keywords */
    int result = transform_expression(ctx, where);
    CYPHER_DEBUG("WHERE transformation result: %d, SQL so far: %s", result, ctx->sql_buffer);
    return result;
}