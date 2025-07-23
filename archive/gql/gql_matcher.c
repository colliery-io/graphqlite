#include "gql_matcher.h"
#include "gql_debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations for helper functions
static int64_t* find_edges_by_source_and_type(gql_execution_context_t *ctx, int64_t source_id, const char *edge_type, size_t *count);
static int64_t* find_edges_by_source(gql_execution_context_t *ctx, int64_t source_id, size_t *count);
static void destroy_binding_set_structure(variable_binding_set_t *set);

// ============================================================================
// Multi-hop Pattern Structures
// ============================================================================

typedef struct hop_step {
    gql_ast_node_t *source_node;
    gql_ast_node_t *edge;
    gql_ast_node_t *target_node;
    struct hop_step *next;
} hop_step_t;

typedef struct multi_hop_pattern {
    hop_step_t *steps;
    size_t step_count;
} multi_hop_pattern_t;

// ============================================================================
// Multi-hop Pattern Flattening
// ============================================================================

static hop_step_t* create_hop_step(gql_ast_node_t *source, gql_ast_node_t *edge, gql_ast_node_t *target) {
    hop_step_t *step = malloc(sizeof(hop_step_t));
    if (!step) return NULL;
    
    step->source_node = source;
    step->edge = edge;
    step->target_node = target;
    step->next = NULL;
    return step;
}

static void destroy_multi_hop_pattern(multi_hop_pattern_t *pattern) {
    if (!pattern) return;
    
    hop_step_t *current = pattern->steps;
    while (current) {
        hop_step_t *next = current->next;
        free(current);
        current = next;
    }
    
    free(pattern);
}

static multi_hop_pattern_t* flatten_pattern(gql_ast_node_t *pattern) {
    if (!pattern || pattern->type != GQL_AST_PATTERN) {
        return NULL;
    }
    
    multi_hop_pattern_t *result = malloc(sizeof(multi_hop_pattern_t));
    if (!result) return NULL;
    
    result->steps = NULL;
    result->step_count = 0;
    
    hop_step_t *last_step = NULL;
    gql_ast_node_t *current_pattern = pattern;
    
    // Recursively flatten nested patterns
    while (current_pattern && current_pattern->type == GQL_AST_PATTERN) {
        gql_ast_node_t *source = current_pattern->data.pattern.node;
        gql_ast_node_t *edge = current_pattern->data.pattern.edge;
        gql_ast_node_t *target = current_pattern->data.pattern.target_node;
        
        // If source is a nested pattern, we need to flatten it first
        if (source && source->type == GQL_AST_PATTERN) {
            multi_hop_pattern_t *nested = flatten_pattern(source);
            if (nested) {
                // Append all steps from nested pattern
                hop_step_t *nested_step = nested->steps;
                while (nested_step) {
                    hop_step_t *new_step = create_hop_step(nested_step->source_node, 
                                                          nested_step->edge, 
                                                          nested_step->target_node);
                    if (new_step) {
                        if (!result->steps) {
                            result->steps = new_step;
                        } else {
                            last_step->next = new_step;
                        }
                        last_step = new_step;
                        result->step_count++;
                    }
                    nested_step = nested_step->next;
                }
                
                // Use the final target of the nested pattern as our source
                source = nested->steps ? nested->steps->target_node : source;
                
                // Find the last target in the nested pattern
                nested_step = nested->steps;
                while (nested_step && nested_step->next) {
                    nested_step = nested_step->next;
                }
                if (nested_step) {
                    source = nested_step->target_node;
                }
                
                destroy_multi_hop_pattern(nested);
            }
        }
        
        // Create step for current level
        if (edge && target) {
            hop_step_t *step = create_hop_step(source, edge, target);
            if (step) {
                if (!result->steps) {
                    result->steps = step;
                } else {
                    last_step->next = step;
                }
                last_step = step;
                result->step_count++;
            }
        }
        
        // Move to next level (should be NULL for properly structured patterns)
        current_pattern = NULL;
    }
    
    GQL_DEBUG("flatten_pattern - created %zu steps", result->step_count);
    
    return result;
}

// ============================================================================
// Multi-hop Path Traversal
// ============================================================================

static match_result_set_t* match_multi_hop_pattern(gql_execution_context_t *ctx, multi_hop_pattern_t *multi_hop) {
    if (!multi_hop || !multi_hop->steps) {
        return NULL;
    }
    
    GQL_DEBUG("match_multi_hop_pattern - starting with %zu steps", multi_hop->step_count);
    
    match_result_set_t *results = malloc(sizeof(match_result_set_t));
    if (!results) return NULL;
    
    results->result_sets = NULL;
    results->result_count = 0;
    results->status = MATCH_RESULT_SUCCESS;
    results->error_message = NULL;
    
    // Start with the first step
    hop_step_t *first_step = multi_hop->steps;
    if (!first_step) {
        results->status = MATCH_RESULT_NO_MATCHES;
        return results;
    }
    
    // Match the first hop to get starting points
    match_result_set_t *current_results = match_edge_pattern(ctx, 
                                                            first_step->source_node,
                                                            first_step->edge,
                                                            first_step->target_node);
    
    if (!current_results || current_results->status != MATCH_RESULT_SUCCESS || current_results->result_count == 0) {
        GQL_DEBUG("match_multi_hop_pattern - first hop failed");
        results->status = MATCH_RESULT_NO_MATCHES;
        return results;
    }
    
    GQL_DEBUG("match_multi_hop_pattern - first hop found %zu matches", current_results->result_count);
    
    // For each subsequent step, extend the current paths
    hop_step_t *current_step = first_step->next;
    while (current_step) {
        GQL_DEBUG("match_multi_hop_pattern - processing next step");
        
        match_result_set_t *extended_results = malloc(sizeof(match_result_set_t));
        if (!extended_results) break;
        
        extended_results->result_sets = NULL;
        extended_results->result_count = 0;
        extended_results->status = MATCH_RESULT_SUCCESS;
        extended_results->error_message = NULL;
        
        // For each current result, try to extend with the next step
        for (size_t i = 0; i < current_results->result_count; i++) {
            variable_binding_set_t *current_bindings = &current_results->result_sets[i];
            
            // Get the target node from the previous step as the source for this step
            const char *intermediate_var = NULL;
            if (current_step->source_node && current_step->source_node->type == GQL_AST_NODE_PATTERN) {
                intermediate_var = current_step->source_node->data.node_pattern.variable;
            }
            
            if (!intermediate_var) {
                GQL_DEBUG("match_multi_hop_pattern - no intermediate variable found");
                continue;
            }
            
            // Get the intermediate node from current bindings
            gql_value_t *intermediate_value = get_binding(current_bindings, intermediate_var);
            if (!intermediate_value || intermediate_value->type != GQL_VALUE_NODE) {
                GQL_DEBUG("match_multi_hop_pattern - intermediate variable %s not found or wrong type", intermediate_var);
                continue;
            }
            
            int64_t intermediate_id = intermediate_value->data.node.id;
            GQL_DEBUG("match_multi_hop_pattern - extending from intermediate node %lld", (long long)intermediate_id);
            
            // Find edges from this intermediate node that match the current step
            int64_t *edge_ids = NULL;
            size_t edge_count = 0;
            
            const char *edge_type = NULL;
            if (current_step->edge && current_step->edge->type == GQL_AST_EDGE_PATTERN) {
                edge_type = current_step->edge->data.edge_pattern.type;
            }
            
            if (edge_type) {
                edge_ids = find_edges_by_source_and_type(ctx, intermediate_id, edge_type, &edge_count);
            } else {
                edge_ids = find_edges_by_source(ctx, intermediate_id, &edge_count);
            }
            
            if (!edge_ids || edge_count == 0) {
                GQL_DEBUG("match_multi_hop_pattern - no edges found from intermediate node");
                free(edge_ids);
                continue;
            }
            
            GQL_DEBUG("match_multi_hop_pattern - found %zu edges from intermediate node", edge_count);
            
            // For each edge, check if target matches the target pattern
            for (size_t j = 0; j < edge_count; j++) {
                int64_t target_id = graphqlite_get_edge_target(ctx->db, edge_ids[j]);
                if (target_id <= 0) continue;
                
                // Check if target node matches the target pattern constraints
                bool target_matches = true;
                if (current_step->target_node && current_step->target_node->type == GQL_AST_NODE_PATTERN) {
                    gql_ast_node_t *target_labels = current_step->target_node->data.node_pattern.labels;
                    if (target_labels) {
                        gql_ast_node_t *label_node = target_labels->next;
                        while (label_node && target_matches) {
                            if (label_node->type == GQL_AST_STRING_LITERAL) {
                                size_t label_count = 0;
                                char **labels = graphqlite_get_node_labels(ctx->db, target_id, &label_count);
                                bool has_this_label = false;
                                
                                if (labels) {
                                    for (size_t k = 0; k < label_count; k++) {
                                        if (strcmp(labels[k], label_node->data.string_literal.value) == 0) {
                                            has_this_label = true;
                                            break;
                                        }
                                    }
                                    graphqlite_free_labels(labels, label_count);
                                }
                                
                                if (!has_this_label) {
                                    target_matches = false;
                                }
                            }
                            label_node = label_node->next;
                        }
                    }
                }
                
                if (target_matches) {
                    // Create extended binding set
                    variable_binding_set_t *new_bindings = copy_binding_set(current_bindings);
                    if (new_bindings) {
                        // Add bindings for the edge and target node
                        if (current_step->edge && current_step->edge->type == GQL_AST_EDGE_PATTERN && 
                            current_step->edge->data.edge_pattern.variable) {
                            gql_value_t *edge_value_ptr = load_edge_value(ctx, edge_ids[j]);
                            if (!edge_value_ptr) continue;
                            add_binding(new_bindings, current_step->edge->data.edge_pattern.variable, edge_value_ptr);
                            // Note: add_binding takes ownership of the value
                        }
                        
                        if (current_step->target_node && current_step->target_node->type == GQL_AST_NODE_PATTERN &&
                            current_step->target_node->data.node_pattern.variable) {
                            gql_value_t *target_value_ptr = load_node_value(ctx, target_id);
                            if (!target_value_ptr) continue;
                            add_binding(new_bindings, current_step->target_node->data.node_pattern.variable, target_value_ptr);
                            // Note: add_binding takes ownership of the value
                        }
                        
                        // Add to extended results
                        add_match_result(extended_results, new_bindings);
                        // Note: add_match_result takes ownership of new_bindings
                    }
                }
            }
            
            free(edge_ids);
        }
        
        GQL_DEBUG("match_multi_hop_pattern - step extended to %zu results", extended_results->result_count);
        
        // Replace current results with extended results
        destroy_match_result_set(current_results);
        current_results = extended_results;
        
        if (current_results->result_count == 0) {
            GQL_DEBUG("match_multi_hop_pattern - no results after extension, stopping");
            break;
        }
        
        current_step = current_step->next;
    }
    
    GQL_DEBUG("match_multi_hop_pattern - completed with %zu final results", 
           current_results ? current_results->result_count : 0);
    
    return current_results;
}

// ============================================================================
// Helper Functions for Multi-hop
// ============================================================================

static int64_t* find_edges_by_source_and_type(gql_execution_context_t *ctx, int64_t source_id, const char *edge_type, size_t *count) {
    *count = 0;
    
    const char *sql = "SELECT id FROM edges WHERE source_id = ? AND type = ?";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(ctx->db->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, source_id);
    sqlite3_bind_text(stmt, 2, edge_type, -1, SQLITE_STATIC);
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    
    if (result_count == 0) {
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Allocate array
    int64_t *results = malloc(result_count * sizeof(int64_t));
    if (!results) {
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Second pass: collect results
    sqlite3_reset(stmt);
    size_t i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < result_count) {
        results[i++] = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    *count = result_count;
    return results;
}

static int64_t* find_edges_by_source(gql_execution_context_t *ctx, int64_t source_id, size_t *count) {
    *count = 0;
    
    const char *sql = "SELECT id FROM edges WHERE source_id = ?";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(ctx->db->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, source_id);
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    
    if (result_count == 0) {
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Allocate array
    int64_t *results = malloc(result_count * sizeof(int64_t));
    if (!results) {
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Second pass: collect results
    sqlite3_reset(stmt);
    size_t i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < result_count) {
        results[i++] = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    *count = result_count;
    return results;
}

variable_binding_set_t* copy_binding_set(variable_binding_set_t *original) {
    if (!original) return NULL;
    
    variable_binding_set_t *copy = create_binding_set();
    if (!copy) return NULL;
    
    for (size_t i = 0; i < original->binding_count; i++) {
        variable_binding_t *orig_binding = &original->bindings[i];
        
        // Deep copy the value to heap-allocated memory
        gql_value_t *copied_value = malloc(sizeof(gql_value_t));
        if (!copied_value) {
            destroy_binding_set_structure(copy);
            return NULL;
        }
        
        copied_value->type = orig_binding->value->type;
        
        switch (orig_binding->value->type) {
            case GQL_VALUE_STRING:
                copied_value->data.string = strdup(orig_binding->value->data.string);
                break;
            case GQL_VALUE_INTEGER:
                copied_value->data.integer = orig_binding->value->data.integer;
                break;
            case GQL_VALUE_BOOLEAN:
                copied_value->data.boolean = orig_binding->value->data.boolean;
                break;
            case GQL_VALUE_NODE:
                copied_value->data.node.id = orig_binding->value->data.node.id;
                copied_value->data.node.label_count = orig_binding->value->data.node.label_count;
                if (orig_binding->value->data.node.labels) {
                    copied_value->data.node.labels = malloc(copied_value->data.node.label_count * sizeof(char*));
                    for (size_t j = 0; j < copied_value->data.node.label_count; j++) {
                        copied_value->data.node.labels[j] = strdup(orig_binding->value->data.node.labels[j]);
                    }
                } else {
                    copied_value->data.node.labels = NULL;
                }
                // Note: Not copying properties for now
                copied_value->data.node.properties = NULL;
                break;
            case GQL_VALUE_EDGE:
                copied_value->data.edge.id = orig_binding->value->data.edge.id;
                copied_value->data.edge.source_id = orig_binding->value->data.edge.source_id;
                copied_value->data.edge.target_id = orig_binding->value->data.edge.target_id;
                copied_value->data.edge.type = orig_binding->value->data.edge.type ? 
                    strdup(orig_binding->value->data.edge.type) : NULL;
                copied_value->data.edge.properties = NULL;
                break;
            default:
                // For other types, just copy the basic data
                copied_value->data = orig_binding->value->data;
                break;
        }
        
        add_binding(copy, orig_binding->variable_name, copied_value);
    }
    
    return copy;
}

// ============================================================================
// Variable Binding Management
// ============================================================================

variable_binding_set_t* create_binding_set(void) {
    variable_binding_set_t *set = calloc(1, sizeof(variable_binding_set_t));
    if (!set) return NULL;
    
    set->bindings = NULL;
    set->binding_count = 0;
    set->binding_capacity = 0;
    
    return set;
}

void destroy_binding_set(variable_binding_set_t *set) {
    GQL_DEBUG("destroy_binding_set - starting");
    if (!set) {
        GQL_DEBUG("destroy_binding_set - set is NULL");
        return;
    }
    
    GQL_DEBUG("destroy_binding_set - destroying %zu bindings", set->binding_count);
    if (set->bindings) {
        GQL_DEBUG("destroy_binding_set - bindings array exists at %p", (void*)set->bindings);
        for (size_t i = 0; i < set->binding_count; i++) {
            GQL_DEBUG("destroy_binding_set - destroying binding %zu (%s)", 
                   i, set->bindings[i].variable_name ? set->bindings[i].variable_name : "NULL");
            free(set->bindings[i].variable_name);
            if (set->bindings[i].value) {
                GQL_DEBUG("destroy_binding_set - freeing value for binding %zu", i);
                gql_value_free(set->bindings[i].value);
                GQL_DEBUG("destroy_binding_set - value freed for binding %zu", i);
            }
        }
        GQL_DEBUG("destroy_binding_set - freeing bindings array");
        free(set->bindings);
        GQL_DEBUG("destroy_binding_set - bindings array freed");
    } else {
        GQL_DEBUG("destroy_binding_set - bindings array is NULL");
    }
    
    GQL_DEBUG("destroy_binding_set - completed");
}

void destroy_binding_set_structure(variable_binding_set_t *set) {
    destroy_binding_set(set);
    GQL_DEBUG("destroy_binding_set_structure - freeing set structure");
    free(set);
    GQL_DEBUG("destroy_binding_set_structure - completed");
}

int add_binding(variable_binding_set_t *set, const char *name, gql_value_t *value) {
    if (!set || !name || !value) return -1;
    
    // Check if binding already exists
    for (size_t i = 0; i < set->binding_count; i++) {
        if (strcmp(set->bindings[i].variable_name, name) == 0) {
            // Update existing binding
            if (set->bindings[i].value) {
                gql_value_free(set->bindings[i].value);
            }
            set->bindings[i].value = value;
            return 0;
        }
    }
    
    // Add new binding
    if (set->binding_count >= set->binding_capacity) {
        size_t new_capacity = set->binding_capacity == 0 ? 4 : set->binding_capacity * 2;
        variable_binding_t *new_bindings = realloc(set->bindings, 
                                                  new_capacity * sizeof(variable_binding_t));
        if (!new_bindings) return -1;
        
        set->bindings = new_bindings;
        set->binding_capacity = new_capacity;
    }
    
    set->bindings[set->binding_count].variable_name = strdup(name);
    set->bindings[set->binding_count].value = value;
    set->binding_count++;
    
    return 0;
}

gql_value_t* get_binding(variable_binding_set_t *set, const char *name) {
    GQL_DEBUG("get_binding - looking for '%s'", name ? name : "NULL");
    if (!set || !name) {
        GQL_DEBUG("get_binding - invalid parameters");
        return NULL;
    }
    
    GQL_DEBUG("get_binding - searching %zu bindings", set->binding_count);
    for (size_t i = 0; i < set->binding_count; i++) {
        GQL_DEBUG("get_binding - checking binding %zu: '%s'", i, set->bindings[i].variable_name);
        if (strcmp(set->bindings[i].variable_name, name) == 0) {
            GQL_DEBUG("get_binding - found match, value = %p", (void*)set->bindings[i].value);
            return set->bindings[i].value;
        }
    }
    
    GQL_DEBUG("get_binding - no match found");
    return NULL;
}

// ============================================================================
// Match Result Management
// ============================================================================

match_result_set_t* create_match_result_set(void) {
    match_result_set_t *results = calloc(1, sizeof(match_result_set_t));
    if (!results) return NULL;
    
    results->result_sets = NULL;
    results->result_count = 0;
    results->result_capacity = 0;
    results->status = MATCH_RESULT_SUCCESS;
    results->error_message = NULL;
    
    return results;
}

void destroy_match_result_set(match_result_set_t *results) {
    GQL_DEBUG("destroy_match_result_set - starting");
    if (!results) {
        GQL_DEBUG("destroy_match_result_set - results is NULL");
        return;
    }
    
    GQL_DEBUG("destroy_match_result_set - destroying %zu result sets", results->result_count);
    if (results->result_sets) {
        for (size_t i = 0; i < results->result_count; i++) {
            GQL_DEBUG("destroy_match_result_set - destroying result set %zu", i);
            destroy_binding_set(&results->result_sets[i]); // Only free contents, not structure
            GQL_DEBUG("destroy_match_result_set - result set %zu destroyed", i);
        }
        GQL_DEBUG("destroy_match_result_set - freeing result_sets array");
        free(results->result_sets); // Free the whole array
    }
    
    GQL_DEBUG("destroy_match_result_set - freeing error message");
    free(results->error_message);
    GQL_DEBUG("destroy_match_result_set - freeing results structure");
    free(results);
    GQL_DEBUG("destroy_match_result_set - completed");
}

int add_match_result(match_result_set_t *results, variable_binding_set_t *bindings) {
    if (!results || !bindings) return -1;
    
    if (results->result_count >= results->result_capacity) {
        size_t new_capacity = results->result_capacity == 0 ? 4 : results->result_capacity * 2;
        variable_binding_set_t *new_results = realloc(results->result_sets,
                                                     new_capacity * sizeof(variable_binding_set_t));
        if (!new_results) return -1;
        
        results->result_sets = new_results;
        results->result_capacity = new_capacity;
    }
    
    // Transfer ownership properly - move the contents, then clear the source
    results->result_sets[results->result_count] = *bindings;
    results->result_count++;
    
    // Clear the source binding set to prevent double-free
    // The caller should not use the binding set after this call
    bindings->bindings = NULL;
    bindings->binding_count = 0;
    bindings->binding_capacity = 0;
    
    return 0;
}

void set_match_error(match_result_set_t *results, const char *message) {
    if (!results) return;
    
    results->status = MATCH_RESULT_ERROR;
    free(results->error_message);
    results->error_message = message ? strdup(message) : NULL;
}

// ============================================================================
// Node Matching Functions
// ============================================================================

int64_t* find_all_nodes(gql_execution_context_t *ctx, size_t *count) {
    if (!ctx || !ctx->db || !count) {
        GQL_DEBUG("find_all_nodes - invalid parameters");
        return NULL;
    }
    
    *count = 0;
    
    // Simple implementation: query the nodes table directly
    const char *sql = "SELECT id FROM nodes ORDER BY id";
    GQL_DEBUG("find_all_nodes - preparing SQL: %s", sql);
    
    // Use sqlite3_prepare_v2 directly to avoid statement caching issues
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(ctx->db->sqlite_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK || !stmt) {
        GQL_DEBUG("find_all_nodes - prepare failed: %s", sqlite3_errmsg(ctx->db->sqlite_db));
        return NULL;
    }
    
    GQL_DEBUG("find_all_nodes - starting first pass (count)");
    
    // First pass: count nodes
    size_t node_count = 0;
    int step_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        node_count++;
        step_count++;
        if (step_count % 1000 == 0) {
            GQL_DEBUG("find_all_nodes - counted %d rows so far", step_count);
        }
        if (step_count > 10000) {
            GQL_DEBUG("find_all_nodes - BREAKING: too many steps in first pass");
            break;
        }
    }
    GQL_DEBUG("find_all_nodes - first pass complete, found %zu nodes", node_count);
    
    int reset_rc = sqlite3_reset(stmt);
    GQL_DEBUG("find_all_nodes - reset result: %d", reset_rc);
    
    if (node_count == 0) {
        GQL_DEBUG("find_all_nodes - no nodes found, returning NULL");
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Allocate result array
    GQL_DEBUG("find_all_nodes - allocating array for %zu nodes", node_count);
    int64_t *node_ids = malloc(node_count * sizeof(int64_t));
    if (!node_ids) {
        GQL_DEBUG("find_all_nodes - malloc failed");
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Second pass: collect node IDs
    GQL_DEBUG("find_all_nodes - starting second pass (collect)");
    size_t index = 0;
    step_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < node_count) {
        node_ids[index++] = sqlite3_column_int64(stmt, 0);
        step_count++;
        if (step_count % 1000 == 0) {
            GQL_DEBUG("find_all_nodes - collected %d rows so far", step_count);
        }
        if (step_count > 10000) {
            GQL_DEBUG("find_all_nodes - BREAKING: too many steps in second pass");
            break;
        }
    }
    
    GQL_DEBUG("find_all_nodes - second pass complete, collected %zu nodes", index);
    sqlite3_finalize(stmt);
    
    *count = index;
    GQL_DEBUG("find_all_nodes - returning %zu nodes", *count);
    return node_ids;
}

int64_t* find_nodes_by_label(gql_execution_context_t *ctx, const char *label, size_t *count) {
    if (!ctx || !ctx->db || !label || !count) return NULL;
    
    return graphqlite_find_nodes_by_label(ctx->db, label, count);
}

gql_value_t* load_node_value(gql_execution_context_t *ctx, int64_t node_id) {
    GQL_DEBUG("load_node_value - starting for node %lld", (long long)node_id);
    if (!ctx || !ctx->db || node_id <= 0) {
        GQL_DEBUG("load_node_value - invalid parameters");
        return NULL;
    }
    
    // Get node labels
    GQL_DEBUG("load_node_value - getting labels for node %lld", (long long)node_id);
    size_t label_count = 0;
    char **labels = graphqlite_get_node_labels(ctx->db, node_id, &label_count);
    GQL_DEBUG("load_node_value - found %zu labels", label_count);
    
    // For now, create a simple node value without properties
    GQL_DEBUG("load_node_value - creating node value");
    gql_value_t *value = gql_value_create_node(node_id, labels, label_count, NULL);
    GQL_DEBUG("load_node_value - node value created: %p", (void*)value);
    
    return value;
}

bool node_matches_properties(gql_execution_context_t *ctx, int64_t node_id, 
                            gql_ast_node_t *property_map) {
    // For now, assume all nodes match if no property constraints
    // TODO: Implement actual property matching
    (void)ctx;
    (void)node_id;
    (void)property_map;
    return true;
}

// ============================================================================
// Pattern Matching Implementation
// ============================================================================

match_result_set_t* match_node_pattern(gql_execution_context_t *ctx, gql_ast_node_t *node_pattern) {
    GQL_DEBUG("match_node_pattern - starting");
    match_result_set_t *results = create_match_result_set();
    if (!results) return NULL;
    
    if (!node_pattern || node_pattern->type != GQL_AST_NODE_PATTERN) {
        GQL_DEBUG("match_node_pattern - invalid node pattern");
        set_match_error(results, "Invalid node pattern");
        return results;
    }
    
    // Find matching nodes
    int64_t *node_ids = NULL;
    size_t node_count = 0;
    
    gql_ast_node_t *labels = node_pattern->data.node_pattern.labels;
    GQL_DEBUG("match_node_pattern - labels: %s", labels ? "present" : "(no labels)");
    
    if (labels) {
        // For multiple labels, we need to find nodes that have ALL specified labels
        // Start with the first label to get initial candidates
        // Note: labels is a list container, actual labels start at labels->next
        gql_ast_node_t *first_label = labels->next;
        if (first_label && first_label->type == GQL_AST_STRING_LITERAL) {
            GQL_DEBUG("match_node_pattern - calling find_nodes_by_label for: %s", first_label->data.string_literal.value);
            node_ids = find_nodes_by_label(ctx, first_label->data.string_literal.value, &node_count);
            
            // Filter nodes that don't have all other required labels
            if (node_ids && first_label->next) {
                size_t filtered_count = 0;
                for (size_t i = 0; i < node_count; i++) {
                    bool has_all_labels = true;
                    
                    // Check if this node has all required labels
                    gql_ast_node_t *label_node = labels->next;
                    while (label_node && has_all_labels) {
                        if (label_node->type == GQL_AST_STRING_LITERAL) {
                            size_t node_label_count = 0;
                            char **node_labels = graphqlite_get_node_labels(ctx->db, node_ids[i], &node_label_count);
                            bool has_this_label = false;
                            
                            if (node_labels) {
                                for (size_t j = 0; j < node_label_count; j++) {
                                    if (strcmp(node_labels[j], label_node->data.string_literal.value) == 0) {
                                        has_this_label = true;
                                        break;
                                    }
                                }
                                // Free the node labels
                                for (size_t j = 0; j < node_label_count; j++) {
                                    free(node_labels[j]);
                                }
                                free(node_labels);
                            }
                            
                            if (!has_this_label) {
                                has_all_labels = false;
                            }
                        }
                        label_node = label_node->next;
                    }
                    
                    if (has_all_labels) {
                        node_ids[filtered_count++] = node_ids[i];
                    }
                }
                node_count = filtered_count;
            }
        } else {
            GQL_DEBUG("match_node_pattern - calling find_all_nodes (empty labels list)");
            node_ids = find_all_nodes(ctx, &node_count);
        }
    } else {
        GQL_DEBUG("match_node_pattern - calling find_all_nodes");
        node_ids = find_all_nodes(ctx, &node_count);
    }
    
    GQL_DEBUG("match_node_pattern - found %zu nodes", node_count);
    
    if (!node_ids || node_count == 0) {
        results->status = MATCH_RESULT_NO_MATCHES;
        free(node_ids);
        return results;
    }
    
    // Create binding sets for each matching node
    const char *variable = node_pattern->data.node_pattern.variable;
    GQL_DEBUG("match_node_pattern - variable: %s", variable ? variable : "(no variable)");
    
    for (size_t i = 0; i < node_count; i++) {
        GQL_DEBUG("match_node_pattern - processing node %zu of %zu (id=%lld)", 
               i+1, node_count, (long long)node_ids[i]);
        
        // Check property constraints
        if (!node_matches_properties(ctx, node_ids[i], node_pattern->data.node_pattern.properties)) {
            GQL_DEBUG("match_node_pattern - node %lld skipped (property constraints)", 
                   (long long)node_ids[i]);
            continue;
        }
        
        GQL_DEBUG("match_node_pattern - creating binding set for node %lld", 
               (long long)node_ids[i]);
        variable_binding_set_t *bindings = create_binding_set();
        if (!bindings) {
            GQL_DEBUG("match_node_pattern - failed to create binding set");
            continue;
        }
        
        // Add variable binding if variable name is specified
        if (variable) {
            GQL_DEBUG("match_node_pattern - loading node value for variable %s", variable);
            gql_value_t *node_value = load_node_value(ctx, node_ids[i]);
            if (node_value) {
                GQL_DEBUG("match_node_pattern - adding binding %s = node %lld", 
                       variable, (long long)node_ids[i]);
                add_binding(bindings, variable, node_value);
            } else {
                GQL_DEBUG("match_node_pattern - failed to load node value");
            }
        }
        
        GQL_DEBUG("match_node_pattern - adding result to match set");
        add_match_result(results, bindings);
        // Note: add_match_result now owns the bindings content, but we still need to free the structure
        free(bindings);
    }
    
    free(node_ids);
    
    if (results->result_count == 0) {
        results->status = MATCH_RESULT_NO_MATCHES;
    }
    
    return results;
}

gql_value_t* load_edge_value(gql_execution_context_t *ctx, int64_t edge_id) {
    GQL_DEBUG("load_edge_value - starting for edge %lld", (long long)edge_id);
    if (!ctx || !ctx->db || edge_id <= 0) {
        GQL_DEBUG("load_edge_value - invalid parameters");
        return NULL;
    }
    
    // Get edge information
    const char *sql = "SELECT id, source_id, target_id, type FROM edges WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(ctx->db->sqlite_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK || !stmt) {
        GQL_DEBUG("load_edge_value - prepare failed: %s", sqlite3_errmsg(ctx->db->sqlite_db));
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, edge_id);
    
    gql_value_t *value = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t source_id = sqlite3_column_int64(stmt, 1);
        int64_t target_id = sqlite3_column_int64(stmt, 2);
        const char *type = (const char*)sqlite3_column_text(stmt, 3);
        
        GQL_DEBUG("load_edge_value - creating edge value (source=%lld, target=%lld, type=%s)", 
               (long long)source_id, (long long)target_id, type ? type : "NULL");
        value = gql_value_create_edge(edge_id, source_id, target_id, type, NULL);
    }
    
    sqlite3_finalize(stmt);
    GQL_DEBUG("load_edge_value - returning %p", (void*)value);
    return value;
}

match_result_set_t* match_edge_pattern(gql_execution_context_t *ctx, 
                                      gql_ast_node_t *source_node,
                                      gql_ast_node_t *edge_pattern,
                                      gql_ast_node_t *target_node) {
    GQL_DEBUG("match_edge_pattern - starting");
    match_result_set_t *results = create_match_result_set();
    if (!results) return NULL;
    
    if (!ctx || !source_node || !target_node) {
        set_match_error(results, "Invalid edge pattern parameters");
        return results;
    }
    
    // First, match source nodes
    GQL_DEBUG("match_edge_pattern - matching source nodes");
    match_result_set_t *source_matches = match_node_pattern(ctx, source_node);
    if (!source_matches || source_matches->status != MATCH_RESULT_SUCCESS || source_matches->result_count == 0) {
        GQL_DEBUG("match_edge_pattern - no source nodes matched");
        if (source_matches) {
            destroy_match_result_set(source_matches);
        }
        results->status = MATCH_RESULT_NO_MATCHES;
        return results;
    }
    
    GQL_DEBUG("match_edge_pattern - found %zu source nodes", source_matches->result_count);
    
    // Extract edge constraints
    const char *edge_type = NULL;
    const char *edge_variable = NULL;
    bool directed = true; // Default to directed
    
    if (edge_pattern && edge_pattern->type == GQL_AST_EDGE_PATTERN) {
        edge_type = edge_pattern->data.edge_pattern.type;
        edge_variable = edge_pattern->data.edge_pattern.variable;
        directed = edge_pattern->data.edge_pattern.directed;
    }
    
    GQL_DEBUG("match_edge_pattern - edge constraints: type=%s, variable=%s, directed=%d",
           edge_type ? edge_type : "(any)", edge_variable ? edge_variable : "(none)", directed);
    
    // For each source node, find connected target nodes
    for (size_t i = 0; i < source_matches->result_count; i++) {
        variable_binding_set_t *source_bindings = &source_matches->result_sets[i];
        
        // Get the source node value
        const char *source_var = source_node->data.node_pattern.variable;
        gql_value_t *source_value = source_var ? get_binding(source_bindings, source_var) : NULL;
        
        if (!source_value || source_value->type != GQL_VALUE_NODE) {
            GQL_DEBUG("match_edge_pattern - no source node value for binding %zu", i);
            continue;
        }
        
        int64_t source_id = source_value->data.node.id;
        GQL_DEBUG("match_edge_pattern - processing source node %lld", (long long)source_id);
        
        // Find outgoing edges from this source node
        size_t edge_count = 0;
        int64_t *edge_ids = graphqlite_get_outgoing_edges(ctx->db, source_id, edge_type, &edge_count);
        
        GQL_DEBUG("match_edge_pattern - found %zu outgoing edges", edge_count);
        
        if (!edge_ids || edge_count == 0) {
            free(edge_ids);
            continue;
        }
        
        // For each edge, check if target node matches the target pattern
        for (size_t j = 0; j < edge_count; j++) {
            int64_t edge_id = edge_ids[j];
            GQL_DEBUG("match_edge_pattern - processing edge %lld", (long long)edge_id);
            
            // Get target node ID from edge
            int64_t target_id = graphqlite_get_edge_target(ctx->db, edge_id);
            if (target_id <= 0) {
                GQL_DEBUG("match_edge_pattern - invalid target for edge %lld", (long long)edge_id);
                continue;
            }
            
            GQL_DEBUG("match_edge_pattern - target node is %lld", (long long)target_id);
            
            // Check if target node matches the target pattern constraints
            gql_ast_node_t *target_labels = target_node->data.node_pattern.labels;
            bool target_matches = true;
            
            if (target_labels) {
                GQL_DEBUG("match_edge_pattern - checking target label constraints");
                
                // Check that target node has ALL required labels
                // Start from the first label, not target_labels->next
                gql_ast_node_t *label_node = target_labels;
                while (label_node && target_matches) {
                    if (label_node->type == GQL_AST_STRING_LITERAL) {
                        GQL_DEBUG("match_edge_pattern - checking target label: %s", label_node->data.string_literal.value);
                        size_t label_count = 0;
                        char **labels = graphqlite_get_node_labels(ctx->db, target_id, &label_count);
                        bool has_this_label = false;
                        
                        if (labels) {
                            for (size_t k = 0; k < label_count; k++) {
                                if (strcmp(labels[k], label_node->data.string_literal.value) == 0) {
                                    has_this_label = true;
                                    break;
                                }
                            }
                            graphqlite_free_labels(labels, label_count);
                        }
                        
                        if (!has_this_label) {
                            target_matches = false;
                            GQL_DEBUG("match_edge_pattern - target node %lld doesn't have label %s", 
                                   (long long)target_id, label_node->data.string_literal.value);
                        }
                    }
                    label_node = label_node->next;
                }
            }
            
            if (target_matches) {
                GQL_DEBUG("match_edge_pattern - target node %lld matches constraints", (long long)target_id);
            } else {
                GQL_DEBUG("match_edge_pattern - target node %lld does not match constraints", (long long)target_id);
                continue;
            }
            
            // Create a new binding set with source, edge, and target
            variable_binding_set_t *new_bindings = copy_binding_set(source_bindings);
            if (!new_bindings) {
                GQL_DEBUG("match_edge_pattern - failed to copy source bindings");
                continue;
            }
            
            // Add edge binding if edge variable is specified
            if (edge_variable) {
                GQL_DEBUG("match_edge_pattern - binding edge variable %s", edge_variable);
                gql_value_t *edge_value = load_edge_value(ctx, edge_id);
                if (edge_value) {
                    add_binding(new_bindings, edge_variable, edge_value);
                }
            }
            
            // Add target node binding if target variable is specified
            const char *target_variable = target_node->data.node_pattern.variable;
            if (target_variable) {
                GQL_DEBUG("match_edge_pattern - binding target variable %s", target_variable);
                gql_value_t *target_value = load_node_value(ctx, target_id);
                if (target_value) {
                    add_binding(new_bindings, target_variable, target_value);
                }
            }
            
            GQL_DEBUG("match_edge_pattern - adding result to match set");
            add_match_result(results, new_bindings);
            // Note: add_match_result takes ownership of the contents, we still need to free the structure
            free(new_bindings);
        }
        
        free(edge_ids);
    }
    
    destroy_match_result_set(source_matches);
    
    if (results->result_count == 0) {
        results->status = MATCH_RESULT_NO_MATCHES;
    }
    
    GQL_DEBUG("match_edge_pattern - completed with %zu results", results->result_count);
    return results;
}

match_result_set_t* match_single_pattern(gql_execution_context_t *ctx, gql_ast_node_t *pattern) {
    if (!pattern) return NULL;
    
    switch (pattern->type) {
        case GQL_AST_NODE_PATTERN:
            return match_node_pattern(ctx, pattern);
            
        case GQL_AST_PATTERN:
        {
            // Check if this is a multi-hop pattern (source is also a pattern)
            gql_ast_node_t *source_node = pattern->data.pattern.node;
            gql_ast_node_t *target_node = pattern->data.pattern.target_node;
            bool is_multi_hop = (source_node && source_node->type == GQL_AST_PATTERN);
            
            if (is_multi_hop) {
                
                // Flatten the nested pattern structure
                multi_hop_pattern_t *multi_hop = flatten_pattern(pattern);
                if (!multi_hop) {
                    GQL_DEBUG("match_single_pattern - failed to flatten multi-hop pattern");
                    return NULL;
                }
                
                // Process the multi-hop pattern
                match_result_set_t *results = match_multi_hop_pattern(ctx, multi_hop);
                
                // Clean up
                destroy_multi_hop_pattern(multi_hop);
                
                return results;
            }
            
            // Handle simple single-hop pattern: (a)-[r]->(b)
            gql_ast_node_t *edge = pattern->data.pattern.edge;
            // source_node and target_node already declared above
            
            if (!source_node || !target_node) {
                match_result_set_t *results = create_match_result_set();
                if (results) {
                    set_match_error(results, "Invalid pattern structure");
                }
                return results;
            }
            
            return match_edge_pattern(ctx, source_node, edge, target_node);
        }
        
        default:
        {
            match_result_set_t *results = create_match_result_set();
            if (results) {
                set_match_error(results, "Unsupported pattern type");
            }
            return results;
        }
    }
}

match_result_set_t* match_patterns(gql_execution_context_t *ctx, gql_ast_node_t *patterns) {
    GQL_DEBUG("match_patterns - starting");
    if (!ctx || !patterns) {
        GQL_DEBUG("match_patterns - invalid context or patterns");
        match_result_set_t *results = create_match_result_set();
        if (results) {
            set_match_error(results, "Invalid context or patterns");
        }
        return results;
    }
    
    // For now, handle single pattern only
    gql_ast_node_t *first_pattern = patterns->next;
    if (!first_pattern) {
        GQL_DEBUG("match_patterns - no patterns to match");
        match_result_set_t *results = create_match_result_set();
        if (results) {
            set_match_error(results, "No patterns to match");
        }
        return results;
    }
    
    GQL_DEBUG("match_patterns - calling match_single_pattern");
    return match_single_pattern(ctx, first_pattern);
}

// ============================================================================
// WHERE Clause Evaluation
// ============================================================================

gql_value_t evaluate_expression_with_bindings(gql_execution_context_t *ctx,
                                             gql_ast_node_t *expr,
                                             variable_binding_set_t *bindings) {
    gql_value_t null_value = {.type = GQL_VALUE_NULL};
    
    if (!ctx || !expr) return null_value;
    
    switch (expr->type) {
        case GQL_AST_INTEGER_LITERAL:
        case GQL_AST_STRING_LITERAL:
        case GQL_AST_BOOLEAN_LITERAL:
        case GQL_AST_NULL_LITERAL:
            // Evaluate literals normally
            return evaluate_expression(ctx, expr);
            
        case GQL_AST_IDENTIFIER:
        {
            const char *name = expr->data.identifier.name;
            GQL_DEBUG("evaluate_expression_with_bindings - looking for identifier '%s'", name ? name : "NULL");
            if (bindings) {
                gql_value_t *bound_value = get_binding(bindings, name);
                GQL_DEBUG("evaluate_expression_with_bindings - bound_value = %p", (void*)bound_value);
                if (bound_value) {
                    GQL_DEBUG("evaluate_expression_with_bindings - bound value type = %d", bound_value->type);
                    // Return a deep copy to avoid sharing pointers
                    gql_value_t copy = *bound_value;
                    
                    // Deep copy complex data to avoid double-free issues
                    switch (bound_value->type) {
                        case GQL_VALUE_STRING:
                            if (bound_value->data.string) {
                                copy.data.string = strdup(bound_value->data.string);
                            }
                            break;
                            
                        case GQL_VALUE_NODE:
                            // Deep copy labels array
                            if (bound_value->data.node.labels && bound_value->data.node.label_count > 0) {
                                char **new_labels = malloc(bound_value->data.node.label_count * sizeof(char*));
                                if (new_labels) {
                                    for (size_t j = 0; j < bound_value->data.node.label_count; j++) {
                                        new_labels[j] = strdup(bound_value->data.node.labels[j]);
                                    }
                                    copy.data.node.labels = new_labels;
                                }
                            }
                            break;
                            
                        case GQL_VALUE_EDGE:
                            // Deep copy edge type
                            if (bound_value->data.edge.type) {
                                copy.data.edge.type = strdup(bound_value->data.edge.type);
                            }
                            break;
                            
                        default:
                            // For simple types, shallow copy is sufficient
                            break;
                    }
                    
                    return copy;
                }
            }
            // Fall back to context variables
            return evaluate_expression(ctx, expr);
        }
        
        case GQL_AST_PROPERTY_ACCESS:
        {
            const char *object = expr->data.property_access.object;
            const char *property = expr->data.property_access.property;
            
            GQL_DEBUG("property access: %s.%s", object ? object : "NULL", property ? property : "NULL");
            
            if (bindings && object) {
                gql_value_t *obj_value = get_binding(bindings, object);
                GQL_DEBUG("property access: obj_value=%p, type=%d", (void*)obj_value, obj_value ? obj_value->type : -1);
                if (obj_value && obj_value->type == GQL_VALUE_NODE) {
                    // Load property from node
                    property_value_t prop_value;
                    int result = graphqlite_get_property(ctx->db, ENTITY_NODE,
                                                       obj_value->data.node.id,
                                                       property, &prop_value);
                    
                    GQL_DEBUG("property load: node_id=%lld, property=%s, result=%d", 
                             (long long)obj_value->data.node.id, property ? property : "NULL", result);
                    
                    if (result == SQLITE_OK) {
                        gql_value_t value;
                        GQL_DEBUG("property loaded successfully, prop_value.type=%d", prop_value.type);
                        switch (prop_value.type) {
                            case PROP_INT:
                                value.type = GQL_VALUE_INTEGER;
                                value.data.integer = prop_value.value.int_val;
                                GQL_DEBUG("converted to integer: %lld", (long long)value.data.integer);
                                break;
                            case PROP_TEXT:
                                value.type = GQL_VALUE_STRING;
                                value.data.string = strdup(prop_value.value.text_val ? 
                                                          prop_value.value.text_val : "");
                                GQL_DEBUG("converted to string: '%s'", value.data.string ? value.data.string : "NULL");
                                free_property_value(&prop_value);
                                break;
                            case PROP_BOOL:
                                value.type = GQL_VALUE_BOOLEAN;
                                value.data.boolean = prop_value.value.bool_val;
                                break;
                            default:
                                value = null_value;
                                break;
                        }
                        GQL_DEBUG("property access returning value type=%d", value.type);
                        return value;
                    } else {
                        GQL_DEBUG("property load failed, result=%d", result);
                    }
                } else {
                    GQL_DEBUG("property access: obj_value not found or not a node");
                }
            } else {
                GQL_DEBUG("property access: no bindings or no object");
            }
            return null_value;
        }
        
        case GQL_AST_UNARY_EXPR:
        {
            gql_value_t operand = evaluate_expression_with_bindings(ctx, expr->data.unary_expr.operand, bindings);
            gql_value_t result = {.type = GQL_VALUE_BOOLEAN};
            
            switch (expr->data.unary_expr.operator) {
                case GQL_OP_NOT:
                    if (operand.type == GQL_VALUE_BOOLEAN) {
                        result.data.boolean = !operand.data.boolean;
                    } else {
                        result.data.boolean = false;
                    }
                    break;
                    
                case GQL_OP_IS_NULL:
                    result.data.boolean = (operand.type == GQL_VALUE_NULL);
                    break;
                    
                case GQL_OP_IS_NOT_NULL:
                    result.data.boolean = (operand.type != GQL_VALUE_NULL);
                    break;
                    
                default:
                    return null_value;
            }
            
            // Clean up temporary values
            if (operand.type == GQL_VALUE_STRING && operand.data.string) {
                free(operand.data.string);
            }
            
            return result;
        }
        
        case GQL_AST_BINARY_EXPR:
        {
            gql_value_t left = evaluate_expression_with_bindings(ctx, expr->data.binary_expr.left, bindings);
            gql_value_t right = evaluate_expression_with_bindings(ctx, expr->data.binary_expr.right, bindings);
            
            gql_value_t result = {.type = GQL_VALUE_BOOLEAN};
            
            switch (expr->data.binary_expr.operator) {
                case GQL_OP_EQUALS:
                    result.data.boolean = gql_values_equal(&left, &right);
                    break;
                    
                case GQL_OP_NOT_EQUALS:
                    result.data.boolean = !gql_values_equal(&left, &right);
                    break;
                    
                case GQL_OP_LESS_THAN:
                    result.data.boolean = gql_value_compare(&left, &right) < 0;
                    break;
                    
                case GQL_OP_LESS_EQUAL:
                    result.data.boolean = gql_value_compare(&left, &right) <= 0;
                    break;
                    
                case GQL_OP_GREATER_THAN:
                    result.data.boolean = gql_value_compare(&left, &right) > 0;
                    break;
                    
                case GQL_OP_GREATER_EQUAL:
                    result.data.boolean = gql_value_compare(&left, &right) >= 0;
                    break;
                    
                case GQL_OP_AND:
                    if (left.type == GQL_VALUE_BOOLEAN && right.type == GQL_VALUE_BOOLEAN) {
                        result.data.boolean = left.data.boolean && right.data.boolean;
                    } else {
                        result.data.boolean = false;
                    }
                    break;
                    
                case GQL_OP_OR:
                    if (left.type == GQL_VALUE_BOOLEAN && right.type == GQL_VALUE_BOOLEAN) {
                        result.data.boolean = left.data.boolean || right.data.boolean;
                    } else {
                        result.data.boolean = false;
                    }
                    break;
                    
                case GQL_OP_CONTAINS:
                    if (left.type == GQL_VALUE_STRING && right.type == GQL_VALUE_STRING) {
                        result.data.boolean = strstr(left.data.string, right.data.string) != NULL;
                    } else {
                        result.data.boolean = false;
                    }
                    break;
                    
                case GQL_OP_STARTS_WITH:
                    if (left.type == GQL_VALUE_STRING && right.type == GQL_VALUE_STRING) {
                        result.data.boolean = strncmp(left.data.string, right.data.string, 
                                                     strlen(right.data.string)) == 0;
                    } else {
                        result.data.boolean = false;
                    }
                    break;
                    
                case GQL_OP_ENDS_WITH:
                    if (left.type == GQL_VALUE_STRING && right.type == GQL_VALUE_STRING) {
                        size_t left_len = strlen(left.data.string);
                        size_t right_len = strlen(right.data.string);
                        if (left_len >= right_len) {
                            result.data.boolean = strcmp(left.data.string + left_len - right_len, 
                                                       right.data.string) == 0;
                        } else {
                            result.data.boolean = false;
                        }
                    } else {
                        result.data.boolean = false;
                    }
                    break;
                    
                default:
                    return null_value;
            }
            
            // Clean up temporary string values from evaluate_expression_with_bindings
            if (left.type == GQL_VALUE_STRING && left.data.string) {
                free(left.data.string);
            }
            if (right.type == GQL_VALUE_STRING && right.data.string) {
                free(right.data.string);
            }
            
            return result;
        }
        
        default:
            return null_value;
    }
}

match_result_set_t* apply_where_filter(gql_execution_context_t *ctx, 
                                      match_result_set_t *matches,
                                      gql_ast_node_t *where_clause) {
    if (!matches || !where_clause || matches->status != MATCH_RESULT_SUCCESS) {
        return matches;
    }
    
    match_result_set_t *filtered = create_match_result_set();
    if (!filtered) return matches;
    
    gql_ast_node_t *condition = where_clause->data.where_clause.expression;
    
    for (size_t i = 0; i < matches->result_count; i++) {
        gql_value_t result = evaluate_expression_with_bindings(ctx, condition, &matches->result_sets[i]);
        
        if (result.type == GQL_VALUE_BOOLEAN && result.data.boolean) {
            variable_binding_set_t *copy = copy_binding_set(&matches->result_sets[i]);
            if (copy) {
                add_match_result(filtered, copy);
            }
        }
    }
    
    if (filtered->result_count == 0) {
        filtered->status = MATCH_RESULT_NO_MATCHES;
    }
    
    return filtered;
}

// ============================================================================
// RETURN Clause Processing
// ============================================================================

gql_result_t* project_match_results(gql_execution_context_t *ctx,
                                   match_result_set_t *matches,
                                   gql_ast_node_t *return_clause) {
    GQL_DEBUG("project_match_results - starting");
    gql_result_t *result = gql_result_create();
    if (!result) return NULL;
    
    if (!matches || matches->status != MATCH_RESULT_SUCCESS || matches->result_count == 0) {
        GQL_DEBUG("project_match_results - no matches or empty result");
        result->status = GQL_RESULT_EMPTY;
        return result;
    }
    
    GQL_DEBUG("project_match_results - %zu matches to process", matches->result_count);
    
    if (!return_clause || return_clause->type != GQL_AST_RETURN_CLAUSE) {
        gql_result_set_error(result, "Invalid RETURN clause");
        return result;
    }
    
    // Process return items
    gql_ast_node_t *items = return_clause->data.return_clause.items;
    if (!items || !items->next) {
        gql_result_set_error(result, "No return items specified");
        return result;
    }
    
    // Count return items and add columns
    gql_ast_node_t *item = items->next;
    size_t column_count = 0;
    while (item) {
        char column_name[64];
        
        if (item->type == GQL_AST_RETURN_ITEM) {
            // Use alias if available, otherwise generate from expression
            if (item->data.return_item.alias) {
                snprintf(column_name, sizeof(column_name), "%s", item->data.return_item.alias);
            } else {
                // Generate column name from expression
                gql_ast_node_t *expr = item->data.return_item.expression;
                if (expr->type == GQL_AST_IDENTIFIER) {
                    snprintf(column_name, sizeof(column_name), "%s", 
                            expr->data.identifier.name ? expr->data.identifier.name : "unknown");
                } else if (expr->type == GQL_AST_PROPERTY_ACCESS) {
                    snprintf(column_name, sizeof(column_name), "%s.%s",
                            expr->data.property_access.object ? expr->data.property_access.object : "?",
                            expr->data.property_access.property ? expr->data.property_access.property : "?");
                } else {
                    snprintf(column_name, sizeof(column_name), "expr_%zu", column_count);
                }
            }
        } else {
            // Legacy support for direct expressions (fallback)
            if (item->type == GQL_AST_IDENTIFIER) {
                snprintf(column_name, sizeof(column_name), "%s", 
                        item->data.identifier.name ? item->data.identifier.name : "unknown");
            } else if (item->type == GQL_AST_PROPERTY_ACCESS) {
                snprintf(column_name, sizeof(column_name), "%s.%s",
                        item->data.property_access.object ? item->data.property_access.object : "?",
                        item->data.property_access.property ? item->data.property_access.property : "?");
            } else {
                snprintf(column_name, sizeof(column_name), "expr_%zu", column_count);
            }
        }
        
        gql_result_add_column(result, column_name);
        column_count++;
        item = item->next;
    }
    
    // Generate result rows
    GQL_DEBUG("project_match_results - generating %zu result rows with %zu columns", 
           matches->result_count, column_count);
    for (size_t i = 0; i < matches->result_count; i++) {
        GQL_DEBUG("project_match_results - processing row %zu", i);
        gql_value_t *row_values = malloc(column_count * sizeof(gql_value_t));
        if (!row_values) {
            GQL_DEBUG("project_match_results - malloc failed for row %zu", i);
            continue;
        }
        
        item = items->next;
        for (size_t j = 0; j < column_count && item; j++) {
            GQL_DEBUG("project_match_results - evaluating column %zu", j);
            
            // Handle return items vs direct expressions
            if (item->type == GQL_AST_RETURN_ITEM) {
                GQL_DEBUG("evaluating RETURN_ITEM, expression type=%d", item->data.return_item.expression ? item->data.return_item.expression->type : -1);
                row_values[j] = evaluate_expression_with_bindings(ctx, item->data.return_item.expression, &matches->result_sets[i]);
            } else {
                GQL_DEBUG("evaluating direct expression, type=%d", item->type);
                // Legacy support for direct expressions
                row_values[j] = evaluate_expression_with_bindings(ctx, item, &matches->result_sets[i]);
            }
            
            GQL_DEBUG("project_match_results - column %zu evaluated", j);
            item = item->next;
        }
        
        GQL_DEBUG("project_match_results - adding row %zu to result", i);
        gql_result_add_row(result, row_values, column_count);
        GQL_DEBUG("project_match_results - row %zu added", i);
        
        // Clean up temporary values - we created deep copies in evaluate_expression_with_bindings
        for (size_t j = 0; j < column_count; j++) {
            // Free the contents of temporary values since we made deep copies
            gql_value_free_contents(&row_values[j]);
        }
        
        free(row_values);
    }
    
    GQL_DEBUG("project_match_results - all rows processed, returning result");
    
    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

void print_binding_set(variable_binding_set_t *bindings) {
    if (!bindings) {
        printf("NULL bindings\n");
        return;
    }
    
    printf("Bindings (%zu):\n", bindings->binding_count);
    for (size_t i = 0; i < bindings->binding_count; i++) {
        printf("  %s = ", bindings->bindings[i].variable_name);
        if (bindings->bindings[i].value) {
            char *str = gql_value_to_string(bindings->bindings[i].value);
            printf("%s\n", str);
            free(str);
        } else {
            printf("NULL\n");
        }
    }
}

void print_match_results(match_result_set_t *results) {
    if (!results) {
        printf("NULL results\n");
        return;
    }
    
    printf("Match Results: %zu result(s), status=%d\n", 
           results->result_count, results->status);
    
    if (results->error_message) {
        printf("Error: %s\n", results->error_message);
    }
    
    for (size_t i = 0; i < results->result_count; i++) {
        printf("Result %zu:\n", i);
        print_binding_set(&results->result_sets[i]);
    }
}

// ============================================================================
// Multi-hop Pattern Matching
// ============================================================================

// Forward declarations
static hop_step_t* create_hop_step(gql_ast_node_t *source, gql_ast_node_t *edge, gql_ast_node_t *target);
static multi_hop_pattern_t* flatten_pattern(gql_ast_node_t *pattern);
static void destroy_multi_hop_pattern(multi_hop_pattern_t *pattern);
static match_result_set_t* match_multi_hop_pattern(gql_execution_context_t *ctx, multi_hop_pattern_t *pattern);