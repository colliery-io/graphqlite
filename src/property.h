#ifndef GRAPHQLITE_PROPERTY_H
#define GRAPHQLITE_PROPERTY_H

#include <sqlite3.h>
#include "graphqlite.h"
#include "ast.h"

/**
 * Property System Module
 * 
 * This module handles the EAV (Entity-Attribute-Value) property management
 * for GraphQLite. It provides typed property storage and retrieval for both
 * nodes and edges in the graph database.
 * 
 * Features:
 * - Property key interning for efficient storage
 * - Typed property values (integer, float, text, boolean)
 * - AST property value extraction and conversion
 * - Separate storage for node and edge properties
 */

/**
 * Extract property value and type from AST literal node.
 * 
 * This function analyzes an AST node and extracts its value and type
 * information for storage in the property system.
 * 
 * @param value_node AST node containing the property value
 * @param prop_value_str Output pointer for string values (can be NULL)
 * @param prop_value_num Output pointer for numeric values (can be NULL)
 * @param prop_value_int Output pointer for integer values (can be NULL)
 * @param prop_type Output pointer for the determined type (required)
 * @return 1 on success, 0 on failure
 */
int extract_property_from_ast(cypher_ast_node_t *value_node, const char **prop_value_str, 
                             double *prop_value_num, int *prop_value_int, 
                             graphqlite_value_type_t *prop_type);

/**
 * Get or create property key ID (property key interning).
 * 
 * This function implements property key interning - reusing existing
 * property key IDs when possible, or creating new ones as needed.
 * This reduces storage overhead and improves query performance.
 * 
 * @param db SQLite database handle
 * @param key Property key name
 * @return Property key ID on success, -1 on failure
 */
int64_t get_or_create_property_key_id(sqlite3 *db, const char *key);

/**
 * Insert node property with typed storage.
 * 
 * Stores a property value for a node using the appropriate typed
 * table based on the property's data type.
 * 
 * @param db SQLite database handle
 * @param node_id Node ID to attach property to
 * @param key_id Property key ID (from get_or_create_property_key_id)
 * @param value Property value as string (will be converted to appropriate type)
 * @param type Data type of the property value
 * @return GRAPHQLITE_OK on success, error code on failure
 */
int insert_node_property(sqlite3 *db, int64_t node_id, int64_t key_id, 
                        const char *value, graphqlite_value_type_t type);

/**
 * Insert edge property with typed storage.
 * 
 * Stores a property value for an edge using the appropriate typed
 * table based on the property's data type.
 * 
 * @param db SQLite database handle
 * @param edge_id Edge ID to attach property to
 * @param key_id Property key ID (from get_or_create_property_key_id)
 * @param value Property value as string (will be converted to appropriate type)
 * @param type Data type of the property value
 * @return GRAPHQLITE_OK on success, error code on failure
 */
int insert_edge_property(sqlite3 *db, int64_t edge_id, int64_t key_id, 
                        const char *value, graphqlite_value_type_t type);

#endif // GRAPHQLITE_PROPERTY_H