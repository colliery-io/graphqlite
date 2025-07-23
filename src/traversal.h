#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include <sqlite3.h>
#include "ast.h"
#include "graphqlite.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Traversal Result Structures
// ============================================================================

// Represents a single path in the traversal result
typedef struct {
    int *node_ids;        // Array of node IDs in the path
    int *relationship_ids; // Array of relationship IDs in the path
    int length;           // Number of hops in the path
    int capacity;         // Allocated capacity for arrays
} traversal_path_t;

// Collection of paths found during traversal
typedef struct {
    traversal_path_t *paths;  // Array of paths
    int count;               // Number of paths found
    int capacity;            // Allocated capacity
} traversal_result_t;

// Traversal configuration options
typedef struct {
    int max_paths;           // Maximum number of paths to return (-1 = unlimited)
    int max_depth;           // Maximum traversal depth (-1 = unlimited)
    int use_bfs;             // 1 = BFS (shortest paths first), 0 = DFS
    char **allowed_types;    // Array of allowed relationship types (NULL = all)
    int type_count;          // Number of allowed types
} traversal_config_t;

// ============================================================================
// Core Traversal Functions
// ============================================================================

/**
 * Execute a variable-length pattern traversal
 * @param db SQLite database connection
 * @param start_node_id Starting node ID (-1 = match any)
 * @param end_node_id Ending node ID (-1 = match any)
 * @param pattern Variable-length pattern AST node
 * @param config Traversal configuration
 * @return Traversal result or NULL on error
 */
traversal_result_t* execute_variable_length_traversal(
    sqlite3 *db,
    int start_node_id,
    int end_node_id,
    cypher_ast_node_t *pattern,
    traversal_config_t *config
);

/**
 * Execute breadth-first search traversal
 * @param db SQLite database connection
 * @param start_node_id Starting node ID
 * @param end_node_id Target node ID (-1 = match any)
 * @param min_hops Minimum number of hops
 * @param max_hops Maximum number of hops (-1 = unlimited)
 * @param allowed_types Array of allowed relationship types (NULL = all)
 * @param type_count Number of allowed types
 * @param max_paths Maximum paths to return (-1 = unlimited)
 * @return Traversal result or NULL on error
 */
traversal_result_t* bfs_traversal(
    sqlite3 *db,
    int start_node_id,
    int end_node_id,
    int min_hops,
    int max_hops,
    char **allowed_types,
    int type_count,
    int max_paths
);

/**
 * Execute depth-first search traversal
 * @param db SQLite database connection
 * @param start_node_id Starting node ID
 * @param end_node_id Target node ID (-1 = match any)
 * @param min_hops Minimum number of hops
 * @param max_hops Maximum number of hops (-1 = unlimited)
 * @param allowed_types Array of allowed relationship types (NULL = all)
 * @param type_count Number of allowed types
 * @param max_paths Maximum paths to return (-1 = unlimited)
 * @return Traversal result or NULL on error
 */
traversal_result_t* dfs_traversal(
    sqlite3 *db,
    int start_node_id,
    int end_node_id,
    int min_hops,
    int max_hops,
    char **allowed_types,
    int type_count,
    int max_paths
);

// ============================================================================
// Result Management Functions
// ============================================================================

/**
 * Create a new traversal result structure
 * @return New traversal result or NULL on error
 */
traversal_result_t* traversal_result_create(void);

/**
 * Add a path to the traversal result
 * @param result Traversal result to add to
 * @param path Path to add
 * @return 1 on success, 0 on error
 */
int traversal_result_add_path(traversal_result_t *result, traversal_path_t *path);

/**
 * Create a new path structure
 * @param initial_capacity Initial capacity for node/relationship arrays
 * @return New path or NULL on error
 */
traversal_path_t* traversal_path_create(int initial_capacity);

/**
 * Add a node to a path
 * @param path Path to add to
 * @param node_id Node ID to add
 * @return 1 on success, 0 on error
 */
int traversal_path_add_node(traversal_path_t *path, int node_id);

/**
 * Add a relationship to a path
 * @param path Path to add to
 * @param relationship_id Relationship ID to add
 * @return 1 on success, 0 on error
 */
int traversal_path_add_relationship(traversal_path_t *path, int relationship_id);

/**
 * Free a traversal result and all its paths
 * @param result Result to free
 */
void traversal_result_free(traversal_result_t *result);

/**
 * Free a single traversal path
 * @param path Path to free
 */
void traversal_path_free(traversal_path_t *path);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Check if a relationship type is allowed
 * @param relationship_type Type to check
 * @param allowed_types Array of allowed types (NULL = all allowed)
 * @param type_count Number of allowed types
 * @return 1 if allowed, 0 if not
 */
int is_relationship_type_allowed(
    const char *relationship_type,
    char **allowed_types,
    int type_count
);

/**
 * Convert traversal result to GraphQLite result format
 * @param traversal_result Traversal result to convert
 * @param db Database connection for node/relationship lookups
 * @param return_paths Whether to return full path objects or just endpoints
 * @return GraphQLite result or NULL on error
 */
graphqlite_result_t* traversal_to_graphqlite_result(
    traversal_result_t *traversal_result,
    sqlite3 *db,
    int return_paths
);

/**
 * Extract relationship types from variable-length pattern AST
 * @param pattern Variable-length pattern AST node
 * @param type_count Output parameter for number of types
 * @return Array of type strings (caller must free) or NULL
 */
char** extract_relationship_types(cypher_ast_node_t *pattern, int *type_count);

#ifdef __cplusplus
}
#endif

#endif // TRAVERSAL_H