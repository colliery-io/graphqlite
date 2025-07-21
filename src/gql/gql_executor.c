#include "gql_executor.h"
#include "gql_parser.h"
#include "gql_matcher.h"
#include "gql_debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// Value Management Implementation
// ============================================================================

gql_value_t* gql_value_create_null(void) {
    gql_value_t *value = malloc(sizeof(gql_value_t));
    if (!value) return NULL;
    
    value->type = GQL_VALUE_NULL;
    return value;
}

gql_value_t* gql_value_create_integer(int64_t int_value) {
    gql_value_t *value = malloc(sizeof(gql_value_t));
    if (!value) return NULL;
    
    value->type = GQL_VALUE_INTEGER;
    value->data.integer = int_value;
    return value;
}

gql_value_t* gql_value_create_string(const char *str_value) {
    gql_value_t *value = malloc(sizeof(gql_value_t));
    if (!value) return NULL;
    
    value->type = GQL_VALUE_STRING;
    value->data.string = str_value ? strdup(str_value) : NULL;
    return value;
}

gql_value_t* gql_value_create_boolean(bool bool_value) {
    gql_value_t *value = malloc(sizeof(gql_value_t));
    if (!value) return NULL;
    
    value->type = GQL_VALUE_BOOLEAN;
    value->data.boolean = bool_value;
    return value;
}

gql_value_t* gql_value_create_node(int64_t id, char **labels, size_t label_count, property_set_t *props) {
    gql_value_t *value = malloc(sizeof(gql_value_t));
    if (!value) return NULL;
    
    value->type = GQL_VALUE_NODE;
    value->data.node.id = id;
    value->data.node.labels = labels;
    value->data.node.label_count = label_count;
    value->data.node.properties = props;
    return value;
}

gql_value_t* gql_value_create_edge(int64_t id, int64_t source, int64_t target, const char *type, property_set_t *props) {
    gql_value_t *value = malloc(sizeof(gql_value_t));
    if (!value) return NULL;
    
    value->type = GQL_VALUE_EDGE;
    value->data.edge.id = id;
    value->data.edge.source_id = source;
    value->data.edge.target_id = target;
    value->data.edge.type = type ? strdup(type) : NULL;
    value->data.edge.properties = props;
    return value;
}

void gql_value_free_contents(gql_value_t *value) {
    if (!value) {
        GQL_DEBUG("gql_value_free_contents - value is NULL");
        return;
    }
    
    GQL_DEBUG("gql_value_free_contents - freeing value type %d", value->type);
    
    switch (value->type) {
        case GQL_VALUE_STRING:
            GQL_DEBUG("gql_value_free_contents - freeing string");
            free(value->data.string);
            break;
        case GQL_VALUE_NODE:
            GQL_DEBUG("gql_value_free_contents - freeing node (id=%lld, label_count=%zu)", 
                   (long long)value->data.node.id, value->data.node.label_count);
            if (value->data.node.labels) {
                for (size_t i = 0; i < value->data.node.label_count; i++) {
                    GQL_DEBUG("gql_value_free_contents - freeing label %zu", i);
                    free(value->data.node.labels[i]);
                }
                GQL_DEBUG("gql_value_free_contents - freeing labels array");
                free(value->data.node.labels);
            }
            GQL_DEBUG("gql_value_free_contents - freeing node properties");
            free_property_set(value->data.node.properties);
            GQL_DEBUG("gql_value_free_contents - node properties freed");
            break;
        case GQL_VALUE_EDGE:
            free(value->data.edge.type);
            free_property_set(value->data.edge.properties);
            break;
        case GQL_VALUE_ARRAY:
            if (value->data.array.items) {
                for (size_t i = 0; i < value->data.array.count; i++) {
                    gql_value_free_contents(&value->data.array.items[i]);
                }
                free(value->data.array.items);
            }
            break;
        default:
            GQL_DEBUG("gql_value_free_contents - default case (type=%d)", value->type);
            break;
    }
    GQL_DEBUG("gql_value_free_contents - completed");
}

void gql_value_free(gql_value_t *value) {
    gql_value_free_contents(value);
    free(value);
}

char* gql_value_to_string(gql_value_t *value) {
    if (!value) return strdup("NULL");
    
    char *result = NULL;
    
    switch (value->type) {
        case GQL_VALUE_NULL:
            result = strdup("NULL");
            break;
        case GQL_VALUE_INTEGER:
            result = malloc(32);
            if (result) {
                snprintf(result, 32, "%lld", (long long)value->data.integer);
            }
            break;
        case GQL_VALUE_STRING:
            result = strdup(value->data.string ? value->data.string : "");
            break;
        case GQL_VALUE_BOOLEAN:
            result = strdup(value->data.boolean ? "true" : "false");
            break;
        case GQL_VALUE_NODE:
            result = malloc(256);
            if (result) {
                snprintf(result, 256, "Node{id:%lld}", (long long)value->data.node.id);
            }
            break;
        case GQL_VALUE_EDGE:
            result = malloc(256);
            if (result) {
                snprintf(result, 256, "Edge{id:%lld, type:%s}", 
                        (long long)value->data.edge.id, 
                        value->data.edge.type ? value->data.edge.type : "");
            }
            break;
        default:
            result = strdup("UNKNOWN");
            break;
    }
    
    return result ? result : strdup("ERROR");
}

// ============================================================================
// Result Management Implementation
// ============================================================================

gql_result_t* gql_result_create(void) {
    gql_result_t *result = calloc(1, sizeof(gql_result_t));
    if (!result) return NULL;
    
    result->status = GQL_RESULT_SUCCESS;
    result->error_message = NULL;
    result->rows = NULL;
    result->row_count = 0;
    result->column_names = NULL;
    result->column_count = 0;
    
    return result;
}

void gql_result_destroy(gql_result_t *result) {
    GQL_DEBUG("gql_result_destroy - starting");
    if (!result) return;
    
    GQL_DEBUG("gql_result_destroy - freeing error message");
    // Free error message
    free(result->error_message);
    
    GQL_DEBUG("gql_result_destroy - freeing column names");
    // Free column names
    if (result->column_names) {
        for (size_t i = 0; i < result->column_count; i++) {
            free(result->column_names[i]);
        }
        free(result->column_names);
    }
    
    GQL_DEBUG("gql_result_destroy - freeing %zu rows", result->row_count);
    // Free rows
    gql_result_row_t *row = result->rows;
    size_t row_index = 0;
    while (row) {
        GQL_DEBUG("gql_result_destroy - freeing row %zu", row_index++);
        gql_result_row_t *next = row->next;
        
        // Free column values
        if (row->columns) {
            GQL_DEBUG("gql_result_destroy - freeing %zu column values", row->column_count);
            for (size_t i = 0; i < row->column_count; i++) {
                GQL_DEBUG("gql_result_destroy - freeing column value %zu", i);
                gql_value_free_contents(&row->columns[i]); // Only free contents, not the struct
                GQL_DEBUG("gql_result_destroy - column value %zu freed", i);
            }
            GQL_DEBUG("gql_result_destroy - freeing columns array");
            free(row->columns);
        }
        
        // Free column names (if different from result column names)
        if (row->column_names && row->column_names != result->column_names) {
            for (size_t i = 0; i < row->column_count; i++) {
                free(row->column_names[i]);
            }
            free(row->column_names);
        }
        
        GQL_DEBUG("gql_result_destroy - freeing row structure");
        free(row);
        row = next;
    }
    
    GQL_DEBUG("gql_result_destroy - freeing result structure");
    free(result);
    GQL_DEBUG("gql_result_destroy - completed");
}

int gql_result_add_column(gql_result_t *result, const char *name) {
    if (!result || !name) return -1;
    
    // Resize column names array
    char **new_names = realloc(result->column_names, 
                              (result->column_count + 1) * sizeof(char*));
    if (!new_names) return -1;
    
    result->column_names = new_names;
    result->column_names[result->column_count] = strdup(name);
    result->column_count++;
    
    return 0;
}

int gql_result_add_row(gql_result_t *result, gql_value_t *values, size_t count) {
    GQL_DEBUG("gql_result_add_row - starting with count=%zu", count);
    if (!result || !values) {
        GQL_DEBUG("gql_result_add_row - invalid parameters");
        return -1;
    }
    
    GQL_DEBUG("gql_result_add_row - allocating row");
    gql_result_row_t *row = malloc(sizeof(gql_result_row_t));
    if (!row) {
        GQL_DEBUG("gql_result_add_row - row allocation failed");
        return -1;
    }
    
    GQL_DEBUG("gql_result_add_row - allocating columns array");
    row->columns = malloc(count * sizeof(gql_value_t));
    if (!row->columns) {
        GQL_DEBUG("gql_result_add_row - columns allocation failed");
        free(row);
        return -1;
    }
    
    GQL_DEBUG("gql_result_add_row - deep copying %zu values", count);
    // Deep copy values to avoid ownership issues
    for (size_t i = 0; i < count; i++) {
        GQL_DEBUG("gql_result_add_row - copying value %zu (type=%d)", i, values[i].type);
        row->columns[i] = values[i]; // Copy the basic structure
        
        // Deep copy complex data to avoid double-free issues
        GQL_DEBUG("gql_result_add_row - processing type %d", values[i].type);
        switch (values[i].type) {
            case GQL_VALUE_STRING:
                GQL_DEBUG("gql_result_add_row - copying string value");
                if (values[i].data.string) {
                    row->columns[i].data.string = strdup(values[i].data.string);
                    GQL_DEBUG("gql_result_add_row - string copied");
                }
                break;
                
            case GQL_VALUE_NODE:
                GQL_DEBUG("gql_result_add_row - copying node value (label_count=%zu)", values[i].data.node.label_count);
                // Deep copy labels array
                if (values[i].data.node.labels && values[i].data.node.label_count > 0) {
                    char **new_labels = malloc(values[i].data.node.label_count * sizeof(char*));
                    if (new_labels) {
                        for (size_t j = 0; j < values[i].data.node.label_count; j++) {
                            new_labels[j] = strdup(values[i].data.node.labels[j]);
                        }
                        row->columns[i].data.node.labels = new_labels;
                    }
                    GQL_DEBUG("gql_result_add_row - node labels copied");
                }
                // Note: properties are NULL for now, so no deep copy needed
                break;
                
            case GQL_VALUE_EDGE:
                GQL_DEBUG("gql_result_add_row - copying edge value");
                // Deep copy edge type
                if (values[i].data.edge.type) {
                    row->columns[i].data.edge.type = strdup(values[i].data.edge.type);
                    GQL_DEBUG("gql_result_add_row - edge type copied");
                } else {
                    GQL_DEBUG("gql_result_add_row - edge type is NULL");
                }
                // Note: properties are NULL for now, so no deep copy needed
                break;
                
            default:
                GQL_DEBUG("gql_result_add_row - default case for type %d", values[i].type);
                // For simple types, shallow copy is sufficient
                break;
        }
        GQL_DEBUG("gql_result_add_row - value %zu processed", i);
    }
    
    row->column_count = count;
    row->column_names = result->column_names; // Share column names
    row->next = NULL;
    
    // Add to end of list
    if (!result->rows) {
        result->rows = row;
    } else {
        gql_result_row_t *last = result->rows;
        while (last->next) {
            last = last->next;
        }
        last->next = row;
    }
    
    result->row_count++;
    return 0;
}

void gql_result_set_error(gql_result_t *result, const char *message) {
    if (!result) return;
    
    result->status = GQL_RESULT_ERROR;
    free(result->error_message);
    result->error_message = message ? strdup(message) : NULL;
}

// ============================================================================
// Execution Context Implementation
// ============================================================================

gql_execution_context_t* gql_context_create(graphqlite_db_t *db) {
    gql_execution_context_t *ctx = calloc(1, sizeof(gql_execution_context_t));
    if (!ctx) return NULL;
    
    ctx->db = db;
    ctx->variables = NULL;
    ctx->variable_count = 0;
    ctx->variable_capacity = 0;
    ctx->current_result = NULL;
    ctx->in_transaction = false;
    ctx->error_message = NULL;
    
    return ctx;
}

void gql_context_destroy(gql_execution_context_t *ctx) {
    if (!ctx) return;
    
    // Free variables
    if (ctx->variables) {
        for (size_t i = 0; i < ctx->variable_count; i++) {
            free(ctx->variables[i].name);
            gql_value_free(&ctx->variables[i].value);
        }
        free(ctx->variables);
    }
    
    free(ctx->error_message);
    free(ctx);
}

int gql_context_set_variable(gql_execution_context_t *ctx, const char *name, gql_value_t *value) {
    if (!ctx || !name || !value) return -1;
    
    // Check if variable already exists
    for (size_t i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            // Update existing variable
            gql_value_free(&ctx->variables[i].value);
            ctx->variables[i].value = *value;
            return 0;
        }
    }
    
    // Add new variable
    if (ctx->variable_count >= ctx->variable_capacity) {
        size_t new_capacity = ctx->variable_capacity == 0 ? 8 : ctx->variable_capacity * 2;
        gql_variable_t *new_vars = realloc(ctx->variables, 
                                          new_capacity * sizeof(gql_variable_t));
        if (!new_vars) return -1;
        
        ctx->variables = new_vars;
        ctx->variable_capacity = new_capacity;
    }
    
    ctx->variables[ctx->variable_count].name = strdup(name);
    ctx->variables[ctx->variable_count].value = *value;
    ctx->variable_count++;
    
    return 0;
}

gql_value_t* gql_context_get_variable(gql_execution_context_t *ctx, const char *name) {
    if (!ctx || !name) return NULL;
    
    for (size_t i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return &ctx->variables[i].value;
        }
    }
    
    return NULL;
}

// ============================================================================
// Expression Evaluation
// ============================================================================

gql_value_t evaluate_expression(gql_execution_context_t *ctx, gql_ast_node_t *expr) {
    gql_value_t null_value = {.type = GQL_VALUE_NULL};
    
    if (!ctx || !expr) return null_value;
    
    switch (expr->type) {
        case GQL_AST_INTEGER_LITERAL:
        {
            gql_value_t value = {.type = GQL_VALUE_INTEGER};
            value.data.integer = expr->data.integer_literal.value;
            return value;
        }
        
        case GQL_AST_STRING_LITERAL:
        {
            gql_value_t value = {.type = GQL_VALUE_STRING};
            value.data.string = strdup(expr->data.string_literal.value ? 
                                     expr->data.string_literal.value : "");
            return value;
        }
        
        case GQL_AST_BOOLEAN_LITERAL:
        {
            gql_value_t value = {.type = GQL_VALUE_BOOLEAN};
            value.data.boolean = expr->data.boolean_literal.value;
            return value;
        }
        
        case GQL_AST_NULL_LITERAL:
            return null_value;
            
        case GQL_AST_IDENTIFIER:
        {
            const char *name = expr->data.identifier.name;
            gql_value_t *var = gql_context_get_variable(ctx, name);
            return var ? *var : null_value;
        }
        
        case GQL_AST_PROPERTY_ACCESS:
        {
            // For now, return null - property access needs more context
            return null_value;
        }
        
        case GQL_AST_BINARY_EXPR:
        {
            gql_value_t left = evaluate_expression(ctx, expr->data.binary_expr.left);
            gql_value_t right = evaluate_expression(ctx, expr->data.binary_expr.right);
            
            // Simple comparison for now
            if (expr->data.binary_expr.operator == GQL_OP_EQUALS) {
                gql_value_t result = {.type = GQL_VALUE_BOOLEAN};
                result.data.boolean = gql_values_equal(&left, &right);
                return result;
            }
            
            return null_value;
        }
        
        default:
            return null_value;
    }
}

bool gql_values_equal(gql_value_t *a, gql_value_t *b) {
    if (!a || !b) return false;
    if (a->type != b->type) return false;
    
    switch (a->type) {
        case GQL_VALUE_NULL:
            return true;
        case GQL_VALUE_INTEGER:
            return a->data.integer == b->data.integer;
        case GQL_VALUE_STRING:
            return strcmp(a->data.string ? a->data.string : "",
                         b->data.string ? b->data.string : "") == 0;
        case GQL_VALUE_BOOLEAN:
            return a->data.boolean == b->data.boolean;
        case GQL_VALUE_NODE:
            return a->data.node.id == b->data.node.id;
        case GQL_VALUE_EDGE:
            return a->data.edge.id == b->data.edge.id;
        default:
            return false;
    }
}

int gql_value_compare(gql_value_t *a, gql_value_t *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    // NULL values are less than everything else
    if (a->type == GQL_VALUE_NULL && b->type == GQL_VALUE_NULL) return 0;
    if (a->type == GQL_VALUE_NULL) return -1;
    if (b->type == GQL_VALUE_NULL) return 1;
    
    // Different types - compare type order
    if (a->type != b->type) {
        return (int)a->type - (int)b->type;
    }
    
    // Same types - compare values
    switch (a->type) {
        case GQL_VALUE_INTEGER:
            if (a->data.integer < b->data.integer) return -1;
            if (a->data.integer > b->data.integer) return 1;
            return 0;
            
        case GQL_VALUE_STRING:
            return strcmp(a->data.string ? a->data.string : "",
                         b->data.string ? b->data.string : "");
            
        case GQL_VALUE_BOOLEAN:
            if (!a->data.boolean && b->data.boolean) return -1;
            if (a->data.boolean && !b->data.boolean) return 1;
            return 0;
            
        case GQL_VALUE_NODE:
            if (a->data.node.id < b->data.node.id) return -1;
            if (a->data.node.id > b->data.node.id) return 1;
            return 0;
            
        case GQL_VALUE_EDGE:
            if (a->data.edge.id < b->data.edge.id) return -1;
            if (a->data.edge.id > b->data.edge.id) return 1;
            return 0;
            
        default:
            return 0;
    }
}

// ============================================================================
// CREATE Query Execution
// ============================================================================

gql_result_t* execute_create_query(gql_execution_context_t *ctx, gql_ast_node_t *ast) {
    gql_result_t *result = gql_result_create();
    if (!result) return NULL;
    
    if (!ctx || !ast || ast->type != GQL_AST_CREATE_QUERY) {
        gql_result_set_error(result, "Invalid CREATE query");
        return result;
    }
    
    // Start transaction
    bool started_transaction = false;
    if (!graphqlite_in_transaction(ctx->db)) {
        if (graphqlite_begin_transaction(ctx->db) == SQLITE_OK) {
            started_transaction = true;
        }
    }
    
    // Execute pattern creation
    gql_ast_node_t *patterns = ast->data.create_query.patterns;
    if (!patterns) {
        gql_result_set_error(result, "No patterns to create");
        goto cleanup;
    }
    
    // Process each pattern in the list
    gql_ast_node_t *current = patterns->next; // Skip list header
    while (current) {
        if (current->type == GQL_AST_PATTERN) {
            // Handle full pattern (node-edge-node)
            gql_ast_node_t *node1 = current->data.pattern.node;
            gql_ast_node_t *edge = current->data.pattern.edge;
            gql_ast_node_t *node2 = current->data.pattern.target_node;
            
            if (node1 && node2) {
                // Create both nodes
                int64_t node1_id = graphqlite_create_node(ctx->db);
                int64_t node2_id = graphqlite_create_node(ctx->db);
                
                if (node1_id > 0 && node2_id > 0) {
                    result->nodes_created += 2;
                    
                    // Add labels if specified
                    if (node1->data.node_pattern.labels) {
                        gql_ast_node_t *label_node = node1->data.node_pattern.labels;
                        while (label_node) {
                            if (label_node->type == GQL_AST_STRING_LITERAL) {
                                graphqlite_add_node_label(ctx->db, node1_id, label_node->data.string_literal.value);
                            }
                            label_node = label_node->next;
                        }
                    }
                    if (node2->data.node_pattern.labels) {
                        gql_ast_node_t *label_node = node2->data.node_pattern.labels;
                        while (label_node) {
                            if (label_node->type == GQL_AST_STRING_LITERAL) {
                                graphqlite_add_node_label(ctx->db, node2_id, label_node->data.string_literal.value);
                            }
                            label_node = label_node->next;
                        }
                    }
                    
                    // Create edge if specified
                    if (edge && edge->data.edge_pattern.type) {
                        int64_t edge_id = graphqlite_create_edge(ctx->db, node1_id, node2_id, 
                                                               edge->data.edge_pattern.type);
                        if (edge_id > 0) {
                            result->edges_created++;
                        }
                    }
                }
            }
        } else if (current->type == GQL_AST_NODE_PATTERN) {
            // Handle single node creation
            int64_t node_id = graphqlite_create_node(ctx->db);
            if (node_id > 0) {
                result->nodes_created++;
                
                // Add labels if specified
                if (current->data.node_pattern.labels) {
                    gql_ast_node_t *label_node = current->data.node_pattern.labels;
                    while (label_node) {
                        if (label_node->type == GQL_AST_STRING_LITERAL) {
                            graphqlite_add_node_label(ctx->db, node_id, label_node->data.string_literal.value);
                        }
                        label_node = label_node->next;
                    }
                }
                
                // Set properties if specified
                if (current->data.node_pattern.properties) {
                    // TODO: Implement property setting from property map
                }
            }
        }
        
        current = current->next;
    }
    
cleanup:
    // Commit transaction if we started it
    if (started_transaction) {
        if (result->status == GQL_RESULT_SUCCESS) {
            graphqlite_commit_transaction(ctx->db);
        } else {
            graphqlite_rollback_transaction(ctx->db);
        }
    }
    
    return result;
}

// ============================================================================
// MATCH Query Execution
// ============================================================================

gql_result_t* execute_match_query(gql_execution_context_t *ctx, gql_ast_node_t *ast) {
    GQL_DEBUG("execute_match_query - starting");
    gql_result_t *result = gql_result_create();
    if (!result) return NULL;
    
    if (!ctx || !ast || ast->type != GQL_AST_MATCH_QUERY) {
        gql_result_set_error(result, "Invalid MATCH query");
        return result;
    }
    
    GQL_DEBUG("execute_match_query - valid parameters");
    
    // Extract query components
    gql_ast_node_t *patterns = ast->data.match_query.patterns;
    gql_ast_node_t *where_clause = ast->data.match_query.where_clause;
    gql_ast_node_t *return_clause = ast->data.match_query.return_clause;
    
    if (!patterns) {
        gql_result_set_error(result, "No patterns specified in MATCH query");
        return result;
    }
    
    if (!return_clause) {
        gql_result_set_error(result, "No RETURN clause specified");
        return result;
    }
    
    // Step 1: Match patterns
    GQL_DEBUG("execute_match_query - calling match_patterns");
    match_result_set_t *matches = match_patterns(ctx, patterns);
    if (!matches) {
        GQL_DEBUG("execute_match_query - match_patterns returned NULL");
        gql_result_set_error(result, "Failed to match patterns");
        return result;
    }
    
    GQL_DEBUG("execute_match_query - match_patterns completed, status=%d", matches->status);
    
    if (matches->status == MATCH_RESULT_ERROR) {
        gql_result_set_error(result, matches->error_message ? 
                           matches->error_message : "Pattern matching failed");
        destroy_match_result_set(matches);
        return result;
    }
    
    if (matches->status == MATCH_RESULT_NO_MATCHES) {
        // Return empty result set
        destroy_match_result_set(matches);
        return result;
    }
    
    // Step 2: Apply WHERE filter if present
    match_result_set_t *filtered_matches = matches;
    if (where_clause) {
        filtered_matches = apply_where_filter(ctx, matches, where_clause);
        if (filtered_matches != matches) {
            destroy_match_result_set(matches);
        }
        
        if (!filtered_matches) {
            gql_result_set_error(result, "Failed to apply WHERE filter");
            return result;
        }
        
        if (filtered_matches->status == MATCH_RESULT_NO_MATCHES) {
            destroy_match_result_set(filtered_matches);
            return result;
        }
    }
    
    // Step 3: Project results according to RETURN clause
    GQL_DEBUG("execute_match_query - calling project_match_results");
    gql_result_destroy(result); // Destroy the empty result
    result = project_match_results(ctx, filtered_matches, return_clause);
    GQL_DEBUG("execute_match_query - project_match_results completed");
    
    GQL_DEBUG("execute_match_query - destroying filtered_matches");
    destroy_match_result_set(filtered_matches);
    GQL_DEBUG("execute_match_query - filtered_matches destroyed");
    
    GQL_DEBUG("execute_match_query - returning result");
    return result;
}

// ============================================================================
// Main Execution Interface
// ============================================================================

gql_result_t* gql_execute(graphqlite_db_t *db, gql_ast_node_t *ast) {
    if (!db || !ast) {
        gql_result_t *result = gql_result_create();
        if (result) {
            gql_result_set_error(result, "Invalid database or AST");
        }
        return result;
    }
    
    gql_execution_context_t *ctx = gql_context_create(db);
    if (!ctx) {
        gql_result_t *result = gql_result_create();
        if (result) {
            gql_result_set_error(result, "Failed to create execution context");
        }
        return result;
    }
    
    gql_result_t *result = NULL;
    
    switch (ast->type) {
        case GQL_AST_MATCH_QUERY:
            GQL_DEBUG("gql_execute - calling execute_match_query");
            result = execute_match_query(ctx, ast);
            GQL_DEBUG("gql_execute - execute_match_query returned");
            break;
        case GQL_AST_CREATE_QUERY:
            result = execute_create_query(ctx, ast);
            break;
        default:
            result = gql_result_create();
            if (result) {
                gql_result_set_error(result, "Unsupported query type");
            }
            break;
    }
    
    GQL_DEBUG("gql_execute - destroying context");
    gql_context_destroy(ctx);
    GQL_DEBUG("gql_execute - context destroyed, returning");
    return result;
}

gql_result_t* gql_execute_query(const char *query, graphqlite_db_t *db) {
    if (!query || !db) {
        gql_result_t *result = gql_result_create();
        if (result) {
            gql_result_set_error(result, "Invalid query or database");
        }
        return result;
    }
    
    // Parse the query
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) {
        gql_result_t *result = gql_result_create();
        if (result) {
            gql_result_set_error(result, "Failed to create parser");
        }
        return result;
    }
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    gql_result_t *result = NULL;
    if (gql_parser_has_error(parser)) {
        result = gql_result_create();
        if (result) {
            gql_result_set_error(result, gql_parser_get_error(parser));
        }
    } else if (ast) {
        result = gql_execute(db, ast);
    } else {
        result = gql_result_create();
        if (result) {
            gql_result_set_error(result, "Failed to parse query");
        }
    }
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    
    gql_parser_destroy(parser);
    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* gql_value_type_name(gql_value_type_t type) {
    switch (type) {
        case GQL_VALUE_NULL: return "NULL";
        case GQL_VALUE_INTEGER: return "INTEGER";
        case GQL_VALUE_STRING: return "STRING";
        case GQL_VALUE_BOOLEAN: return "BOOLEAN";
        case GQL_VALUE_NODE: return "NODE";
        case GQL_VALUE_EDGE: return "EDGE";
        case GQL_VALUE_ARRAY: return "ARRAY";
        default: return "UNKNOWN";
    }
}

void gql_result_print(gql_result_t *result) {
    if (!result) {
        printf("NULL result\n");
        return;
    }
    
    if (result->status == GQL_RESULT_ERROR) {
        printf("Error: %s\n", result->error_message ? result->error_message : "Unknown error");
        return;
    }
    
    if (result->row_count == 0) {
        printf("No results\n");
        return;
    }
    
    // Print column headers
    if (result->column_names) {
        for (size_t i = 0; i < result->column_count; i++) {
            printf("%-20s", result->column_names[i]);
        }
        printf("\n");
        
        for (size_t i = 0; i < result->column_count; i++) {
            printf("%-20s", "--------------------");
        }
        printf("\n");
    }
    
    // Print rows
    gql_result_row_t *row = result->rows;
    while (row) {
        for (size_t i = 0; i < row->column_count; i++) {
            char *str = gql_value_to_string(&row->columns[i]);
            printf("%-20s", str);
            free(str);
        }
        printf("\n");
        row = row->next;
    }
    
    printf("\n%zu row(s) returned\n", result->row_count);
    if (result->nodes_created > 0) {
        printf("%llu node(s) created\n", (unsigned long long)result->nodes_created);
    }
    if (result->edges_created > 0) {
        printf("%llu edge(s) created\n", (unsigned long long)result->edges_created);
    }
}