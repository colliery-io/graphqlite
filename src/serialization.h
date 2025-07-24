#ifndef GRAPHQLITE_SERIALIZATION_H
#define GRAPHQLITE_SERIALIZATION_H

#include <sqlite3.h>
#include <stdint.h>

/**
 * Serialization Module
 * 
 * This module handles the serialization of graph entities (nodes and relationships)
 * into OpenCypher-compatible JSON format. It provides the output formatting
 * for query results in GraphQLite.
 * 
 * Features:
 * - Node entity serialization with labels and typed properties
 * - Relationship entity serialization with type, endpoints, and properties
 * - OpenCypher JSON format compliance
 * - Efficient property retrieval and formatting
 */

/**
 * Serialize a complete node entity to JSON format.
 * 
 * Creates a JSON representation of a node including its identity,
 * labels, and all properties with proper type formatting.
 * 
 * Format: {"identity": <id>, "labels": [<labels>], "properties": {<props>}}
 * 
 * @param db SQLite database handle
 * @param node_id Node ID to serialize
 * @return Allocated JSON string, or NULL on failure (caller must free)
 */
char* serialize_node_entity(sqlite3 *db, int64_t node_id);

/**
 * Serialize a complete relationship entity to JSON format.
 * 
 * Creates a JSON representation of a relationship including its identity,
 * type, start/end node IDs, and all properties.
 * 
 * Format: {"identity": <id>, "type": "<type>", "start": <start_id>, 
 *          "end": <end_id>, "properties": {<props>}}
 * 
 * @param db SQLite database handle
 * @param edge_id Edge ID to serialize
 * @return Allocated JSON string, or NULL on failure (caller must free)
 */
char* serialize_relationship_entity(sqlite3 *db, int64_t edge_id);

#endif // GRAPHQLITE_SERIALIZATION_H