/*
 * cypher_rows_vtab.h
 *
 * Perf review F10 (Metis ADR GQLITE-A-0006): `cypher_rows`, an eponymous
 * virtual table that exposes a Cypher result as SQL rows:
 *
 *   SELECT c0, c1 FROM cypher_rows('MATCH (n) RETURN n.name, n.age') LIMIT 10;
 *   SELECT row FROM cypher_rows('MATCH (n) RETURN n', '{"x": 1}');
 *
 * Columns:
 *   row    TEXT     the row as a JSON object keyed by RETURN column name
 *                   (the same value shapes cypher() emits inside its array)
 *   cols   TEXT     JSON array of the RETURN column names (constant per query)
 *   ncols  INTEGER  number of RETURN columns
 *   c0..c31         positional values with native SQLite types: integers,
 *                   reals, booleans (as 0/1 with the boolean subtype), text;
 *                   nodes, relationships, paths, lists and maps as JSON text
 *   query, params   hidden; the function-call arguments
 *
 * A write query without RETURN yields one row whose positional columns are
 * nodes_created, relationships_created, nodes_deleted, relationships_deleted,
 * properties_set and whose `row` is the same statistics object cypher()
 * returns. Errors surface as SQL errors with the same message cypher() gives.
 */
#ifndef GQL_CYPHER_ROWS_VTAB_H
#define GQL_CYPHER_ROWS_VTAB_H

#include "graphqlite_sqlite.h"
#include "executor/cypher_executor.h"

/* Returns the connection's executor, creating it on first use; the
 * extension supplies this so the table shares the executor (and its
 * statement cache, CSR cache, ...) with cypher(). */
typedef cypher_executor *(*gql_executor_getter)(void *arg);

int graphqlite_register_cypher_rows(sqlite3 *db, gql_executor_getter getter, void *getter_arg);

#endif
