/*
 * GraphQLite SQLite Header Wrapper
 *
 * This header ensures proper SQLite API access for both extension builds
 * (where calls must go through the API pointer table) and test builds
 * (where direct linking is fine).
 */

#ifndef GRAPHQLITE_SQLITE_H
#define GRAPHQLITE_SQLITE_H

#ifdef GRAPHQLITE_EXTENSION
/*
 * Extension build: Use sqlite3ext.h which redefines all sqlite3_* functions
 * as macros that go through the sqlite3_api pointer table.
 */
#include <sqlite3ext.h>

/*
 * Declare the global API pointer. This is defined in extension.c.
 * SQLITE_EXTENSION_INIT3 expands to: extern const sqlite3_api_routines *sqlite3_api;
 */
SQLITE_EXTENSION_INIT3

#else
/*
 * Test/standalone build: Use regular sqlite3.h with direct function calls.
 */
#include <sqlite3.h>

#endif /* GRAPHQLITE_EXTENSION */

#endif /* GRAPHQLITE_SQLITE_H */
