/* Minimal GraphQLite extension test */
#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

/* Simple test function */
static void minimal_test_func(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3_result_text(context, "Minimal test works!", -1, SQLITE_STATIC);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_minimaltest_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  
  /* Register test function */
  sqlite3_create_function(db, "minimal_test", 0, SQLITE_UTF8, 0,
                         minimal_test_func, 0, 0);
  
  return rc;
}