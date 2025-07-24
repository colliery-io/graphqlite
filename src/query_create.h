#ifndef GRAPHQLITE_QUERY_CREATE_H
#define GRAPHQLITE_QUERY_CREATE_H

#include <sqlite3.h>
#include <stdint.h>
#include "graphqlite.h"
#include "ast.h"

/**
 * Query Create Module
 * 
 * This module handles the execution of CREATE statements in GraphQLite.
 * It implements node and relationship creation operations with full
 * property support and proper error handling.
 * 
 * Features:
 * - Node creation with labels and typed properties
 * - Relationship creation with type and properties
 * - Atomic operations with proper error reporting
 * - Support for all property types (text, integer, float, boolean)
 */

/**
 * Execute a CREATE statement from the AST.
 * 
 * This is the main entry point for CREATE operations. It dispatches
 * to either node or relationship creation based on the pattern type.
 * 
 * @param db SQLite database handle
 * @param ast AST node representing the CREATE statement
 * @return GraphQLite result with success/error status
 */
graphqlite_result_t* execute_create_statement(sqlite3 *db, cypher_ast_node_t *ast);

/**
 * Execute creation of a single node.
 * 
 * Creates a node with the specified label and properties.
 * 
 * @param db SQLite database handle
 * @param node_pattern AST node pattern for the node to create
 * @return GraphQLite result with success/error status
 */
graphqlite_result_t* execute_create_node(sqlite3 *db, cypher_ast_node_t *node_pattern);

/**
 * Execute creation of a relationship pattern.
 * 
 * Creates both nodes (if they don't exist) and the connecting relationship
 * with the specified type and properties.
 * 
 * @param db SQLite database handle
 * @param rel_pattern AST relationship pattern for the relationship to create
 * @return GraphQLite result with success/error status
 */
graphqlite_result_t* execute_create_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern);

/**
 * Helper function to create a single node and return its ID.
 * 
 * This is a lower-level function that handles the actual node creation
 * including label assignment and property insertion.
 * 
 * @param db SQLite database handle
 * @param node_pattern AST node pattern for the node to create
 * @param result Result object for error reporting
 * @return Node ID on success, -1 on failure (error set in result)
 */
int64_t create_node_with_properties(sqlite3 *db, cypher_ast_node_t *node_pattern, graphqlite_result_t *result);

#endif // GRAPHQLITE_QUERY_CREATE_H