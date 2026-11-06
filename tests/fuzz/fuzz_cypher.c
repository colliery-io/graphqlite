/*
 * libFuzzer target: one Cypher query per input, run through cypher() on an
 * in-memory database with a small graph (GQLITE-T-0496).
 *
 * The extension is built with -fsanitize=fuzzer-no-link,address,undefined
 * and loaded with sqlite3_load_extension (path: GQLITE_FUZZ_EXT, default
 * build/fuzz/graphqlite). Each run is wrapped in BEGIN ... ROLLBACK so runs
 * do not change each other's graph; a progress handler stops a query after
 * a fixed number of VM steps, so an expensive but finite query is not a
 * timeout (a real hang in C code still is).
 *
 * Build and run: `angreal test fuzz` (Linux, or a clang with libFuzzer).
 */
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3 *db;
static sqlite3_stmt *query_stmt;
static long long steps;

#define MAX_STEPS 2000000LL   /* progress callbacks are every 1000 VM ops */

static int progress(void *arg)
{
    (void)arg;
    return ++steps > MAX_STEPS / 1000;
}

static void must(int rc, const char *what)
{
    if (rc != SQLITE_OK) {
        fprintf(stderr, "fuzz setup: %s: %s\n", what, db ? sqlite3_errmsg(db) : "no db");
        abort();
    }
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc; (void)argv;
    const char *ext = getenv("GQLITE_FUZZ_EXT");
    if (!ext) ext = "build/fuzz/graphqlite";
    must(sqlite3_open(":memory:", &db), "open");
    must(sqlite3_enable_load_extension(db, 1), "enable_load_extension");
    char *err = NULL;
    if (sqlite3_load_extension(db, ext, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "fuzz setup: load %s: %s\n", ext, err ? err : "?");
        abort();
    }
    /* A small graph every query can see: labels A-C, types R and S, int,
     * float, string, bool and list properties, a self-loop and parallel
     * edges. */
    must(sqlite3_exec(db,
        "SELECT cypher('CREATE (a:A {id: 1, v: 1, s: ''x'', b: true, l: [1, 2]}), "
        "(b:B {id: 2, v: 2.5, s: ''y''}), (c:A:C {id: 3, v: -1}), (d {id: 4}), "
        "(a)-[:R {w: 1}]->(b), (b)-[:S {w: 2.0}]->(c), (c)-[:R]->(a), "
        "(a)-[:R {w: 3}]->(b), (d)-[:S]->(d)')", NULL, NULL, NULL), "graph");
    must(sqlite3_prepare_v2(db, "SELECT cypher(?)", -1, &query_stmt, NULL), "prepare");
    sqlite3_progress_handler(db, 1000, progress, NULL);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 4096) return 0;
    steps = 0;
    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    sqlite3_bind_text(query_stmt, 1, (const char *)data, (int)size, SQLITE_TRANSIENT);
    while (sqlite3_step(query_stmt) == SQLITE_ROW) {
        const unsigned char *t = sqlite3_column_text(query_stmt, 0);
        (void)t;
    }
    sqlite3_reset(query_stmt);
    sqlite3_clear_bindings(query_stmt);
    steps = 0;
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}
