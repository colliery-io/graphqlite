/*
 * gql_check_storage(): consistency of the stored graph (GQLITE-T-0525).
 */
#ifndef GRAPHQLITE_STORAGE_CHECK_H
#define GRAPHQLITE_STORAGE_CHECK_H

#include "graphqlite_sqlite.h"

/* JSON array of violations, "[]" when consistent; sqlite3_free it. */
char *gql_check_storage_db(sqlite3 *db);

void gql_check_storage_func(sqlite3_context *context, int argc, sqlite3_value **argv);

#endif
