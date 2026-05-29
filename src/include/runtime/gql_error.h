/*
 * runtime/gql_error.h
 *    Structured error response helper for SQLite UDFs. Returns
 *    JSON of the shape `{"error": "...", "code": "..."}` via
 *    sqlite3_result_error so callers can distinguish error classes
 *    programmatically.
 *
 *    Shared between src/extension.c and src/backend/runtime/udf_helpers.c
 *    (lifted from extension.c during I-0040 M6).
 */

#ifndef RUNTIME_GQL_ERROR_H
#define RUNTIME_GQL_ERROR_H

#include "graphqlite_sqlite.h"

#define GQL_ERR_VALIDATION    "VALIDATION_ERROR"
#define GQL_ERR_PARSE         "PARSE_ERROR"
#define GQL_ERR_EXECUTION     "EXECUTION_ERROR"
#define GQL_ERR_MEMORY        "MEMORY_ERROR"
#define GQL_ERR_INTERNAL      "INTERNAL_ERROR"
#define GQL_ERR_NOT_IMPL      "NOT_IMPLEMENTED"

/* sqlite3_result_subtype value used to tag UDF results whose payload is a
 * Cypher boolean encoded as the text "true" / "false". The JSON result
 * formatter reads this subtype back via sqlite3_value_subtype() to emit
 * JSON booleans (unquoted) instead of strings. Without the tag a text
 * cell containing "true" cannot be distinguished from a user-supplied
 * string "true" and renders incorrectly. (I-0040 M13.) */
#define GQL_SUBTYPE_BOOLEAN   0x42

/* Sentinel value stored in cypher_result.data_types[row][col] for
 * boolean-tagged cells. Picked above the SQLite type-code range
 * (SQLITE_INTEGER=1 .. SQLITE_NULL=5) so existing readers ignore it. */
#define GQL_COL_TYPE_BOOLEAN  100

/* NaN sentinel. SQLite collapses float NaN to NULL and drops subtypes across
 * CTE boundaries, so a runtime NaN value (e.g. produced by `0.0/0.0` inside an
 * UNWIND list) is carried as a private TEXT string recognized by content. The
 * leading control byte (0x01, SOH) makes collision with a real Cypher string
 * effectively impossible. The formatter renders it as unquoted `NaN`; the
 * orderability rank places it just after numbers (rank 8). (GQLITE-T-0340.) */
#define GQL_NAN_SENTINEL  "\x01NaN"

void graphqlite_result_error(sqlite3_context *context,
                             const char *message,
                             const char *code);

#endif /* RUNTIME_GQL_ERROR_H */
