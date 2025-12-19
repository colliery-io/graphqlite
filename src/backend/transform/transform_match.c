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
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern, bool optional);
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional);
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel, 
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node, int rel_index, bool optional);

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
    if (!ctx->sql_builder.using_builder && ctx->sql_size == 0) {
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
                } else {
                    /* Anonymous node - use legacy approach for now */
                    char temp_alias[32];
                    snprintf(temp_alias, sizeof(temp_alias), "n_%d", j);
                    alias = temp_alias;
                }
                
                /* Add label constraint if specified */
                if (node->label) {
                    if (ctx->sql_builder.using_builder) {
                        /* Using SQL builder - collect WHERE constraints */
                        if (ctx->sql_builder.where_size > 0) {
                            append_where_clause(ctx, " AND ");
                        }
                        append_where_clause(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.id AND label = '%s')", 
                                          alias, node->label);
                    } else {
                        /* Traditional SQL generation */
                        if (first_constraint) {
                            append_sql(ctx, " WHERE ");
                            first_constraint = false;
                        } else {
                            append_sql(ctx, " AND ");
                        }
                        
                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.id AND label = ", alias);
                        append_string_literal(ctx, node->label);
                        append_sql(ctx, ")");
                    }
                }
                
                /* Add property constraints if specified */
                if (node->properties && node->properties->type == AST_NODE_MAP) {
                    cypher_map *map = (cypher_map*)node->properties;
                    if (map->pairs) {
                        for (int k = 0; k < map->pairs->count; k++) {
                            cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[k];
                            if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                if (first_constraint) {
                                    append_sql(ctx, " WHERE ");
                                    first_constraint = false;
                                } else {
                                    append_sql(ctx, " AND ");
                                }
                                
                                cypher_literal *lit = (cypher_literal*)pair->value;
                                switch (lit->literal_type) {
                                    case LITERAL_STRING:
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = ", alias);
                                        append_string_literal(ctx, pair->key);
                                        append_sql(ctx, " AND npt.value = ");
                                        append_string_literal(ctx, lit->value.string);
                                        append_sql(ctx, ")");
                                        break;
                                    case LITERAL_INTEGER:
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = ", alias);
                                        append_string_literal(ctx, pair->key);
                                        append_sql(ctx, " AND npi.value = %d)", lit->value.integer);
                                        break;
                                    case LITERAL_DECIMAL:
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = ", alias);
                                        append_string_literal(ctx, pair->key);
                                        append_sql(ctx, " AND npr.value = %f)", lit->value.decimal);
                                        break;
                                    case LITERAL_BOOLEAN:
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = ", alias);
                                        append_string_literal(ctx, pair->key);
                                        append_sql(ctx, " AND npb.value = %d)", lit->value.boolean ? 1 : 0);
                                        break;
                                    case LITERAL_NULL:
                                        append_sql(ctx, "NOT EXISTS (SELECT 1 FROM property_keys pk WHERE pk.key = ");
                                        append_string_literal(ctx, pair->key);
                                        append_sql(ctx, " AND (EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %s.id AND key_id = pk.id) OR ", alias);
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_int WHERE node_id = %s.id AND key_id = pk.id) OR ", alias);
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %s.id AND key_id = pk.id) OR ", alias);
                                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %s.id AND key_id = pk.id)))", alias);
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
        /* For OPTIONAL MATCH, check if we need WHERE or AND */
        bool needs_where = true;
        if (match->optional) {
            /* Check if there's already a WHERE clause in the SQL buffer */
            needs_where = (strstr(ctx->sql_buffer, " WHERE ") == NULL);
        } else {
            /* For regular MATCH, use the first_constraint variable */
            needs_where = first_constraint;
        }
        
        /* Add WHERE or AND */
        if (needs_where) {
            append_sql(ctx, " WHERE ");
        } else {
            append_sql(ctx, " AND ");
        }
        
        if (transform_expression(ctx, match->where) < 0) {
            return -1;
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
    
    bool first_table = (ctx->sql_size == 0 || strstr(ctx->sql_buffer, "FROM") == NULL);
    
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
                    /* Check if this alias is already in the FROM clause */
                    if (!strstr(ctx->sql_buffer, entity->table_alias)) {
                        need_from_clause = true;
                    }
                } else if (entity && !entity->is_current_clause) {
                    /* Entity from previous clause - reuse but may need FROM clause */
                    alias = entity->table_alias;
                    entity->is_current_clause = true; /* Mark as used in current clause */
                    if (!strstr(ctx->sql_buffer, entity->table_alias)) {
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
            if (generate_relationship_match(ctx, rel, source_node, target_node, i, optional) < 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

/* Generate SQL for matching a node pattern */
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional)
{
    CYPHER_DEBUG("Generating %s match for node %s (label: %s)", 
                 optional ? "OPTIONAL" : "regular",
                 node->variable ? node->variable : "<anonymous>",
                 node->label ? node->label : "<no label>");
    
    if (ctx->sql_builder.using_builder) {
        /* Using SQL builder - build FROM/JOIN clauses separately */
        
        /* Check if this alias is already in the FROM or JOIN clauses to avoid duplicates */
        bool already_added = false;
        if (ctx->sql_builder.from_clause && strstr(ctx->sql_builder.from_clause, alias)) {
            already_added = true;
        }
        if (!already_added && ctx->sql_builder.join_clauses && strstr(ctx->sql_builder.join_clauses, alias)) {
            already_added = true;
        }
        
        if (!already_added) {
            if (!ctx->sql_builder.from_clause || ctx->sql_builder.from_size == 0) {
                /* First table - use FROM */
                append_from_clause(ctx, "FROM nodes AS %s", alias);
            } else {
                /* Subsequent tables - use LEFT JOIN for optional, comma for regular */
                if (optional) {
                    append_join_clause(ctx, " LEFT JOIN nodes AS %s ON 1=1", alias);
                } else {
                    append_from_clause(ctx, ", nodes AS %s", alias);
                }
            }
        }
    } else {
        /* Traditional SQL generation */
        /* Check if we need FROM clause or JOIN */
        bool has_from = (strstr(ctx->sql_buffer, "FROM") != NULL);
        
        if (!has_from) {
            /* First table - always use FROM */
            append_sql(ctx, "FROM ");
            append_sql(ctx, "nodes AS %s", alias);
        } else {
            /* Subsequent tables - use LEFT JOIN for optional, comma for regular */
            if (optional) {
                append_sql(ctx, " LEFT JOIN nodes AS %s ON 1=1", alias);
            } else {
                append_sql(ctx, ", ");
                append_sql(ctx, "nodes AS %s", alias);
            }
        }
    }
    
    /* Note: Label and property constraints will be added later in transform_match_clause */
    /* to ensure all table joins are complete before WHERE clause starts */
    
    return 0;
}

/* Generate SQL for matching a relationship pattern */
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node, int rel_index, bool optional)
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
        /* This must be done BEFORE adding WHERE constraints that reference target_alias */
        append_sql(ctx, ", nodes AS %s", target_alias);

        CYPHER_DEBUG("Added varlen CTE join: %s for relationship between %s and %s",
                     cte_name, source_alias, target_alias);

        /* Store info for WHERE clause generation later */
        /* We'll add the join conditions in the relationship constraint phase */
        if (rel->variable) {
            register_edge_variable(ctx, rel->variable, edge_alias);
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

        return 0; /* Skip the rest of the relationship handling */
    }
    /* Add edges table - use LEFT JOIN for optional relationships */
    else if (ctx->sql_builder.using_builder) {
        /* Using SQL builder */
        if (optional) {
            /* For OPTIONAL MATCH, we need to LEFT JOIN both the target node and the edge */
            /* Check if target node is already added to avoid duplicates */
            bool target_already_added = false;
            if (ctx->sql_builder.from_clause && strstr(ctx->sql_builder.from_clause, target_alias)) {
                target_already_added = true;
            }
            if (!target_already_added && ctx->sql_builder.join_clauses && strstr(ctx->sql_builder.join_clauses, target_alias)) {
                target_already_added = true;
            }

            if (!target_already_added) {
                append_join_clause(ctx, " LEFT JOIN nodes AS %s ON 1=1", target_alias);
            }

            /* Always add the edge JOIN as each relationship is unique */
            append_join_clause(ctx, " LEFT JOIN edges AS %s ON %s.source_id = %s.id AND %s.target_id = %s.id",
                              edge_alias, edge_alias, source_alias, edge_alias, target_alias);

            /* Add relationship type constraint to ON clause for OPTIONAL MATCH */
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
        } else {
            append_from_clause(ctx, ", edges AS %s", edge_alias);
        }
    } else {
        /* Traditional SQL generation */
        if (optional) {
            /* For OPTIONAL MATCH, we need to LEFT JOIN both the target node and the edge */
            append_sql(ctx, " LEFT JOIN nodes AS %s ON 1=1", target_alias);
            append_sql(ctx, " LEFT JOIN edges AS %s ON %s.source_id = %s.id AND %s.target_id = %s.id",
                       edge_alias, edge_alias, source_alias, edge_alias, target_alias);

            /* Add relationship type constraint to ON clause for OPTIONAL MATCH */
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
        } else {
            append_sql(ctx, ", edges AS %s", edge_alias);
        }
    }
    
    /* Note: Relationship constraints will be added later in the WHERE clause phase */
    
    /* Register relationship variable if present */
    if (rel->variable) {
        register_edge_variable(ctx, rel->variable, edge_alias);
    } else {
        /* For unnamed relationships, we need a way to track them */
        /* Create a synthetic variable name based on position for tracking */
        char synthetic_var[32];
        snprintf(synthetic_var, sizeof(synthetic_var), "__unnamed_rel_%d", rel_index);
        register_edge_variable(ctx, synthetic_var, edge_alias);
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