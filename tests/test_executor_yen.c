/*
 * Tests for Yen's k-shortest loopless paths (yen / kShortestPaths).
 *
 * Assertions compare the exact JSON result where the output is pinned
 * deterministically (distance order, then discovery order), and the edge
 * arrays also pin the per-edge-slot (parallel edge) semantics.  The unit
 * harness returns the algorithm's JSON object directly, as one result
 * cell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"

/*
 * Weighted test graph with three distinct A->C routes:
 *
 *   A --1--> B --2--> C
 *   A --4--> C
 *   A --2--> D --2--> C
 *
 * Edge rowids follow creation order: 1..5.
 */
static const char *W_SETUP[] = {
    "CREATE (:Node {id: 'A'})",
    "CREATE (:Node {id: 'B'})",
    "CREATE (:Node {id: 'C'})",
    "CREATE (:Node {id: 'D'})",
    "MATCH (a:Node {id: 'A'}), (b:Node {id: 'B'}) CREATE (a)-[:LINK {cost: 1}]->(b)",
    "MATCH (b:Node {id: 'B'}), (c:Node {id: 'C'}) CREATE (b)-[:LINK {cost: 2}]->(c)",
    "MATCH (a:Node {id: 'A'}), (c:Node {id: 'C'}) CREATE (a)-[:LINK {cost: 4}]->(c)",
    "MATCH (a:Node {id: 'A'}), (d:Node {id: 'D'}) CREATE (a)-[:LINK {cost: 2}]->(d)",
    "MATCH (d:Node {id: 'D'}), (c:Node {id: 'C'}) CREATE (d)-[:LINK {cost: 2}]->(c)",
    NULL
};

/*
 * Hop-cap test graph.  s->v costs 6 in 1 hop; the cheaper route to v is
 * s->x->y->v costing 5 in 3 hops, and v->t costs 2.
 *
 *   unbounded: s->x->y->v->t = 7 (4 hops)
 *   cap 3:     s->v->t       = 8 (2 hops)
 *
 * A naive Dijkstra pruned at the hop cap settles v at 5 in 3 hops, cannot
 * extend it, and would miss the 8 answer entirely.
 *
 * Edge rowids follow creation order: 1..5.
 */
static const char *HOP_SETUP[] = {
    "CREATE (:Node {id: 's'})",
    "CREATE (:Node {id: 'x'})",
    "CREATE (:Node {id: 'y'})",
    "CREATE (:Node {id: 'v'})",
    "CREATE (:Node {id: 't'})",
    "MATCH (a:Node {id: 's'}), (b:Node {id: 'x'}) CREATE (a)-[:E {cost: 1}]->(b)",
    "MATCH (a:Node {id: 'x'}), (b:Node {id: 'y'}) CREATE (a)-[:E {cost: 1}]->(b)",
    "MATCH (a:Node {id: 'y'}), (b:Node {id: 'v'}) CREATE (a)-[:E {cost: 3}]->(b)",
    "MATCH (a:Node {id: 's'}), (b:Node {id: 'v'}) CREATE (a)-[:E {cost: 6}]->(b)",
    "MATCH (a:Node {id: 'v'}), (b:Node {id: 't'}) CREATE (a)-[:E {cost: 2}]->(b)",
    NULL
};

/* A single 3-hop route p->q->r->z with no shortcuts. */
static const char *CHAIN_SETUP[] = {
    "CREATE (:Node {id: 'p'})",
    "CREATE (:Node {id: 'q'})",
    "CREATE (:Node {id: 'r'})",
    "CREATE (:Node {id: 'z'})",
    "MATCH (a:Node {id: 'p'}), (b:Node {id: 'q'}) CREATE (a)-[:E]->(b)",
    "MATCH (a:Node {id: 'q'}), (b:Node {id: 'r'}) CREATE (a)-[:E]->(b)",
    "MATCH (a:Node {id: 'r'}), (b:Node {id: 'z'}) CREATE (a)-[:E]->(b)",
    NULL
};

/* Run one query on a fresh in-memory graph built by setup.  params_json
 * may be NULL.  Returns the first cell (caller frees) or NULL. */
static char *yen_run(const char **setup, const char *query, const char *params_json)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return NULL;
    cypher_executor *ex = cypher_executor_create(db);
    char *out = NULL;
    if (ex) {
        for (int i = 0; setup[i]; i++) {
            cypher_result *r = cypher_executor_execute(ex, setup[i]);
            if (r) cypher_result_free(r);
        }
        cypher_result *r = params_json
            ? cypher_executor_execute_params(ex, query, params_json)
            : cypher_executor_execute(ex, query);
        if (r) {
            if (r->success && r->row_count > 0 && r->data && r->data[0][0]) {
                out = strdup(r->data[0][0]);
            } else if (!r->success) {
                printf("Yen error: %s\n", r->error_message);
            }
            cypher_result_free(r);
        }
        cypher_executor_free(ex);
    }
    sqlite3_close(db);
    return out;
}

/* k = 1 returns the plain Dijkstra path. */
static void test_yen_k1_matches_dijkstra(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('A', 'C', 1, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"A\",\"B\",\"C\"],\"edges\":[1,2],\"distance\":3}],\"found\":true}");
    free(json);

    char *dij = yen_run(W_SETUP, "RETURN dijkstra('A', 'C', 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(dij);
    CU_ASSERT_PTR_NOT_NULL(strstr(dij, "[\"A\",\"B\",\"C\"]"));
    CU_ASSERT_PTR_NOT_NULL(strstr(dij, "\"distance\":3"));
    free(dij);
}

/* Three weighted routes, ordered by cost then discovery. */
static void test_yen_k3_weighted(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"A\",\"B\",\"C\"],\"edges\":[1,2],\"distance\":3},"
        "{\"path\":[\"A\",\"C\"],\"edges\":[3],\"distance\":4},"
        "{\"path\":[\"A\",\"D\",\"C\"],\"edges\":[4,5],\"distance\":4}"
        "],\"found\":true}");
    free(json);
}

/* Without a weight property the cost is the hop count. */
static void test_yen_k3_unweighted(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('A', 'C', 3)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"A\",\"C\"],\"edges\":[3],\"distance\":1},"
        "{\"path\":[\"A\",\"B\",\"C\"],\"edges\":[1,2],\"distance\":2},"
        "{\"path\":[\"A\",\"D\",\"C\"],\"edges\":[4,5],\"distance\":2}"
        "],\"found\":true}");
    free(json);
}

/* k beyond the number of distinct routes returns every route found. */
static void test_yen_k_larger_than_paths(void)
{
    char *three = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(three);

    char *ten = yen_run(W_SETUP, "RETURN yen('A', 'C', 10, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(ten);
    CU_ASSERT_STRING_EQUAL(ten, three);

    free(three);
    free(ten);
}

/* k < 1 clamps to 1. */
static void test_yen_k_zero_clamped(void)
{
    char *one = yen_run(W_SETUP, "RETURN yen('A', 'C', 1, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(one);

    char *zero = yen_run(W_SETUP, "RETURN yen('A', 'C', 0, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(zero);
    CU_ASSERT_STRING_EQUAL(zero, one);

    free(one);
    free(zero);
}

/* Omitting k behaves like k = 1 (and unweighted when no property is given). */
static void test_yen_k_omitted(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('A', 'C')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"A\",\"C\"],\"edges\":[3],\"distance\":1}],\"found\":true}");
    free(json);
}

/* The trivial path from a node to itself. */
static void test_yen_same_node(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('A', 'A', 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"A\"],\"edges\":[],\"distance\":0}],\"found\":true}");
    free(json);
}

/* Unreachable target. */
static void test_yen_no_path(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('C', 'A', 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json, "{\"paths\":[],\"found\":false}");
    free(json);
}

/* Unknown node user id. */
static void test_yen_nonexistent_node(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen('A', 'Z', 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json, "{\"paths\":[],\"found\":false}");
    free(json);
}

/* Parallel edges are distinct routes: the second route keeps the same node
 * sequence but names the other edge rowid. */
static void test_yen_parallel_edges_unequal_weights(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'})",
        "CREATE (:Node {id: 'b'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 2.0}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 0.5}]->(b)",
        NULL
    };
    char *json = yen_run(setup, "RETURN yen('a', 'b', 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"a\",\"b\"],\"edges\":[2],\"distance\":0.5},"
        "{\"path\":[\"a\",\"b\"],\"edges\":[1],\"distance\":2}"
        "],\"found\":true}");
    free(json);
}

/* Equal-weight parallel edges are still three distinct routes. */
static void test_yen_parallel_edges_equal_weights(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'})",
        "CREATE (:Node {id: 'b'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 1}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 1}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 1}]->(b)",
        NULL
    };
    char *json = yen_run(setup, "RETURN yen('a', 'b', 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"a\",\"b\"],\"edges\":[1],\"distance\":1},"
        "{\"path\":[\"a\",\"b\"],\"edges\":[2],\"distance\":1},"
        "{\"path\":[\"a\",\"b\"],\"edges\":[3],\"distance\":1}"
        "],\"found\":true}");
    free(json);
}

/* A cycle in the graph must not produce looping paths. */
static void test_yen_cycle_graph(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'})",
        "CREATE (:Node {id: 'b'})",
        "CREATE (:Node {id: 'c'})",
        "CREATE (:Node {id: 'd'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK]->(b)",
        "MATCH (b:Node {id: 'b'}), (c:Node {id: 'c'}) CREATE (b)-[:LINK]->(c)",
        "MATCH (c:Node {id: 'c'}), (a:Node {id: 'a'}) CREATE (c)-[:LINK]->(a)",
        "MATCH (a:Node {id: 'a'}), (c:Node {id: 'c'}) CREATE (a)-[:LINK]->(c)",
        "MATCH (c:Node {id: 'c'}), (d:Node {id: 'd'}) CREATE (c)-[:LINK]->(d)",
        NULL
    };
    char *json = yen_run(setup, "RETURN yen('a', 'd', 2)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"a\",\"c\",\"d\"],\"edges\":[4,5],\"distance\":2},"
        "{\"path\":[\"a\",\"b\",\"c\",\"d\"],\"edges\":[1,2,5],\"distance\":3}"
        "],\"found\":true}");
    free(json);
}

/* kShortestPaths is an alias of yen. */
static void test_yen_alias(void)
{
    char *yen = yen_run(W_SETUP, "RETURN yen('A', 'C', 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(yen);

    char *alias = yen_run(W_SETUP, "RETURN kShortestPaths('A', 'C', 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(alias);
    CU_ASSERT_STRING_EQUAL(alias, yen);

    free(yen);
    free(alias);
}

/* Arguments may arrive through query parameters. */
static void test_yen_parameters(void)
{
    char *json = yen_run(W_SETUP, "RETURN yen($s, $t, $k, 'cost')",
                         "{\"s\":\"A\",\"t\":\"C\",\"k\":2}");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"A\",\"B\",\"C\"],\"edges\":[1,2],\"distance\":3},"
        "{\"path\":[\"A\",\"C\"],\"edges\":[3],\"distance\":4}"
        "],\"found\":true}");
    free(json);
}

/* Repeated runs over the same graph produce byte-identical output. */
static void test_yen_deterministic_repeat(void)
{
    char *first = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(first);

    char *second = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(second);
    CU_ASSERT_STRING_EQUAL(second, first);

    free(first);
    free(second);
}

/* Validate one yen result with SQLite's own JSON parser, then round-trip
 * the first two path node IDs through json_extract. */
static void assert_json_path_ids(sqlite3 *db, const char *json,
                                 const char *id0, const char *id1)
{
    sqlite3_stmt *stmt = NULL;

    CU_ASSERT_EQUAL_FATAL(sqlite3_prepare_v2(db, "SELECT json_valid(?1)", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, json, -1, SQLITE_TRANSIENT);
    CU_ASSERT_EQUAL_FATAL(sqlite3_step(stmt), SQLITE_ROW);
    CU_ASSERT_EQUAL_FATAL(sqlite3_column_int(stmt, 0), 1);
    CU_ASSERT_EQUAL(sqlite3_finalize(stmt), SQLITE_OK);
    stmt = NULL;

    CU_ASSERT_EQUAL_FATAL(sqlite3_prepare_v2(db,
        "SELECT json_extract(?1, '$.paths[0].path[0]'), "
        "json_extract(?1, '$.paths[0].path[1]')", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, json, -1, SQLITE_TRANSIENT);
    CU_ASSERT_EQUAL_FATAL(sqlite3_step(stmt), SQLITE_ROW);
    const unsigned char *got0 = sqlite3_column_text(stmt, 0);
    const unsigned char *got1 = sqlite3_column_text(stmt, 1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(got0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(got1);
    CU_ASSERT_STRING_EQUAL((const char *)got0, id0);
    CU_ASSERT_STRING_EQUAL((const char *)got1, id1);
    CU_ASSERT_EQUAL(sqlite3_finalize(stmt), SQLITE_OK);
}

/* Node IDs are arbitrary text; the JSON result must escape quotes,
 * backslashes and control characters instead of corrupting or breaking the
 * document.  Checked by exact output and by SQLite's JSON parser
 * round-tripping the IDs. */
static void test_yen_hostile_node_ids(void)
{
    sqlite3 *db = NULL;
    CU_ASSERT_EQUAL_FATAL(sqlite3_open(":memory:", &db), SQLITE_OK);
    cypher_executor *ex = cypher_executor_create(db);
    CU_ASSERT_PTR_NOT_NULL_FATAL(ex);

    /* quote, newline, backslash, backspace */
    const char *creates[] = {
        "{\"id\": \"a\\\"b\"}",
        "{\"id\": \"r\\ne\"}",
        "{\"id\": \"back\\\\slash\"}",
        "{\"id\": \"ctl\\bx\"}",
    };
    for (size_t i = 0; i < sizeof(creates) / sizeof(creates[0]); i++) {
        cypher_result *r = cypher_executor_execute_params(ex, "CREATE (:N {id: $id})", creates[i]);
        CU_ASSERT_PTR_NOT_NULL_FATAL(r);
        CU_ASSERT_TRUE_FATAL(r->success);
        cypher_result_free(r);
    }

    cypher_result *r = cypher_executor_execute_params(ex,
        "MATCH (a:N {id: $u}), (b:N {id: $v}) CREATE (a)-[:E]->(b)",
        "{\"u\": \"a\\\"b\", \"v\": \"r\\ne\"}");
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_TRUE_FATAL(r->success);
    cypher_result_free(r);

    r = cypher_executor_execute_params(ex,
        "MATCH (a:N {id: $u}), (b:N {id: $v}) CREATE (a)-[:E]->(b)",
        "{\"u\": \"back\\\\slash\", \"v\": \"ctl\\bx\"}");
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_TRUE_FATAL(r->success);
    cypher_result_free(r);

    /* Quote and newline round-trip through the escaped JSON. */
    r = cypher_executor_execute_params(ex, "RETURN yen($s, $t, 1)",
                                       "{\"s\": \"a\\\"b\", \"t\": \"r\\ne\"}");
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_TRUE_FATAL(r->success);
    CU_ASSERT_PTR_NOT_NULL_FATAL(r->data);
    const char *json = r->data[0][0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"a\\\"b\",\"r\\ne\"],\"edges\":[1],\"distance\":1}],\"found\":true}");
    assert_json_path_ids(db, json, "a\"b", "r\ne");
    cypher_result_free(r);

    /* Backslash and backspace must not be silently corrupted. */
    r = cypher_executor_execute_params(ex, "RETURN yen($s, $t, 1)",
                                       "{\"s\": \"back\\\\slash\", \"t\": \"ctl\\bx\"}");
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_TRUE_FATAL(r->success);
    CU_ASSERT_PTR_NOT_NULL_FATAL(r->data);
    json = r->data[0][0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"back\\\\slash\",\"ctl\\bx\"],\"edges\":[2],\"distance\":1}],\"found\":true}");
    assert_json_path_ids(db, json, "back\\slash", "ctl\bx");
    cypher_result_free(r);

    cypher_executor_free(ex);
    sqlite3_close(db);
}

/* The hop cap must be exact: the cheaper-to-v route is out of budget, the
 * costlier shorter one is the answer (see HOP_SETUP). */
static void test_yen_max_hops_exactness(void)
{
    char *unbounded = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(unbounded);
    CU_ASSERT_STRING_EQUAL(unbounded,
        "{\"paths\":["
        "{\"path\":[\"s\",\"x\",\"y\",\"v\",\"t\"],\"edges\":[1,2,3,5],\"distance\":7}"
        "],\"found\":true}");
    free(unbounded);

    char *bounded = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(bounded);
    CU_ASSERT_STRING_EQUAL(bounded,
        "{\"paths\":["
        "{\"path\":[\"s\",\"v\",\"t\"],\"edges\":[4,5],\"distance\":8}"
        "],\"found\":true}");
    free(bounded);
}

/* The two optional arguments are typed, order-independent, null-skippable. */
static void test_yen_max_hops_argument_forms(void)
{
    char *hops_first = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(hops_first);

    char *weight_first = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 'cost', 3)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(weight_first);
    CU_ASSERT_STRING_EQUAL(weight_first, hops_first);
    free(weight_first);

    char *null_first = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, null, 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(null_first);
    CU_ASSERT_STRING_EQUAL(null_first, hops_first);
    free(null_first);

    free(hops_first);
}

/* A hop cap that no route fits reports not found. */
static void test_yen_max_hops_excludes_all(void)
{
    char *json = yen_run(CHAIN_SETUP, "RETURN yen('p', 'z', 1, 2)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json, "{\"paths\":[],\"found\":false}");
    free(json);

    char *within = yen_run(CHAIN_SETUP, "RETURN yen('p', 'z', 1, 3)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(within);
    CU_ASSERT_STRING_EQUAL(within,
        "{\"paths\":[{\"path\":[\"p\",\"q\",\"r\",\"z\"],\"edges\":[1,2,3],\"distance\":3}],\"found\":true}");
    free(within);
}

/* Without a weight property the cap counts hops. */
static void test_yen_max_hops_unweighted(void)
{
    char *json = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 3)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"s\",\"v\",\"t\"],\"edges\":[4,5],\"distance\":2}],\"found\":true}");
    free(json);

    /* The same route needs 2 hops; a cap of 1 excludes it entirely. */
    char *too_short = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 1)", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(too_short);
    CU_ASSERT_STRING_EQUAL(too_short, "{\"paths\":[],\"found\":false}");
    free(too_short);
}

/* Zero and over-large caps fall back to the unbounded search. */
static void test_yen_max_hops_unbounded_forms(void)
{
    char *unbounded = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(unbounded);

    char *zero = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 0, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(zero);
    CU_ASSERT_STRING_EQUAL(zero, unbounded);
    free(zero);

    /* node_count - 1 = 4: a cap of 100 cannot exclude anything. */
    char *large = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 100, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(large);
    CU_ASSERT_STRING_EQUAL(large, unbounded);
    free(large);

    free(unbounded);
}

/* The cap applies across k routes and keeps their ordering. */
static void test_yen_max_hops_with_k(void)
{
    /* W: all three A->C routes are within 2 hops, so the result equals the
     * unbounded one. */
    char *unbounded = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(unbounded);

    char *bounded = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 2, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(bounded);
    CU_ASSERT_STRING_EQUAL(bounded, unbounded);
    free(bounded);
    free(unbounded);

    /* Cap 1 leaves only the direct edge. */
    char *direct = yen_run(W_SETUP, "RETURN yen('A', 'C', 3, 1, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(direct);
    CU_ASSERT_STRING_EQUAL(direct,
        "{\"paths\":[{\"path\":[\"A\",\"C\"],\"edges\":[3],\"distance\":4}],\"found\":true}");
    free(direct);
}

/* Parallel edges stay distinct routes under a cap. */
static void test_yen_max_hops_parallel_edges(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'})",
        "CREATE (:Node {id: 'b'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 2.0}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 0.5}]->(b)",
        NULL
    };
    char *json = yen_run(setup, "RETURN yen('a', 'b', 2, 1, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":["
        "{\"path\":[\"a\",\"b\"],\"edges\":[2],\"distance\":0.5},"
        "{\"path\":[\"a\",\"b\"],\"edges\":[1],\"distance\":2}"
        "],\"found\":true}");
    free(json);
}

/* Zero-weight edges must not break the hop-capped reconstruction (loopless
 * output, exact cost). */
static void test_yen_max_hops_zero_weights(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'})",
        "CREATE (:Node {id: 'b'})",
        "CREATE (:Node {id: 't'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:E {cost: 0}]->(b)",
        "MATCH (a:Node {id: 'b'}), (b:Node {id: 'a'}) CREATE (a)-[:E {cost: 0}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 't'}) CREATE (a)-[:E {cost: 1}]->(b)",
        NULL
    };
    char *json = yen_run(setup, "RETURN yen('a', 't', 1, 5, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_STRING_EQUAL(json,
        "{\"paths\":[{\"path\":[\"a\",\"t\"],\"edges\":[3],\"distance\":1}],\"found\":true}");
    free(json);
}

/* The hop cap may arrive through a parameter. */
static void test_yen_max_hops_parameter(void)
{
    char *literal = yen_run(HOP_SETUP, "RETURN yen('s', 't', 1, 3, 'cost')", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(literal);

    char *parameter = yen_run(HOP_SETUP, "RETURN yen($s, $t, $k, $h, $w)",
                              "{\"s\":\"s\",\"t\":\"t\",\"k\":1,\"h\":3,\"w\":\"cost\"}");
    CU_ASSERT_PTR_NOT_NULL_FATAL(parameter);
    CU_ASSERT_STRING_EQUAL(parameter, literal);

    free(literal);
    free(parameter);
}

static int setup_yen_suite(void)
{
    return 0;
}

static int teardown_yen_suite(void)
{
    return 0;
}

/* Initialize the Yen executor test suite */
int init_executor_yen_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Yen", setup_yen_suite, teardown_yen_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "Yen k=1 matches Dijkstra", test_yen_k1_matches_dijkstra) ||
        !CU_add_test(suite, "Yen k=3 weighted", test_yen_k3_weighted) ||
        !CU_add_test(suite, "Yen k=3 unweighted", test_yen_k3_unweighted) ||
        !CU_add_test(suite, "Yen k larger than paths", test_yen_k_larger_than_paths) ||
        !CU_add_test(suite, "Yen k=0 clamped to 1", test_yen_k_zero_clamped) ||
        !CU_add_test(suite, "Yen k omitted", test_yen_k_omitted) ||
        !CU_add_test(suite, "Yen same node", test_yen_same_node) ||
        !CU_add_test(suite, "Yen no path", test_yen_no_path) ||
        !CU_add_test(suite, "Yen nonexistent node", test_yen_nonexistent_node) ||
        !CU_add_test(suite, "Yen parallel edges unequal weights", test_yen_parallel_edges_unequal_weights) ||
        !CU_add_test(suite, "Yen parallel edges equal weights", test_yen_parallel_edges_equal_weights) ||
        !CU_add_test(suite, "Yen cycle graph", test_yen_cycle_graph) ||
        !CU_add_test(suite, "Yen kShortestPaths alias", test_yen_alias) ||
        !CU_add_test(suite, "Yen parameters", test_yen_parameters) ||
        !CU_add_test(suite, "Yen deterministic repeat", test_yen_deterministic_repeat) ||
        !CU_add_test(suite, "Yen hostile node IDs", test_yen_hostile_node_ids) ||
        !CU_add_test(suite, "Yen max hops exactness", test_yen_max_hops_exactness) ||
        !CU_add_test(suite, "Yen max hops argument forms", test_yen_max_hops_argument_forms) ||
        !CU_add_test(suite, "Yen max hops excludes all", test_yen_max_hops_excludes_all) ||
        !CU_add_test(suite, "Yen max hops unweighted", test_yen_max_hops_unweighted) ||
        !CU_add_test(suite, "Yen max hops unbounded forms", test_yen_max_hops_unbounded_forms) ||
        !CU_add_test(suite, "Yen max hops with k", test_yen_max_hops_with_k) ||
        !CU_add_test(suite, "Yen max hops parallel edges", test_yen_max_hops_parallel_edges) ||
        !CU_add_test(suite, "Yen max hops zero weights", test_yen_max_hops_zero_weights) ||
        !CU_add_test(suite, "Yen max hops parameter", test_yen_max_hops_parameter)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
