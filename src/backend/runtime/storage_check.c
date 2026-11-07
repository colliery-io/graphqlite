/*
 * gql_check_storage(): consistency of the stored graph (GQLITE-T-0525).
 *
 * Returns a JSON array of violations, `[]` when the storage is consistent.
 * Each violation is {"check": name, "count": n, "sample": [ids...]} with up
 * to five sample ids (or id pairs). Test harnesses call it after each test
 * case; the constraints of graphqlite_schema_objects are checked by
 * gql_check_constraints().
 *
 * The checks hold with or without PRAGMA foreign_keys: SQLite enforces the
 * REFERENCES clauses of the schema only when that pragma is on.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "graphqlite_sqlite.h"
#include "runtime/storage_check.h"

typedef struct {
    const char *name;
    const char *sql;   /* rows of offending ids (one or two columns) */
} storage_check;

#define ORPHAN_NODE(t) \
    { "orphan_" t, "SELECT node_id FROM " t " WHERE node_id NOT IN (SELECT id FROM nodes)" }
#define ORPHAN_EDGE(t) \
    { "orphan_" t, "SELECT edge_id FROM " t " WHERE edge_id NOT IN (SELECT id FROM edges)" }
#define BAD_KEY(t) \
    { "unknown_key_" t, "SELECT key_id FROM " t " WHERE key_id NOT IN (SELECT id FROM property_keys)" }

static const storage_check CHECKS[] = {
    { "edge_source_missing", "SELECT id FROM edges WHERE source_id NOT IN (SELECT id FROM nodes)" },
    { "edge_target_missing", "SELECT id FROM edges WHERE target_id NOT IN (SELECT id FROM nodes)" },
    { "edge_type_empty", "SELECT id FROM edges WHERE type IS NULL OR type = ''" },
    { "label_empty", "SELECT node_id FROM node_labels WHERE label IS NULL OR label = ''" },
    ORPHAN_NODE("node_labels"),
    ORPHAN_NODE("node_props_int"), ORPHAN_NODE("node_props_text"), ORPHAN_NODE("node_props_real"),
    ORPHAN_NODE("node_props_bool"), ORPHAN_NODE("node_props_json"),
    ORPHAN_EDGE("edge_props_int"), ORPHAN_EDGE("edge_props_text"), ORPHAN_EDGE("edge_props_real"),
    ORPHAN_EDGE("edge_props_bool"), ORPHAN_EDGE("edge_props_json"),
    BAD_KEY("node_props_int"), BAD_KEY("node_props_text"), BAD_KEY("node_props_real"),
    BAD_KEY("node_props_bool"), BAD_KEY("node_props_json"),
    BAD_KEY("edge_props_int"), BAD_KEY("edge_props_text"), BAD_KEY("edge_props_real"),
    BAD_KEY("edge_props_bool"), BAD_KEY("edge_props_json"),
    /* One property of one entity lives in exactly one type table. */
    { "node_property_in_two_tables",
      "SELECT node_id, key_id FROM ("
      "SELECT node_id, key_id FROM node_props_int UNION ALL SELECT node_id, key_id FROM node_props_text "
      "UNION ALL SELECT node_id, key_id FROM node_props_real UNION ALL SELECT node_id, key_id FROM node_props_bool "
      "UNION ALL SELECT node_id, key_id FROM node_props_json) GROUP BY node_id, key_id HAVING count(*) > 1" },
    { "edge_property_in_two_tables",
      "SELECT edge_id, key_id FROM ("
      "SELECT edge_id, key_id FROM edge_props_int UNION ALL SELECT edge_id, key_id FROM edge_props_text "
      "UNION ALL SELECT edge_id, key_id FROM edge_props_real UNION ALL SELECT edge_id, key_id FROM edge_props_bool "
      "UNION ALL SELECT edge_id, key_id FROM edge_props_json) GROUP BY edge_id, key_id HAVING count(*) > 1" },
    /* Values of the right storage class for their table. */
    { "int_table_not_integer",
      "SELECT node_id FROM node_props_int WHERE typeof(value) <> 'integer' "
      "UNION ALL SELECT edge_id FROM edge_props_int WHERE typeof(value) <> 'integer'" },
    { "real_table_not_real",
      "SELECT node_id FROM node_props_real WHERE typeof(value) NOT IN ('real', 'integer') "
      "UNION ALL SELECT edge_id FROM edge_props_real WHERE typeof(value) NOT IN ('real', 'integer')" },
    { "text_table_not_text",
      "SELECT node_id FROM node_props_text WHERE typeof(value) <> 'text' "
      "UNION ALL SELECT edge_id FROM edge_props_text WHERE typeof(value) <> 'text'" },
    { "json_table_not_json",
      "SELECT node_id FROM node_props_json WHERE NOT json_valid(value) "
      "UNION ALL SELECT edge_id FROM edge_props_json WHERE NOT json_valid(value)" },
    { "integrity_check",
      "SELECT integrity_check FROM pragma_integrity_check WHERE integrity_check <> 'ok'" },
    { "foreign_key_check",
      "SELECT \"table\" || ':' || rowid FROM pragma_foreign_key_check" },
};

static void append_json_string(sqlite3_str *s, const char *t)
{
    sqlite3_str_appendchar(s, 1, '"');
    for (const unsigned char *p = (const unsigned char *)(t ? t : ""); *p; p++) {
        if (*p == '"' || *p == '\\') sqlite3_str_appendf(s, "\\%c", *p);
        else if (*p < 0x20) sqlite3_str_appendf(s, "\\u%04x", *p);
        else sqlite3_str_appendchar(s, 1, (char)*p);
    }
    sqlite3_str_appendchar(s, 1, '"');
}

static int table_exists(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;
    int found = 0;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
        found = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return found;
}

char *gql_check_storage_db(sqlite3 *db)
{
    sqlite3_str *out = sqlite3_str_new(db);
    sqlite3_str_appendchar(out, 1, '[');
    /* A database GraphQLite has not written to yet has nothing to check. */
    if (!table_exists(db, "nodes") || !table_exists(db, "edges")) {
        sqlite3_str_appendchar(out, 1, ']');
        return sqlite3_str_finish(out);
    }
    int n_bad = 0;
    for (size_t c = 0; c < sizeof(CHECKS) / sizeof(CHECKS[0]); c++) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db, CHECKS[c].sql, -1, &st, NULL) != SQLITE_OK) {
            /* a table of an older schema that is not there: skip */
            sqlite3_finalize(st);
            continue;
        }
        int count = 0;
        sqlite3_str *sample = sqlite3_str_new(db);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (count < 5) {
                if (count) sqlite3_str_appendchar(sample, 1, ',');
                int cols = sqlite3_column_count(st);
                if (cols > 1) sqlite3_str_appendchar(sample, 1, '[');
                for (int k = 0; k < cols; k++) {
                    if (k) sqlite3_str_appendchar(sample, 1, ',');
                    if (sqlite3_column_type(st, k) == SQLITE_INTEGER)
                        sqlite3_str_appendf(sample, "%lld", (long long)sqlite3_column_int64(st, k));
                    else
                        append_json_string(sample, (const char *)sqlite3_column_text(st, k));
                }
                if (cols > 1) sqlite3_str_appendchar(sample, 1, ']');
            }
            count++;
        }
        sqlite3_finalize(st);
        char *s = sqlite3_str_finish(sample);
        if (count > 0) {
            sqlite3_str_appendf(out, "%s{\"check\":\"%s\",\"count\":%d,\"sample\":[%s]}",
                                n_bad ? "," : "", CHECKS[c].name, count, s ? s : "");
            n_bad++;
        }
        sqlite3_free(s);
    }
    sqlite3_str_appendchar(out, 1, ']');
    return sqlite3_str_finish(out);
}

void gql_check_storage_func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    (void)argc; (void)argv;
    char *r = gql_check_storage_db(sqlite3_context_db_handle(context));
    if (!r) { sqlite3_result_error_nomem(context); return; }
    sqlite3_result_text(context, r, -1, sqlite3_free);
}

/* _gql_f64(M, E): M * 2^E (GQLITE-T-0531, gql_sql_real) */
void gql_f64_func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 2 || sqlite3_value_type(argv[0]) == SQLITE_NULL) { sqlite3_result_null(context); return; }
    sqlite3_result_double(context, ldexp((double)sqlite3_value_int64(argv[0]), sqlite3_value_int(argv[1])));
}
