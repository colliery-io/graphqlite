/*
 * GraphQLite SQLite Extension - Minimal version with schema migration
 * Based on working old architecture pattern
 */

#include <sqlite3ext.h>
#include "executor/cypher_schema.h"

SQLITE_EXTENSION_INIT1

/* Simple test function */
static void graphqlite_test_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "GraphQLite extension loaded successfully!", -1, SQLITE_STATIC);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_graphqlite_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
)
{
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;  /* Unused parameter */
    
    /* Register the graphqlite_test function */
    sqlite3_create_function(db, "graphqlite_test", 0, SQLITE_UTF8, 0,
                           graphqlite_test_func, 0, 0);
    
    /* Create schema during initialization */
    cypher_schema_manager *schema_manager = cypher_schema_create_manager(db);
    if (schema_manager) {
        cypher_schema_initialize(schema_manager);
        cypher_schema_free_manager(schema_manager);
    }
    
    return rc;
}