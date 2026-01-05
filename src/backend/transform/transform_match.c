/*
 * MATCH clause transformation
 * Converts MATCH patterns into SQL SELECT queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern, bool optional);
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional);
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node,
                                     int rel_index, bool optional, path_type ptype);

/*
 * Generate the proper node id reference for join conditions.
 * For regular nodes: returns "alias.id"
 * For projected variables (from WITH): returns just the alias (which IS the id)
 *
 * Returns a static buffer - not thread-safe, must be used immediately.
 * If var_name is NULL, falls back to using alias.id.
 */
static const char *get_node_id_ref(cypher_transform_context *ctx,
                                   const char *alias,
                                   const char *var_name)
{
    static char id_ref_buf[256];

    /* Check if this is a projected variable or a post-WITH node/edge (alias_is_id) */
    bool skip_id_suffix = var_name &&
        (transform_var_is_projected(ctx->var_ctx, var_name) ||
         transform_var_alias_is_id(ctx->var_ctx, var_name));

    if (skip_id_suffix) {
        /* For projected variables or post-WITH variables, the alias IS the id value */
        snprintf(id_ref_buf, sizeof(id_ref_buf), "%s", alias);
    } else {
        /* For regular nodes, use alias.id */
        snprintf(id_ref_buf, sizeof(id_ref_buf), "%s.id", alias);
    }

    return id_ref_buf;
}

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

    /* Unified builder handles SELECT in RETURN clause - no need to start SELECT here */

    /* Multi-graph support: set current graph for table prefixing */
    ctx->current_graph = match->from_graph;

    /* Save variable count before processing patterns (for multi-graph support) */
    int var_count_before = transform_var_count(ctx->var_ctx);

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

    /* Multi-graph support: set graph on all newly registered variables */
    if (match->from_graph) {
        int var_count_after = transform_var_count(ctx->var_ctx);
        for (int i = var_count_before; i < var_count_after; i++) {
            transform_var *var = transform_var_at(ctx->var_ctx, i);
            if (var && var->name) {
                transform_var_set_graph(ctx->var_ctx, var->name, match->from_graph);
            }
        }
    }

    /* Now add WHERE constraints for all patterns */
    /* All constraints go through unified builder's sql_where() */
    
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
                
                /* Use unified variable system for node variables */
                const char *alias;
                if (node->variable) {
                    transform_var *var = transform_var_lookup(ctx->var_ctx, node->variable);
                    if (!var) {
                        /* New variable - register it */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (!gen_alias) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to allocate alias");
                            return -1;
                        }
                        const char *label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                        if (transform_var_register_node(ctx->var_ctx, node->variable, gen_alias, label) < 0) {
                            free(gen_alias);
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to add node variable");
                            return -1;
                        }
                        free(gen_alias);
                    }
                    alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                    if (!alias) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to get node variable alias");
                        return -1;
                    }
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
                                dynamic_buffer cond;
                                dbuf_init(&cond);

                                cypher_literal *lit = (cypher_literal*)pair->value;
                                /* Property value constraint - _prop_<alias> was added in generate_node_match */
                                switch (lit->literal_type) {
                                    case LITERAL_STRING: {
                                        /* Escape single quotes in string */
                                        char *escaped = escape_sql_string(lit->value.string);
                                        dbuf_appendf(&cond, "_prop_%s.value = '%s'", alias, escaped ? escaped : lit->value.string);
                                        free(escaped);
                                        break;
                                    }
                                    case LITERAL_INTEGER:
                                        dbuf_appendf(&cond, "_prop_%s.value = %d", alias, lit->value.integer);
                                        break;
                                    case LITERAL_DECIMAL:
                                        dbuf_appendf(&cond, "_prop_%s.value = %f", alias, lit->value.decimal);
                                        break;
                                    case LITERAL_BOOLEAN:
                                        dbuf_appendf(&cond, "_prop_%s.value = %d", alias, lit->value.boolean ? 1 : 0);
                                        break;
                                    case LITERAL_NULL:
                                        /* NULL check - property should not exist */
                                        dbuf_appendf(&cond, "_prop_%s.node_id IS NULL", alias);
                                        break;
                                }

                                if (!dbuf_is_empty(&cond)) {
                                    sql_where(ctx->unified_builder, dbuf_get(&cond));
                                }
                                dbuf_free(&cond);
                            } else if (pair->value && pair->value->type == AST_NODE_PARAMETER) {
                                /* Handle parameter in property filter */
                                cypher_parameter *param = (cypher_parameter*)pair->value;
                                dynamic_buffer cond;
                                dbuf_init(&cond);

                                /* Use OR conditions to check each property type table
                                 * This handles string, int, real, and bool params correctly */
                                dbuf_appendf(&cond,
                                    "("
                                    /* String match */
                                    "EXISTS(SELECT 1 FROM node_props_text npt "
                                    "JOIN property_keys pk ON npt.key_id = pk.id "
                                    "WHERE npt.node_id = %s.id AND pk.key = '%s' AND npt.value = :%s) OR "
                                    /* Integer match */
                                    "EXISTS(SELECT 1 FROM node_props_int npi "
                                    "JOIN property_keys pk ON npi.key_id = pk.id "
                                    "WHERE npi.node_id = %s.id AND pk.key = '%s' AND npi.value = :%s) OR "
                                    /* Real match */
                                    "EXISTS(SELECT 1 FROM node_props_real npr "
                                    "JOIN property_keys pk ON npr.key_id = pk.id "
                                    "WHERE npr.node_id = %s.id AND pk.key = '%s' AND npr.value = :%s) OR "
                                    /* Boolean match */
                                    "EXISTS(SELECT 1 FROM node_props_bool npb "
                                    "JOIN property_keys pk ON npb.key_id = pk.id "
                                    "WHERE npb.node_id = %s.id AND pk.key = '%s' AND npb.value = :%s)"
                                    ")",
                                    alias, pair->key, param->name,
                                    alias, pair->key, param->name,
                                    alias, pair->key, param->name,
                                    alias, pair->key, param->name);

                                sql_where(ctx->unified_builder, dbuf_get(&cond));
                                dbuf_free(&cond);
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

                /* Get aliases using unified variable system */
                const char *source_alias, *target_alias, *edge_alias;

                /* Source node alias */
                if (source_node->variable) {
                    source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
                    if (!source_alias) {
                        /* Should have been added already, but add if missing */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            transform_var_register_node(ctx->var_ctx, source_node->variable, gen_alias, NULL);
                            free(gen_alias);
                            source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
                        }
                    }
                    if (!source_alias) continue;
                } else {
                    static char temp_source[32];
                    snprintf(temp_source, sizeof(temp_source), "n_%d", j - 1);
                    source_alias = temp_source;
                }

                /* Target node alias */
                if (target_node->variable) {
                    target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
                    if (!target_alias) {
                        /* Should have been added already, but add if missing */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            transform_var_register_node(ctx->var_ctx, target_node->variable, gen_alias, NULL);
                            free(gen_alias);
                            target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
                        }
                    }
                    if (!target_alias) continue;
                } else {
                    static char temp_target[32];
                    snprintf(temp_target, sizeof(temp_target), "n_%d", j + 1);
                    target_alias = temp_target;
                }

                /* Edge alias */
                if (rel->variable) {
                    edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                    if (!edge_alias) {
                        /* New relationship variable */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            transform_var_register_edge(ctx->var_ctx, rel->variable, gen_alias, rel->type);
                            free(gen_alias);
                            edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                        }
                    }
                    if (!edge_alias) continue;
                } else {
                    /* This shouldn't happen with AGE pattern - anonymous rels get names assigned */
                    ctx->has_error = true;
                    ctx->error_message = strdup("Internal error: anonymous relationship without assigned name");
                    return -1;
                }
                
                /* Add relationship direction constraints using unified builder */
                dynamic_buffer rel_cond;
                dbuf_init(&rel_cond);

                /* Handle relationship direction */
                /* Get proper id references (handles projected variables from WITH) */
                char source_id_ref[256], target_id_ref[256];
                snprintf(source_id_ref, sizeof(source_id_ref), "%s",
                         get_node_id_ref(ctx, source_alias, source_node->variable));
                snprintf(target_id_ref, sizeof(target_id_ref), "%s",
                         get_node_id_ref(ctx, target_alias, target_node->variable));

                if (rel->left_arrow && !rel->right_arrow) {
                    /* <-[:TYPE]- (reversed: target -> source) */
                    dbuf_appendf(&rel_cond, "%s.source_id = %s AND %s.target_id = %s",
                              edge_alias, target_id_ref, edge_alias, source_id_ref);
                } else {
                    /* -[:TYPE]-> or -[:TYPE]- (forward or undirected, treat as forward) */
                    dbuf_appendf(&rel_cond, "%s.source_id = %s AND %s.target_id = %s",
                              edge_alias, source_id_ref, edge_alias, target_id_ref);
                }

                /* Add relationship type constraint if specified */
                if (rel->type) {
                    /* Single type (legacy support) */
                    char *escaped = escape_sql_string(rel->type);
                    dbuf_appendf(&rel_cond, " AND %s.type = '%s'", edge_alias, escaped ? escaped : rel->type);
                    free(escaped);
                } else if (rel->types && rel->types->count > 0) {
                    /* Multiple types - generate OR conditions */
                    dbuf_appendf(&rel_cond, " AND (");
                    for (int t = 0; t < rel->types->count; t++) {
                        if (t > 0) {
                            dbuf_appendf(&rel_cond, " OR ");
                        }
                        /* Type names are stored as string literals in the list */
                        cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                        char *type_escaped = escape_sql_string(type_lit->value.string);
                        dbuf_appendf(&rel_cond, "%s.type = '%s'", edge_alias, type_escaped ? type_escaped : type_lit->value.string);
                        free(type_escaped);
                    }
                    dbuf_appendf(&rel_cond, ")");
                }

                sql_where(ctx->unified_builder, dbuf_get(&rel_cond));
                dbuf_free(&rel_cond);
            }
        }
    }
    
handle_where_clause:
    /* Handle WHERE clause if present - capture expression to unified builder */
    if (match->where) {
        /* Save current sql_buffer state (transform_expression uses it temporarily) */
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

        /* Add the expression to unified builder's WHERE clause */
        if (ctx->sql_size > 0) {
            sql_where(ctx->unified_builder, ctx->sql_buffer);
        }

        /* Restore sql_buffer */
        ctx->sql_size = saved_size;
        if (saved_buffer) {
            strcpy(ctx->sql_buffer, saved_buffer);
            free(saved_buffer);
        } else if (ctx->sql_buffer) {
            ctx->sql_buffer[0] = '\0';
        }
    }

    /* Clear current graph after MATCH processing */
    ctx->current_graph = NULL;

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
            
            /* Use unified variable system */
            const char *alias;
            bool need_from_clause = false;

            if (node->variable) {
                transform_var *var = transform_var_lookup(ctx->var_ctx, node->variable);
                if (var) {
                    /* Variable exists - check if it's from WITH (projected or alias_is_id) */
                    bool is_from_with = (var->kind == VAR_KIND_PROJECTED) ||
                                       transform_var_alias_is_id(ctx->var_ctx, node->variable);
                    if (is_from_with) {
                        /* Variable from WITH - use the CTE, don't add nodes table */
                        alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                        if (!alias) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to get alias for projected variable");
                            return -1;
                        }
                        /* Extract CTE name from source_expr (e.g., "_with_0.p" -> "_with_0") */
                        char cte_name[64];
                        const char *dot = strchr(alias, '.');
                        if (dot) {
                            size_t len = dot - alias;
                            if (len >= sizeof(cte_name)) len = sizeof(cte_name) - 1;
                            strncpy(cte_name, alias, len);
                            cte_name[len] = '\0';
                        } else {
                            strncpy(cte_name, alias, sizeof(cte_name) - 1);
                            cte_name[sizeof(cte_name) - 1] = '\0';
                        }
                        /* Add CTE to FROM clause if not already there */
                        bool cte_in_from = !dbuf_is_empty(&ctx->unified_builder->from) &&
                                           strstr(dbuf_get(&ctx->unified_builder->from), cte_name) != NULL;
                        bool cte_in_joins = !dbuf_is_empty(&ctx->unified_builder->joins) &&
                                            strstr(dbuf_get(&ctx->unified_builder->joins), cte_name) != NULL;
                        if (!cte_in_from && !cte_in_joins) {
                            /* Add as CROSS JOIN if FROM already exists, otherwise as FROM */
                            if (!dbuf_is_empty(&ctx->unified_builder->from)) {
                                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, cte_name, NULL, NULL);
                            } else {
                                sql_from(ctx->unified_builder, cte_name, NULL);
                            }
                        }
                        need_from_clause = false; /* Don't add nodes table */
                    } else {
                        /* Regular variable - reuse alias, check if we need FROM clause */
                        alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                        if (!alias) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to get alias for existing variable");
                            return -1;
                        }
                        /* Check if this alias is already in the unified builder */
                        bool alias_in_builder_from = !dbuf_is_empty(&ctx->unified_builder->from) &&
                                                     strstr(dbuf_get(&ctx->unified_builder->from), alias) != NULL;
                        bool alias_in_builder_joins = !dbuf_is_empty(&ctx->unified_builder->joins) &&
                                                      strstr(dbuf_get(&ctx->unified_builder->joins), alias) != NULL;
                        if (!alias_in_builder_from && !alias_in_builder_joins) {
                            need_from_clause = true;
                        }
                    }
                } else {
                    /* New variable - register it */
                    char *gen_alias = get_next_default_alias(ctx);
                    if (!gen_alias) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to allocate alias");
                        return -1;
                    }
                    const char *label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                    if (transform_var_register_node(ctx->var_ctx, node->variable, gen_alias, label) < 0) {
                        free(gen_alias);
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to add node variable in pattern");
                        return -1;
                    }
                    free(gen_alias);
                    alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                    if (!alias) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to get alias for node variable in pattern");
                        return -1;
                    }
                    need_from_clause = true;
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

    /* Check if there's already a FROM clause using the unified builder */
    bool has_from = !dbuf_is_empty(&ctx->unified_builder->from);
    sql_join_type jtype = optional ? SQL_JOIN_LEFT : SQL_JOIN_INNER;

    /* Check if node has properties - if so, start from property table for better selectivity */
    bool has_properties = (node->properties && node->properties->type == AST_NODE_MAP);
    cypher_map *prop_map = has_properties ? (cypher_map*)node->properties : NULL;
    bool has_prop_pairs = has_properties && prop_map->pairs && prop_map->pairs->count > 0;

    /* Buffer for building ON conditions */
    dynamic_buffer on_cond;
    char prop_alias[64], pk_alias[64], node_alias_buf[64];

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
                    /* FROM property_table */
                    snprintf(prop_alias, sizeof(prop_alias), "_prop_%s", alias);
                    sql_from(ctx->unified_builder, get_graph_table(ctx, prop_table), prop_alias);

                    /* JOIN property_keys with key filter and value filter */
                    snprintf(pk_alias, sizeof(pk_alias), "_pk_%s", alias);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.key_id AND %s.key = '%s'",
                                 pk_alias, prop_alias, pk_alias, first_pair->key);
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&on_cond, " AND %s.value = '%s'", prop_alias, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&on_cond, " AND %s.value = %f", prop_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "property_keys"), pk_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* JOIN nodes */
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.node_id", alias, prop_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "nodes"), alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* Mark first property as handled */
                    first_pair->key = NULL;
                } else {
                    sql_from(ctx->unified_builder, get_graph_table(ctx, "nodes"), alias);
                }
            } else {
                sql_from(ctx->unified_builder, get_graph_table(ctx, "nodes"), alias);
            }
        } else {
            sql_from(ctx->unified_builder, get_graph_table(ctx, "nodes"), alias);
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
                    /* JOIN property_table */
                    snprintf(prop_alias, sizeof(prop_alias), "_prop_%s", alias);
                    sql_join(ctx->unified_builder, jtype, get_graph_table(ctx, prop_table), prop_alias, "1=1");

                    /* JOIN property_keys with key filter and value filter */
                    snprintf(pk_alias, sizeof(pk_alias), "_pk_%s", alias);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.key_id AND %s.key = '%s'",
                                 pk_alias, prop_alias, pk_alias, first_pair->key);
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&on_cond, " AND %s.value = '%s'", prop_alias, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&on_cond, " AND %s.value = %f", prop_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "property_keys"), pk_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* JOIN nodes */
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.node_id", alias, prop_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "nodes"), alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    first_pair->key = NULL;
                } else {
                    sql_join(ctx->unified_builder, jtype, get_graph_table(ctx, "nodes"), alias, "1=1");
                }
            } else {
                sql_join(ctx->unified_builder, jtype, get_graph_table(ctx, "nodes"), alias, "1=1");
            }
        } else {
            if (optional) {
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT, get_graph_table(ctx, "nodes"), alias, "1=1");
            } else {
                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), alias, NULL);
            }
        }
    }

    /* Add label JOINs if specified - one JOIN per label for multi-label support */
    if (has_labels(node)) {
        /* Get proper node id reference (handles projected variables from WITH) */
        const char *node_id = get_node_id_ref(ctx, alias, node->variable);

        for (int i = 0; i < node->labels->count; i++) {
            const char *label = get_label_string(node->labels->items[i]);
            if (label) {
                char nl_alias[64];
                snprintf(nl_alias, sizeof(nl_alias), "_nl_%s_%d", alias, i);
                dbuf_init(&on_cond);
                dbuf_appendf(&on_cond, "%s.node_id = %s AND %s.label = '%s'",
                             nl_alias, node_id, nl_alias, label);
                sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "node_labels"), nl_alias, dbuf_get(&on_cond));
                dbuf_free(&on_cond);
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

    /* Get aliases using unified variable system */
    const char *source_alias, *target_alias, *edge_alias;

    /* Source node */
    if (source_node->variable) {
        source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
        if (!source_alias) {
            /* Add missing variable */
            char *gen_alias = get_next_default_alias(ctx);
            if (!gen_alias) return -1;
            transform_var_register_node(ctx->var_ctx, source_node->variable, gen_alias, NULL);
            free(gen_alias);
            source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
        }
        if (!source_alias) return -1;
    } else {
        static char temp_source[32];
        snprintf(temp_source, sizeof(temp_source), "n_%d", rel_index - 1);
        source_alias = temp_source;
    }

    /* Target node */
    if (target_node->variable) {
        target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
        if (!target_alias) {
            /* Add missing variable */
            char *gen_alias = get_next_default_alias(ctx);
            if (!gen_alias) return -1;
            const char *label = has_labels(target_node) ? get_label_string(target_node->labels->items[0]) : NULL;
            transform_var_register_node(ctx->var_ctx, target_node->variable, gen_alias, label);
            free(gen_alias);
            target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
        }
        if (!target_alias) return -1;
    } else {
        static char temp_target[32];
        snprintf(temp_target, sizeof(temp_target), "n_%d", rel_index + 1);
        target_alias = temp_target;
    }

    /* Edge */
    if (rel->variable) {
        edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
        if (!edge_alias) {
            /* Add new edge variable */
            char *gen_alias = get_next_default_alias(ctx);
            if (!gen_alias) return -1;
            transform_var_register_edge(ctx->var_ctx, rel->variable, gen_alias, rel->type);
            free(gen_alias);
            edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
        }
        if (!edge_alias) return -1;
    } else {
        /* Anonymous relationship - generate name and register */
        char *default_name = get_next_default_alias(ctx);
        char *gen_alias = get_next_default_alias(ctx);
        if (!default_name || !gen_alias) {
            free(default_name);
            free(gen_alias);
            return -1;
        }
        transform_var_register_edge(ctx->var_ctx, default_name, gen_alias, rel->type);
        edge_alias = transform_var_get_alias(ctx->var_ctx, default_name);
        free(default_name);
        free(gen_alias);
        if (!edge_alias) return -1;
    }
    
    /* Handle variable-length relationships differently - use recursive CTE */
    if (rel->varlen) {
        CYPHER_DEBUG("Handling variable-length relationship");

        /* Generate unique CTE name */
        char cte_name[64];
        snprintf(cte_name, sizeof(cte_name), "_varlen_path_%d", rel_index);

        /* Generate the recursive CTE (added to unified builder) */
        if (generate_varlen_cte(ctx, rel, source_alias, target_alias, cte_name) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to generate variable-length CTE");
            return -1;
        }

        /* Get min/max hops for filtering */
        cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
        int min_hops = range->min_hops > 0 ? range->min_hops : 1;

        /* Join the main query with the CTE result using unified builder */
        sql_join(ctx->unified_builder, SQL_JOIN_CROSS, cte_name, edge_alias, NULL);

        /* Add target node to FROM clause - needed for the CTE join */
        bool target_has_properties = (target_node->properties && target_node->properties->type == AST_NODE_MAP);
        cypher_map *target_prop_map = target_has_properties ? (cypher_map*)target_node->properties : NULL;
        bool target_has_prop_pairs = target_has_properties && target_prop_map->pairs && target_prop_map->pairs->count > 0;

        dynamic_buffer on_cond;
        char prop_alias[64], pk_alias[64];

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
                    /* CROSS JOIN property table */
                    snprintf(prop_alias, sizeof(prop_alias), "_prop_%s", target_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, prop_table), prop_alias, NULL);

                    /* JOIN property_keys with key and value filter */
                    snprintf(pk_alias, sizeof(pk_alias), "_pk_%s", target_alias);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.key_id AND %s.key = '%s'",
                                 pk_alias, prop_alias, pk_alias, first_pair->key);
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&on_cond, " AND %s.value = '%s'", prop_alias, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&on_cond, " AND %s.value = %f", prop_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "property_keys"), pk_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* JOIN nodes */
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.node_id", target_alias, prop_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "nodes"), target_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    first_pair->key = NULL;
                } else {
                    sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), target_alias, NULL);
                }
            } else {
                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), target_alias, NULL);
            }
        } else {
            sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), target_alias, NULL);
        }

        /* Add label constraints for target node if specified */
        if (has_labels(target_node)) {
            const char *target_id = get_node_id_ref(ctx, target_alias, target_node->variable);

            for (int i = 0; i < target_node->labels->count; i++) {
                const char *label = get_label_string(target_node->labels->items[i]);
                if (label) {
                    char nl_alias[64];
                    snprintf(nl_alias, sizeof(nl_alias), "_nl_%s_%d", target_alias, i);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.node_id = %s AND %s.label = '%s'",
                                 nl_alias, target_id, nl_alias, label);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "node_labels"), nl_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);
                }
            }
        }

        CYPHER_DEBUG("Added varlen CTE join: %s for relationship between %s and %s",
                     cte_name, source_alias, target_alias);

        /* Add WHERE constraints for the CTE join using unified builder */
        char src_id_ref[256], tgt_id_ref[256];
        snprintf(src_id_ref, sizeof(src_id_ref), "%s",
                 get_node_id_ref(ctx, source_alias, source_node->variable));
        snprintf(tgt_id_ref, sizeof(tgt_id_ref), "%s",
                 get_node_id_ref(ctx, target_alias, target_node->variable));

        dbuf_init(&on_cond);
        dbuf_appendf(&on_cond, "%s.start_id = %s AND %s.end_id = %s",
                     edge_alias, src_id_ref, edge_alias, tgt_id_ref);

        /* Add minimum depth constraint if > 1 */
        if (min_hops > 1) {
            dbuf_appendf(&on_cond, " AND %s.depth >= %d", edge_alias, min_hops);
        }

        /* Add shortest path filtering based on path type */
        if (ptype == PATH_TYPE_SHORTEST || ptype == PATH_TYPE_ALL_SHORTEST) {
            CYPHER_DEBUG("Adding shortest path filtering (type=%d)", ptype);
            dbuf_appendf(&on_cond, " AND %s.depth = (SELECT MIN(sp.depth) FROM %s sp WHERE sp.start_id = %s AND sp.end_id = %s)",
                         edge_alias, cte_name, src_id_ref, tgt_id_ref);
        }

        sql_where(ctx->unified_builder, dbuf_get(&on_cond));
        dbuf_free(&on_cond);

        return 0; /* Skip the rest of the relationship handling */
    }
    /* Add edges table - use LEFT JOIN for optional relationships */
    else {
        if (optional) {
            /* For OPTIONAL MATCH, we LEFT JOIN edges first, then target through edge */
            /* This ensures we get NULLs for unmatched patterns, not cartesian products */

            /* Get proper source id reference (handles projected variables from WITH) */
            const char *source_id = get_node_id_ref(ctx, source_alias, source_node->variable);

            /* Build the edge JOIN condition */
            dynamic_buffer edge_cond;
            dbuf_init(&edge_cond);
            dbuf_appendf(&edge_cond, "%s.source_id = %s", edge_alias, source_id);

            /* Add relationship type constraint to edge JOIN */
            if (rel->type) {
                /* Single type (legacy support) */
                dbuf_appendf(&edge_cond, " AND %s.type = '%s'", edge_alias, rel->type);
            } else if (rel->types && rel->types->count > 0) {
                /* Multiple types - generate OR conditions */
                dbuf_append(&edge_cond, " AND (");
                for (int t = 0; t < rel->types->count; t++) {
                    if (t > 0) {
                        dbuf_append(&edge_cond, " OR ");
                    }
                    cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                    dbuf_appendf(&edge_cond, "%s.type = '%s'", edge_alias, type_lit->value.string);
                }
                dbuf_append(&edge_cond, ")");
            }

            sql_join(ctx->unified_builder, SQL_JOIN_LEFT, get_graph_table(ctx, "edges"), edge_alias, dbuf_get(&edge_cond));
            dbuf_free(&edge_cond);

            /* Then, LEFT JOIN target node through the edge's target_id */
            /* Check if target node is already added to avoid duplicates */
            bool target_already_added = false;
            const char *from_str = dbuf_get(&ctx->unified_builder->from);
            const char *joins_str = dbuf_get(&ctx->unified_builder->joins);
            if (from_str && strstr(from_str, target_alias)) {
                target_already_added = true;
            }
            if (!target_already_added && joins_str && strstr(joins_str, target_alias)) {
                target_already_added = true;
            }

            if (!target_already_added) {
                char target_cond[256];
                snprintf(target_cond, sizeof(target_cond), "%s.id = %s.target_id", target_alias, edge_alias);
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT, get_graph_table(ctx, "nodes"), target_alias, target_cond);
            }
        } else {
            /* Non-optional: use CROSS JOIN for edges (will be filtered by WHERE) */
            sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "edges"), edge_alias, NULL);
        }
    }
    
    /* Note: Relationship constraints will be added later in the WHERE clause phase */
    
    /* Register relationship variable if present */
    if (rel->variable) {
        /* Register in unified system */
        transform_var_register_edge(ctx->var_ctx, rel->variable, edge_alias, rel->type);
    } else {
        /* For unnamed relationships, we need a way to track them */
        /* Create a synthetic variable name based on position for tracking */
        char synthetic_var[32];
        snprintf(synthetic_var, sizeof(synthetic_var), "__unnamed_rel_%d", rel_index);
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