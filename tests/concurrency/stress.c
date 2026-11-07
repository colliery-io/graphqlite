/*
 * Concurrency stress test (GQLITE-T-0528).
 *
 *   stress DB EXT [threads] [seconds] [conns|shared]
 *
 * conns:  each thread opens its own connection to DB (WAL) and loads EXT.
 * shared: all threads use one connection opened with SQLITE_OPEN_FULLMUTEX.
 *
 * Every thread runs a random mix for the given time:
 *   - reads of a fixed seed graph whose answers were recorded on one
 *     thread first (a different answer is a failure);
 *   - writes tagged with the thread id (CREATE, UNWIND CREATE with COUNT { },
 *     FOREACH MERGE, SET, DETACH DELETE), which use the engine's
 *     thread-local state (FOREACH context, eval errors, comprehension depth);
 *   - a query that must fail (integer overflow) followed by one that must
 *     not (the error state is per thread and per query).
 * At the end: per-thread MERGE uniqueness, the count of each thread's
 * nodes, and gql_check_storage(). Build it and the extension with
 * -fsanitize=thread to look for data races (make concurrency-tsan).
 *
 * Exit code 0 when every check held, 1 otherwise.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sqlite3.h"

static const char *DB;
static const char *EXT;
static int SECONDS = 10;
static int SHARED = 0;
static sqlite3 *g_shared;

static const char *READS[] = {
    "MATCH (s:S) WHERE s.i <= 50 RETURN count(*) AS c, sum(s.i) AS t",
    "MATCH (a:S)-[r:R]->(b:S) RETURN a.i AS a, r.w AS w, b.i AS b ORDER BY a LIMIT 5",
    "MATCH (a:S {i: 7}) RETURN [(a)-->(b) | b.i] AS l, [x IN [1, 2] | a.i * x] AS m",
    "MATCH (a:S) WHERE a.i < 4 RETURN a.i AS i, CASE WHEN a.i > 2 THEN 'big' ELSE 'small' END AS k ORDER BY i",
    "UNWIND range(1, 5) AS x WITH x WHERE x % 2 = 1 RETURN collect(x) AS l, max(x > 2) AS b",
    "MATCH (a:S {i: 3}) RETURN EXISTS { (a)-->() } AS e, COUNT { (a)-->() } AS n",
    "MATCH (a:S {i: 9}) RETURN [(a)-->(b) | [(b)<--(c) | c.i]] AS nested",
    "RETURN toString(1.5) AS s, 0.0 / 0.0 AS nan, [1, 'a', true] AS l",
};
#define N_READS (sizeof(READS) / sizeof(READS[0]))
static char *g_ref[N_READS];

static atomic_int g_fail;
static atomic_long g_reads, g_writes, g_busy, g_errors_ok;

static char *run(sqlite3 *db, const char *q, int *rc_out)
{
    sqlite3_stmt *st = NULL;
    char *out = NULL;
    /* The error message belongs to the connection: on a shared connection
     * another thread can replace it between the step and the read, so hold
     * the connection's mutex across both (SQLite's documented way). */
    sqlite3_mutex *mx = sqlite3_db_mutex(db);
    if (mx) sqlite3_mutex_enter(mx);
    int rc = sqlite3_prepare_v2(db, "SELECT cypher(?1)", -1, &st, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(st, 1, q, -1, SQLITE_STATIC);
        rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            const char *t = (const char *)sqlite3_column_text(st, 0);
            out = strdup(t ? t : "");
            rc = SQLITE_OK;
        } else {
            out = strdup(sqlite3_errmsg(db));
        }
    } else {
        out = strdup(sqlite3_errmsg(db));
    }
    sqlite3_finalize(st);
    if (mx) sqlite3_mutex_leave(mx);
    if (rc_out) *rc_out = rc;
    return out;
}

static sqlite3 *open_conn(void)
{
    sqlite3 *db = NULL;
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(DB, &db, flags, NULL) != SQLITE_OK) {
        fprintf(stderr, "open: %s\n", sqlite3_errmsg(db));
        exit(2);
    }
    sqlite3_busy_timeout(db, 10000);
    sqlite3_enable_load_extension(db, 1);
    char *err = NULL;
    if (sqlite3_load_extension(db, EXT, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "load %s: %s\n", EXT, err ? err : "?");
        exit(2);
    }
    return db;
}

static int is_busy(const char *msg)
{
    return msg && (strstr(msg, "locked") || strstr(msg, "busy") || strstr(msg, "BUSY"));
}

static void fail(int t, const char *what, const char *q, const char *got, const char *want)
{
    atomic_fetch_add(&g_fail, 1);
    if (atomic_load(&g_fail) <= 10)
        fprintf(stderr, "thread %d: %s\n  query: %s\n  got:   %.300s\n  want:  %.300s\n",
                t, what, q, got ? got : "(null)", want ? want : "");
}

typedef struct { int id; long created; long deleted; } worker;

static void *work(void *arg)
{
    worker *w = arg;
    unsigned seed = (unsigned)(w->id * 7919 + 17);
    sqlite3 *db = SHARED ? g_shared : open_conn();
    time_t end = time(NULL) + SECONDS;
    char q[512];
    long c = 0;
    while (time(NULL) < end) {
        int op = rand_r(&seed) % 10;
        int rc;
        char *r;
        if (op < 4) {
            size_t k = (size_t)rand_r(&seed) % N_READS;
            r = run(db, READS[k], &rc);
            if (rc == SQLITE_OK && strcmp(r, g_ref[k]) != 0)
                fail(w->id, "read differs", READS[k], r, g_ref[k]);
            else if (rc != SQLITE_OK && !is_busy(r))
                fail(w->id, "read failed", READS[k], r, NULL);
            if (rc != SQLITE_OK && is_busy(r)) atomic_fetch_add(&g_busy, 1);
            atomic_fetch_add(&g_reads, 1);
            free(r);
            continue;
        }
        c++;
        switch (op) {
            case 4:
                snprintf(q, sizeof(q), "CREATE (:W {t: %d, c: %ld, l: [%d, %ld]})", w->id, c, w->id, c);
                break;
            case 5:
                snprintf(q, sizeof(q),
                         "UNWIND [1, 2] AS i CREATE (:W {t: %d, c: %ld, i: i, n: COUNT { MATCH (:S {i: 1}) }})",
                         w->id, c);
                break;
            case 6:
                snprintf(q, sizeof(q), "FOREACH (k IN [%ld %% 10, (%ld + 1) %% 10] | MERGE (:M {t: %d, k: k}))",
                         c, c, w->id);
                break;
            case 7:
                snprintf(q, sizeof(q), "MATCH (n:W {t: %d}) WHERE n.c %% 3 = 0 SET n.seen = %ld", w->id, c);
                break;
            case 8:
                snprintf(q, sizeof(q), "MATCH (n:W {t: %d}) WHERE n.c %% 5 = 0 DETACH DELETE n", w->id);
                break;
            default: {
                r = run(db, "RETURN 9223372036854775807 + 1 AS x", &rc);
                if (rc == SQLITE_OK || !strstr(r, "integer overflow")) {
                    if (!is_busy(r)) fail(w->id, "overflow did not fail", "RETURN 9223372036854775807 + 1", r, "integer overflow");
                } else {
                    atomic_fetch_add(&g_errors_ok, 1);
                }
                free(r);
                r = run(db, "UNWIND [41] AS x RETURN x + 1 AS y", &rc);
                if (rc == SQLITE_OK && strcmp(r, "[{\"y\":42}]") != 0)
                    fail(w->id, "query after an error", "UNWIND [41] AS x RETURN x + 1 AS y", r, "[{\"y\":42}]");
                free(r);
                continue;
            }
        }
        /* A write takes the write lock first (BEGIN IMMEDIATE, with the
         * busy timeout). Without it, `SELECT cypher('CREATE ...')` starts as
         * a read and its upgrade to a write fails at once when another
         * connection wrote meanwhile (WAL: SQLITE_BUSY_SNAPSHOT). */
        if (!SHARED) {
            while (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_BUSY)
                atomic_fetch_add(&g_busy, 1);
        }
        r = run(db, q, &rc);
        if (rc != SQLITE_OK) {
            if (is_busy(r)) atomic_fetch_add(&g_busy, 1);
            else fail(w->id, "write failed", q, r, NULL);
        }
        if (!SHARED) sqlite3_exec(db, rc == SQLITE_OK ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
        atomic_fetch_add(&g_writes, 1);
        free(r);
    }
    if (!SHARED) sqlite3_close(db);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s DB EXT [threads] [seconds] [conns|shared]\n", argv[0]);
        return 2;
    }
    DB = argv[1];
    EXT = argv[2];
    int nthreads = argc > 3 ? atoi(argv[3]) : 8;
    SECONDS = argc > 4 ? atoi(argv[4]) : 10;
    SHARED = argc > 5 && strcmp(argv[5], "shared") == 0;
    unlink(DB);

    sqlite3 *setup = open_conn();
    sqlite3_exec(setup, "PRAGMA journal_mode = WAL", NULL, NULL, NULL);
    int rc;
    char *r = run(setup, "UNWIND range(1, 50) AS i CREATE (:S {i: i})-[:R {w: i * 2}]->(:S {i: i + 1000})", &rc);
    free(r);
    for (size_t k = 0; k < N_READS; k++) {
        g_ref[k] = run(setup, READS[k], &rc);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "reference query failed: %s: %s\n", READS[k], g_ref[k]);
            return 2;
        }
    }
    if (SHARED) g_shared = setup;

    pthread_t th[64];
    worker ws[64];
    if (nthreads > 64) nthreads = 64;
    for (int t = 0; t < nthreads; t++) {
        ws[t] = (worker){ .id = t + 1 };
        pthread_create(&th[t], NULL, work, &ws[t]);
    }
    for (int t = 0; t < nthreads; t++) pthread_join(th[t], NULL);

    sqlite3 *db = SHARED ? setup : open_conn();
    /* MERGE inside one thread: one :M per (t, k) */
    r = run(db, "MATCH (m:M) WITH m.t AS t, m.k AS k, count(*) AS n WHERE n > 1 RETURN t, k, n", &rc);
    if (rc != SQLITE_OK || strcmp(r, "[]") != 0) fail(0, "MERGE made duplicates", "MATCH (m:M) ...", r, "[]");
    free(r);
    /* UNWIND CREATE wrote both rows of each batch */
    r = run(db, "MATCH (n:W) WHERE n.i IS NOT NULL WITH n.t AS t, n.c AS c, count(*) AS k WHERE k <> 2 RETURN t, c, k", &rc);
    if (rc != SQLITE_OK || strcmp(r, "[]") != 0) fail(0, "partial UNWIND CREATE", "MATCH (n:W) ...", r, "[]");
    free(r);
    r = run(db, "MATCH (n:W) WHERE n.n IS NOT NULL AND n.n <> 1 RETURN count(*) AS c", &rc);
    if (rc != SQLITE_OK || strcmp(r, "[{\"c\":0}]") != 0) fail(0, "COUNT { } in CREATE saw another graph", "MATCH (n:W) ...", r, "[{\"c\":0}]");
    free(r);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT gql_check_storage()", -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        const char *v = (const char *)sqlite3_column_text(st, 0);
        if (!v || strcmp(v, "[]") != 0) fail(0, "storage inconsistent", "gql_check_storage()", v, "[]");
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    if (!SHARED) sqlite3_close(setup);

    printf("concurrency (%s, %d threads, %ds): %ld reads, %ld writes, %ld expected errors, %ld busy retries, %d failures\n",
           SHARED ? "shared connection" : "connection per thread", nthreads, SECONDS,
           (long)g_reads, (long)g_writes, (long)g_errors_ok, (long)g_busy, (int)g_fail);
    for (size_t k = 0; k < N_READS; k++) free(g_ref[k]);
    return atomic_load(&g_fail) ? 1 : 0;
}
