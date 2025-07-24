#ifndef GRAPHQLITE_SCHEMA_H
#define GRAPHQLITE_SCHEMA_H

#include <sqlite3.h>
#include "graphqlite.h"

/**
 * Schema Management Module
 * 
 * This module handles the creation and management of the GraphQLite database schema.
 * It implements a typed EAV (Entity-Attribute-Value) model optimized for graph workloads.
 * 
 * The schema includes:
 * - Core entity tables (nodes, edges)
 * - Property management with typed storage
 * - Performance indexes optimized for graph traversal
 * - ACID transaction support
 */

/**
 * Create the complete GraphQLite schema in the provided database.
 * 
 * This function creates all necessary tables, indexes, and constraints
 * for the GraphQLite graph database. It is idempotent - calling it
 * multiple times on the same database is safe.
 * 
 * @param db SQLite database handle
 * @return GRAPHQLITE_OK on success, GRAPHQLITE_ERROR on failure
 */
int create_schema(sqlite3 *db);

#endif // GRAPHQLITE_SCHEMA_H