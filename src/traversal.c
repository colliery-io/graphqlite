#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "traversal.h"
#include "ast.h"
#include "graphqlite.h"

// Forward declaration for node serialization
extern char* serialize_node_entity(sqlite3 *db, int64_t node_id);

// ============================================================================
// Internal Data Structures for Traversal
// ============================================================================

// Queue node for BFS traversal
typedef struct queue_node {
    int node_id;
    traversal_path_t *path;
    struct queue_node *next;
} queue_node_t;

// Queue structure for BFS
typedef struct {
    queue_node_t *front;
    queue_node_t *rear;
    int size;
} traversal_queue_t;

// Stack node for DFS traversal
typedef struct stack_node {
    int node_id;
    traversal_path_t *path;
    struct stack_node *next;
} stack_node_t;

// ============================================================================
// Result Management Functions
// ============================================================================

traversal_result_t* traversal_result_create(void) {
    traversal_result_t *result = malloc(sizeof(traversal_result_t));
    if (!result) return NULL;
    
    result->paths = NULL;
    result->count = 0;
    result->capacity = 0;
    
    return result;
}

int traversal_result_add_path(traversal_result_t *result, traversal_path_t *path) {
    if (!result || !path) return 0;
    
    // Resize array if needed
    if (result->count >= result->capacity) {
        int new_capacity = result->capacity == 0 ? 10 : result->capacity * 2;
        traversal_path_t *new_paths = realloc(result->paths, 
                                             new_capacity * sizeof(traversal_path_t));
        if (!new_paths) return 0;
        
        result->paths = new_paths;
        result->capacity = new_capacity;
    }
    
    // Deep copy the path data to avoid double free issues
    traversal_path_t *dest_path = &result->paths[result->count];
    dest_path->capacity = path->capacity;
    dest_path->length = path->length;
    
    // Allocate new memory for the arrays
    dest_path->node_ids = malloc(path->capacity * sizeof(int));
    dest_path->relationship_ids = malloc(path->capacity * sizeof(int));
    
    if (!dest_path->node_ids || !dest_path->relationship_ids) {
        free(dest_path->node_ids);
        free(dest_path->relationship_ids);
        return 0;
    }
    
    // Copy the actual data
    memcpy(dest_path->node_ids, path->node_ids, path->length * sizeof(int));
    memcpy(dest_path->relationship_ids, path->relationship_ids, path->length * sizeof(int));
    
    result->count++;
    return 1;
}

traversal_path_t* traversal_path_create(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = 10;
    
    traversal_path_t *path = malloc(sizeof(traversal_path_t));
    if (!path) return NULL;
    
    path->node_ids = malloc(initial_capacity * sizeof(int));
    path->relationship_ids = malloc(initial_capacity * sizeof(int));
    
    if (!path->node_ids || !path->relationship_ids) {
        free(path->node_ids);
        free(path->relationship_ids);
        free(path);
        return NULL;
    }
    
    path->length = 0;
    path->capacity = initial_capacity;
    
    return path;
}

int traversal_path_add_node(traversal_path_t *path, int node_id) {
    if (!path) return 0;
    
    // Resize if needed
    if (path->length >= path->capacity) {
        int new_capacity = path->capacity * 2;
        int *new_nodes = realloc(path->node_ids, new_capacity * sizeof(int));
        int *new_rels = realloc(path->relationship_ids, new_capacity * sizeof(int));
        
        if (!new_nodes || !new_rels) {
            free(new_nodes);
            free(new_rels);
            return 0;
        }
        
        path->node_ids = new_nodes;
        path->relationship_ids = new_rels;
        path->capacity = new_capacity;
    }
    
    path->node_ids[path->length] = node_id;
    path->length++;
    return 1;
}

int traversal_path_add_relationship(traversal_path_t *path, int relationship_id) {
    if (!path || path->length >= path->capacity) return 0;
    
    path->relationship_ids[path->length] = relationship_id;
    path->length++;
    
    return 1;
}

void traversal_result_free(traversal_result_t *result) {
    if (!result) return;
    
    for (int i = 0; i < result->count; i++) {
        traversal_path_free(&result->paths[i]);
    }
    
    free(result->paths);
    free(result);
}

void traversal_path_free(traversal_path_t *path) {
    if (!path) return;
    
    free(path->node_ids);
    free(path->relationship_ids);
    // Note: Don't free path itself if it's part of an array
}

// ============================================================================
// Queue Operations for BFS
// ============================================================================

static traversal_queue_t* queue_create(void) {
    traversal_queue_t *queue = malloc(sizeof(traversal_queue_t));
    if (!queue) return NULL;
    
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
    
    return queue;
}

static int queue_enqueue(traversal_queue_t *queue, int node_id, traversal_path_t *path) {
    if (!queue) return 0;
    
    queue_node_t *node = malloc(sizeof(queue_node_t));
    if (!node) return 0;
    
    node->node_id = node_id;
    node->path = path;
    node->next = NULL;
    
    if (queue->rear) {
        queue->rear->next = node;
    } else {
        queue->front = node;
    }
    
    queue->rear = node;
    queue->size++;
    
    return 1;
}

static queue_node_t* queue_dequeue(traversal_queue_t *queue) {
    if (!queue || !queue->front) return NULL;
    
    queue_node_t *node = queue->front;
    queue->front = node->next;
    
    if (!queue->front) {
        queue->rear = NULL;
    }
    
    queue->size--;
    return node;
}

static void queue_free(traversal_queue_t *queue) {
    if (!queue) return;
    
    while (queue->front) {
        queue_node_t *node = queue_dequeue(queue);
        if (node->path) {
            traversal_path_free(node->path);
            free(node->path);
        }
        free(node);
    }
    
    free(queue);
}

// ============================================================================
// Utility Functions
// ============================================================================

int is_relationship_type_allowed(const char *relationship_type, 
                                char **allowed_types, 
                                int type_count) {
    if (!allowed_types || type_count == 0) return 1;
    
    for (int i = 0; i < type_count; i++) {
        if (strcmp(relationship_type, allowed_types[i]) == 0) {
            return 1;
        }
    }
    
    return 0;
}

char** extract_relationship_types(cypher_ast_node_t *pattern, int *type_count) {
    if (!pattern || pattern->type != AST_VARIABLE_LENGTH_PATTERN || !type_count) {
        *type_count = 0;
        return NULL;
    }
    
    *type_count = pattern->data.variable_length_pattern.type_count;
    
    if (*type_count == 0) return NULL;
    
    char **types = malloc(*type_count * sizeof(char*));
    if (!types) {
        *type_count = 0;
        return NULL;
    }
    
    for (int i = 0; i < *type_count; i++) {
        cypher_ast_node_t *type_node = pattern->data.variable_length_pattern.relationship_types[i];
        if (type_node && type_node->type == AST_LABEL) {
            types[i] = strdup(type_node->data.label.name);
        } else {
            types[i] = NULL;
        }
    }
    
    return types;
}

// ============================================================================
// Core Traversal Algorithms
// ============================================================================

traversal_result_t* bfs_traversal(sqlite3 *db,
                                 int start_node_id,
                                 int end_node_id,
                                 int min_hops,
                                 int max_hops,
                                 char **allowed_types,
                                 int type_count,
                                 int max_paths) {
    
    traversal_result_t *result = traversal_result_create();
    if (!result) return NULL;
    
    traversal_queue_t *queue = queue_create();
    if (!queue) {
        traversal_result_free(result);
        return NULL;
    }
    
    // Initialize with starting node
    traversal_path_t *initial_path = traversal_path_create(10);
    if (!initial_path) {
        queue_free(queue);
        traversal_result_free(result);
        return NULL;
    }
    
    traversal_path_add_node(initial_path, start_node_id);
    queue_enqueue(queue, start_node_id, initial_path);
    
    // BFS traversal
    while (queue->size > 0 && (max_paths == -1 || result->count < max_paths)) {
        queue_node_t *current = queue_dequeue(queue);
        if (!current) break;
        
        int current_node = current->node_id;
        traversal_path_t *current_path = current->path;
        int current_depth = current_path->length - 1;
        
        // Check if we've reached target depth and node
        if (current_depth >= min_hops && 
            (end_node_id == -1 || current_node == end_node_id)) {
            
            // Found valid path - add to results
            traversal_result_add_path(result, current_path);
        }
        
        // Continue exploration if under max depth
        if (max_hops == -1 || current_depth < max_hops) {
            // Query for outgoing relationships
            const char *sql = 
                "SELECT e.id, e.type, e.target_id "
                "FROM edges e "
                "WHERE e.source_id = ?";
            
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, current_node);
                
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int rel_id = sqlite3_column_int(stmt, 0);
                    const char *rel_type = (const char*)sqlite3_column_text(stmt, 1);
                    int next_node = sqlite3_column_int(stmt, 2);
                    
                    // Check if relationship type is allowed
                    if (!is_relationship_type_allowed(rel_type, allowed_types, type_count)) {
                        continue;
                    }
                    
                    // Create new path by copying current path
                    traversal_path_t *new_path = traversal_path_create(current_path->capacity);
                    if (!new_path) continue;
                    
                    // Copy nodes and relationships
                    memcpy(new_path->node_ids, current_path->node_ids, 
                           current_path->length * sizeof(int));
                    memcpy(new_path->relationship_ids, current_path->relationship_ids, 
                           current_path->length * sizeof(int));
                    new_path->length = current_path->length;
                    
                    // Add new relationship and node
                    traversal_path_add_relationship(new_path, rel_id);
                    traversal_path_add_node(new_path, next_node);
                    
                    // Enqueue for further exploration
                    queue_enqueue(queue, next_node, new_path);
                }
                
                sqlite3_finalize(stmt);
            }
        }
        
        // Clean up current node
        if (current_path) {
            traversal_path_free(current_path);
            free(current_path);
        }
        free(current);
    }
    
    queue_free(queue);
    return result;
}

traversal_result_t* dfs_traversal(sqlite3 *db,
                                 int start_node_id,
                                 int end_node_id,
                                 int min_hops,
                                 int max_hops,
                                 char **allowed_types,
                                 int type_count,
                                 int max_paths) {
    // DFS implementation using recursion helper
    // For now, delegate to BFS - can implement true DFS later
    return bfs_traversal(db, start_node_id, end_node_id, min_hops, max_hops,
                        allowed_types, type_count, max_paths);
}

// ============================================================================
// Iterative Multi-Hop Traversal (Based on Archived GQL Implementation)
// ============================================================================

typedef struct hop_state {
    int *node_ids;
    int count;
    int capacity;
} hop_state_t;

static hop_state_t* hop_state_create(void) {
    hop_state_t *state = malloc(sizeof(hop_state_t));
    if (!state) return NULL;
    
    state->node_ids = malloc(10 * sizeof(int));
    if (!state->node_ids) {
        free(state);
        return NULL;
    }
    
    state->count = 0;
    state->capacity = 10;
    return state;
}

static int hop_state_add_node(hop_state_t *state, int node_id) {
    if (!state) return 0;
    
    // Check for duplicates
    for (int i = 0; i < state->count; i++) {
        if (state->node_ids[i] == node_id) return 1;
    }
    
    // Resize if needed
    if (state->count >= state->capacity) {
        int new_capacity = state->capacity * 2;
        int *new_nodes = realloc(state->node_ids, new_capacity * sizeof(int));
        if (!new_nodes) return 0;
        
        state->node_ids = new_nodes;
        state->capacity = new_capacity;
    }
    
    state->node_ids[state->count++] = node_id;
    return 1;
}

static void hop_state_free(hop_state_t *state) {
    if (!state) return;
    free(state->node_ids);
    free(state);
}

static traversal_result_t* iterative_multi_hop_traversal(sqlite3 *db,
                                                         int start_node_id,
                                                         int end_node_id,
                                                         int min_hops,
                                                         int max_hops,
                                                         char **allowed_types,
                                                         int type_count) {
    traversal_result_t *result = traversal_result_create();
    if (!result) return NULL;
    
    // Initialize current hop state with start node
    hop_state_t *current_hop = hop_state_create();
    if (!current_hop) {
        traversal_result_free(result);
        return NULL;
    }
    
    hop_state_add_node(current_hop, start_node_id);
    
    // For min_hops == 0, add start node as valid result
    if (min_hops == 0 && (end_node_id == -1 || start_node_id == end_node_id)) {
        traversal_path_t *path = traversal_path_create(1);
        if (path) {
            traversal_path_add_node(path, start_node_id);
            traversal_result_add_path(result, path);
            traversal_path_free(path);
            free(path);
        }
    }
    
    // Iterate through each hop level
    for (int hop = 1; hop <= max_hops || max_hops == -1; hop++) {
        hop_state_t *next_hop = hop_state_create();
        if (!next_hop) break;
        
        // For each node in current hop, find all reachable next nodes
        for (int i = 0; i < current_hop->count; i++) {
            int current_node = current_hop->node_ids[i];
            
            // Query for outgoing relationships
            const char *sql = 
                "SELECT e.id, e.type, e.target_id "
                "FROM edges e "
                "WHERE e.source_id = ?";
            
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, current_node);
                
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char *rel_type = (const char*)sqlite3_column_text(stmt, 1);
                    int next_node = sqlite3_column_int(stmt, 2);
                    
                    // Check if relationship type is allowed
                    if (!is_relationship_type_allowed(rel_type, allowed_types, type_count)) {
                        continue;
                    }
                    
                    // Add to next hop state
                    hop_state_add_node(next_hop, next_node);
                    
                    // If this hop satisfies min_hops, add as result
                    if (hop >= min_hops && (end_node_id == -1 || next_node == end_node_id)) {
                        traversal_path_t *path = traversal_path_create(hop + 1);
                        if (path) {
                            // For now, just add start and end nodes
                            // Full path reconstruction would require tracking paths
                            traversal_path_add_node(path, start_node_id);
                            for (int j = 1; j < hop; j++) {
                                traversal_path_add_node(path, -1); // Placeholder for intermediate nodes
                            }
                            traversal_path_add_node(path, next_node);
                            
                            traversal_result_add_path(result, path);
                            traversal_path_free(path);
                            free(path);
                        }
                    }
                }
                
                sqlite3_finalize(stmt);
            }
        }
        
        // Move to next hop
        hop_state_free(current_hop);
        current_hop = next_hop;
        
        // Break if no more nodes to explore or we hit max paths
        if (current_hop->count == 0) break;
        if (max_hops != -1 && hop >= max_hops) break;
    }
    
    hop_state_free(current_hop);
    return result;
}

traversal_result_t* execute_variable_length_traversal(sqlite3 *db,
                                                     int start_node_id,
                                                     int end_node_id,
                                                     cypher_ast_node_t *pattern,
                                                     traversal_config_t *config) {
    (void)config; // Suppress unused parameter warning
    
    if (!db || !pattern || pattern->type != AST_VARIABLE_LENGTH_PATTERN) {
        return NULL;
    }
    
    // Extract pattern parameters
    int min_hops = pattern->data.variable_length_pattern.min_hops;
    int max_hops = pattern->data.variable_length_pattern.max_hops;
    
    // Extract relationship types
    int type_count;
    char **allowed_types = extract_relationship_types(pattern, &type_count);
    
    // Use the iterative approach instead of BFS/DFS
    traversal_result_t *result = iterative_multi_hop_traversal(db, start_node_id, end_node_id, 
                                                               min_hops, max_hops,
                                                               allowed_types, type_count);
    
    // Clean up
    if (allowed_types) {
        for (int i = 0; i < type_count; i++) {
            free(allowed_types[i]);
        }
        free(allowed_types);
    }
    
    return result;
}

// ============================================================================
// Result Conversion Functions
// ============================================================================

graphqlite_result_t* traversal_to_graphqlite_result(traversal_result_t *traversal_result,
                                                   sqlite3 *db,
                                                   int return_paths) {
    (void)return_paths; // Suppress unused parameter warning
    
    if (!traversal_result || !db) return NULL;
    
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Always add the node column, even if no results
    graphqlite_result_add_column(result, "node", GRAPHQLITE_TYPE_TEXT);
    
    // For now, just return the end nodes of each path
    // Full path object support can be added later
    for (int i = 0; i < traversal_result->count; i++) {
        traversal_path_t *path = &traversal_result->paths[i];
        if (path->length > 0) {
            int end_node_id = path->node_ids[path->length - 1];
            
            // Use the existing serialize_node_entity function to get proper node data
            char *node_json = serialize_node_entity(db, end_node_id);
            if (node_json) {
                // Add to result
                graphqlite_result_add_row(result);
                size_t row_idx = result->row_count - 1;
                
                graphqlite_value_t value;
                value.type = GRAPHQLITE_TYPE_TEXT;
                value.data.text = node_json;
                
                graphqlite_result_set_value(result, row_idx, 0, &value);
                free(node_json);
            }
        }
    }
    
    return result;
}