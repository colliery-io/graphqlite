#include <stddef.h>

#ifndef SQLITE_CORE
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#else
#include "sqlite3.h"
#endif

// Simple test function
static void simple_test_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "Hello from GraphQLite!", -1, SQLITE_STATIC);
}

// Minimal extension entry point (must match filename: simpletest.so -> sqlite3_simpletest_init)
int sqlite3_simpletest_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;  // Ignore error message like working tiny.c
    
    // Direct return like tiny.c
    return sqlite3_create_function(db, "graphqlite_test", 0, SQLITE_UTF8, NULL, 
                                  simple_test_func, NULL, NULL);
}