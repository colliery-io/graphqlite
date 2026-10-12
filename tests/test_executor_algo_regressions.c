/*
 * test_executor_algo_regressions.c
 *
 * Regression tests for graph algorithm bugs found by the NetworkX oracle
 * tests (bindings/python/tests/test_algorithms_oracle.py): GQLITE-T-0382
 * (Louvain), GQLITE-T-0383 (label propagation), GQLITE-T-0384
 * (eigenvector), GQLITE-T-0385 (triangles).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "executor/cypher_executor.h"

/* Build a graph on a fresh database from `nodes` (single-letter ids) and
 * `edges` ("ab" = a -> b), run `query`, return the first cell. */
static char *algo_run(const char *nodes, const char **edges, const char *query)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return NULL;
    cypher_executor *ex = cypher_executor_create(db);
    char *out = NULL;
    if (ex) {
        char q[256];
        for (const char *n = nodes; *n; n++) {
            snprintf(q, sizeof(q), "CREATE (:N {id: '%c'})", *n);
            cypher_result *r = cypher_executor_execute(ex, q);
            if (r) cypher_result_free(r);
        }
        for (int i = 0; edges[i]; i++) {
            snprintf(q, sizeof(q), "MATCH (a:N {id: '%c'}), (b:N {id: '%c'}) CREATE (a)-[:E]->(b)",
                     edges[i][0], edges[i][1]);
            cypher_result *r = cypher_executor_execute(ex, q);
            if (r) cypher_result_free(r);
        }
        cypher_result *r = cypher_executor_execute(ex, query);
        if (r && r->success && r->row_count > 0 && r->data[0][0]) out = strdup(r->data[0][0]);
        if (r) cypher_result_free(r);
        cypher_executor_free(ex);
    }
    sqlite3_close(db);
    return out;
}

/* Value of `key` (an integer) in the result object for user id `id`. */
static int int_for(const char *json, char id, const char *key)
{
    char needle[32];
    snprintf(needle, sizeof(needle), "\"user_id\":\"%c\"", id);
    const char *p = strstr(json, needle);
    if (!p) return -999;
    char k[40];
    snprintf(k, sizeof(k), "\"%s\":", key);
    p = strstr(p, k);
    return p ? atoi(p + strlen(k)) : -999;
}

static double score_for(const char *json, char id)
{
    char needle[32];
    snprintf(needle, sizeof(needle), "\"user_id\":\"%c\"", id);
    const char *p = strstr(json, needle);
    if (!p || !(p = strstr(p, "\"score\":"))) return -1.0;
    return atof(p + 8);
}

/* A self-loop is not a triangle (GQLITE-T-0385). */
static void test_triangles_ignore_self_loop(void)
{
    const char *edges[] = {"aa", "ab", NULL};
    char *json = algo_run("ab", edges, "RETURN triangleCount()");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_EQUAL(int_for(json, 'a', "triangles"), 0);
    free(json);
}

/* A 3-cycle and a complete graph are one community (GQLITE-T-0382). */
static void test_louvain_does_not_split_a_clique(void)
{
    const char *cycle[] = {"ab", "bc", "ca", NULL};
    char *json = algo_run("abc", cycle, "RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_EQUAL(int_for(json, 'a', "community"), int_for(json, 'b', "community"));
    CU_ASSERT_EQUAL(int_for(json, 'a', "community"), int_for(json, 'c', "community"));
    free(json);

    const char *complete[] = {"ab", "ac", "ad", "ba", "bc", "bd", "ca", "cb", "cd", "da", "db", "dc", NULL};
    json = algo_run("abcd", complete, "RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    for (const char *n = "bcd"; *n; n++) {
        CU_ASSERT_EQUAL(int_for(json, 'a', "community"), int_for(json, *n, "community"));
    }
    free(json);
}

/* Two triangles joined by one edge: two communities. */
static void test_louvain_two_triangles(void)
{
    const char *edges[] = {"ab", "bc", "ca", "de", "ef", "fd", "cd", NULL};
    char *json = algo_run("abcdef", edges, "RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_EQUAL(int_for(json, 'a', "community"), int_for(json, 'c', "community"));
    CU_ASSERT_EQUAL(int_for(json, 'd', "community"), int_for(json, 'f', "community"));
    CU_ASSERT_NOT_EQUAL(int_for(json, 'a', "community"), int_for(json, 'd', "community"));
    free(json);
}

/* An out-star is one community, in the C and the SQL form (GQLITE-T-0383). */
static void test_label_propagation_star(void)
{
    const char *edges[] = {"hx", "hy", "hz", NULL};
    const char *queries[] = {
        "RETURN labelPropagation(10)",
        "MATCH (n:N {id: 'h'}) WITH n RETURN labelPropagation(10)",
        NULL
    };
    for (int q = 0; queries[q]; q++) {
        char *json = algo_run("hxyz", edges, queries[q]);
        CU_ASSERT_PTR_NOT_NULL_FATAL(json);
        for (const char *n = "xyz"; *n; n++) {
            CU_ASSERT_EQUAL(int_for(json, 'h', "community"), int_for(json, *n, "community"));
        }
        free(json);
    }
}

/* Same result for odd and even iteration counts on a periodic graph
 * (GQLITE-T-0384): a <-> b <-> c, c -> d. */
static void test_eigenvector_no_oscillation(void)
{
    const char *edges[] = {"ab", "ba", "bc", "cb", "cd", NULL};
    char *odd = algo_run("abcd", edges, "RETURN eigenvectorCentrality(999)");
    char *even = algo_run("abcd", edges, "RETURN eigenvectorCentrality(1000)");
    CU_ASSERT_PTR_NOT_NULL_FATAL(odd);
    CU_ASSERT_PTR_NOT_NULL_FATAL(even);
    for (const char *n = "abcd"; *n; n++) {
        CU_ASSERT_DOUBLE_EQUAL(score_for(odd, *n), score_for(even, *n), 1e-6);
    }
    /* NetworkX: a 0.471, b 0.667, c 0.471, d 0.333 */
    CU_ASSERT_DOUBLE_EQUAL(score_for(even, 'b'), 0.667, 1e-3);
    CU_ASSERT_DOUBLE_EQUAL(score_for(even, 'd'), 0.333, 1e-3);
    free(odd);
    free(even);
}

int init_executor_algo_regressions_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor algorithm regressions", NULL, NULL);
    if (!suite) return CU_get_error();
    if (!CU_add_test(suite, "Triangles ignore self-loops (GQLITE-T-0385)", test_triangles_ignore_self_loop) ||
        !CU_add_test(suite, "Louvain keeps a clique together (GQLITE-T-0382)", test_louvain_does_not_split_a_clique) ||
        !CU_add_test(suite, "Louvain two triangles (GQLITE-T-0382)", test_louvain_two_triangles) ||
        !CU_add_test(suite, "Label propagation star (GQLITE-T-0383)", test_label_propagation_star) ||
        !CU_add_test(suite, "Eigenvector no oscillation (GQLITE-T-0384)", test_eigenvector_no_oscillation)) {
        return CU_get_error();
    }
    return CUE_SUCCESS;
}
