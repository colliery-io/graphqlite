#include <stddef.h>

#ifndef SQLITE_CORE
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#else
#include "sqlite3.h"
#endif

// Super minimal function - no parameters, just return a constant
static void tiny_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "tiny works", -1, SQLITE_STATIC);
}

// Entry point for tiny.dylib -> sqlite3_tiny_init
int sqlite3_tiny_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;  // Ignore error message for now
    
    return sqlite3_create_function(db, "tiny", 0, SQLITE_UTF8, NULL, tiny_func, NULL, NULL);
}