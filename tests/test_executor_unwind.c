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
#include "parser/cypher_debug.h"

/* Test database handle */
static sqlite3 *test_db = NULL;
static cypher_executor *executor = NULL;

/* Setup function - create test database */
static int setup_executor_unwind_suite(void)
{
    /* Create in-memory database for testing */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    /* Initialize schema */
    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }

    cypher_schema_free_manager(schema_mgr);

    /* Create executor */
    executor = cypher_executor_create(test_db);
    if (!executor) {
        return -1;
    }

    return 0;
}

/* Teardown function */
static int teardown_executor_unwind_suite(void)
{
    if (executor) {
        cypher_executor_free(executor);
        executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Test basic UNWIND with integer list */
static void test_unwind_integer_list(void)
{
    const char *query = "UNWIND [1, 2, 3] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND integer list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 3);
        cypher_result_free(result);
    }
}

/* Test UNWIND with string list */
static void test_unwind_string_list(void)
{
    const char *query = "UNWIND [\"a\", \"b\", \"c\"] AS s RETURN s";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND string list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 3);
        cypher_result_free(result);
    }
}

/* Test UNWIND with empty list */
static void test_unwind_empty_list(void)
{
    const char *query = "UNWIND [] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND empty list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Empty list should return no rows */
        CU_ASSERT_EQUAL(result->row_count, 0);
        cypher_result_free(result);
    }
}

/* Test UNWIND with single element */
static void test_unwind_single_element(void)
{
    const char *query = "UNWIND [42] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND single element failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        cypher_result_free(result);
    }
}

/* Test UNWIND with mixed types */
static void test_unwind_mixed_types(void)
{
    const char *query = "UNWIND [1, \"two\", 3.0] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND mixed types failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 3);
        cypher_result_free(result);
    }
}

/* Test list literal parsing */
static void test_list_literal_parsing(void)
{
    /* Test that list literals are parsed correctly */
    cypher_parse_result *parse = parse_cypher_query_ext("RETURN [1, 2, 3]");
    CU_ASSERT_PTR_NOT_NULL(parse);

    if (parse) {
        CU_ASSERT_PTR_NULL(parse->error_message);
        CU_ASSERT_PTR_NOT_NULL(parse->ast);
        cypher_parse_result_free(parse);
    }
}

/* Test UNWIND parsing */
static void test_unwind_parsing(void)
{
    /* Test that UNWIND is parsed correctly */
    cypher_parse_result *parse = parse_cypher_query_ext("UNWIND [1, 2, 3] AS x RETURN x");
    CU_ASSERT_PTR_NOT_NULL(parse);

    if (parse) {
        if (parse->error_message) {
            printf("\nUNWIND parsing failed: %s\n", parse->error_message);
        }
        CU_ASSERT_PTR_NULL(parse->error_message);
        CU_ASSERT_PTR_NOT_NULL(parse->ast);
        cypher_parse_result_free(parse);
    }
}

/* Test empty list literal parsing */
static void test_empty_list_parsing(void)
{
    cypher_parse_result *parse = parse_cypher_query_ext("RETURN []");
    CU_ASSERT_PTR_NOT_NULL(parse);

    if (parse) {
        CU_ASSERT_PTR_NULL(parse->error_message);
        CU_ASSERT_PTR_NOT_NULL(parse->ast);
        cypher_parse_result_free(parse);
    }
}

/**
 * Regression test for GQLITE-T-0087: UNWIND with range() function
 * Previously returned: "UNWIND requires list literal, property access, or variable"
 * Now should correctly expand range(1, 5) to rows 1, 2, 3, 4, 5
 */
static void test_unwind_range_function_regression(void)
{
    const char *query = "UNWIND range(1, 5) AS n RETURN n";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND range() failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 5);

        /* Verify actual values returned */
        if (result->success && result->row_count == 5 && result->data) {
            bool found_1 = false, found_2 = false, found_3 = false, found_4 = false, found_5 = false;
            for (int i = 0; i < result->row_count; i++) {
                if (result->data[i] && result->data[i][0]) {
                    int val = atoi(result->data[i][0]);
                    if (val == 1) found_1 = true;
                    if (val == 2) found_2 = true;
                    if (val == 3) found_3 = true;
                    if (val == 4) found_4 = true;
                    if (val == 5) found_5 = true;
                }
            }
            CU_ASSERT_TRUE(found_1);
            CU_ASSERT_TRUE(found_2);
            CU_ASSERT_TRUE(found_3);
            CU_ASSERT_TRUE(found_4);
            CU_ASSERT_TRUE(found_5);
        }

        cypher_result_free(result);
    }
}

/**
 * Regression test for GQLITE-T-0087: UNWIND with range() function with step
 * Tests range(0, 10, 2) which should produce 0, 2, 4, 6, 8, 10
 */
static void test_unwind_range_with_step_regression(void)
{
    const char *query = "UNWIND range(0, 10, 2) AS n RETURN n";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND range() with step failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 6);  /* 0, 2, 4, 6, 8, 10 */

        /* Verify values are even numbers */
        if (result->success && result->data) {
            for (int i = 0; i < result->row_count; i++) {
                if (result->data[i] && result->data[i][0]) {
                    int val = atoi(result->data[i][0]);
                    CU_ASSERT_EQUAL(val % 2, 0);  /* All values should be even */
                }
            }
        }

        cypher_result_free(result);
    }
}

/* ============================================================
 * Issue #49: UNWIND $param write path regression tests
 * ============================================================ */

/**
 * Issue #49 Test a: UNWIND $param + CREATE + SET should create nodes
 * with properties from parameter objects.
 * BUG: Errors with "UNWIND+CREATE currently only supports list literals"
 */
static void test_unwind_param_create_set(void)
{
    const char *query =
        "UNWIND $items AS item CREATE (n:Uw49Node) SET n.id = item.id, n.name = item.name";
    const char *params = "{\"items\": [{\"id\": \"a\", \"name\": \"Alpha\"}, {\"id\": \"b\", \"name\": \"Beta\"}]}";

    cypher_result *result = cypher_executor_execute_params(executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIssue #49a UNWIND $param+CREATE+SET: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }

    /* Verify: should have 2 nodes with correct properties */
    cypher_result *verify = cypher_executor_execute(executor,
        "MATCH (n:Uw49Node) RETURN n.id ORDER BY n.id");
    CU_ASSERT_PTR_NOT_NULL(verify);
    if (verify) {
        CU_ASSERT_TRUE(verify->success);
        CU_ASSERT_EQUAL(verify->row_count, 2);
        if (verify->row_count == 2 && verify->data[0][0] && verify->data[1][0]) {
            CU_ASSERT_STRING_EQUAL(verify->data[0][0], "a");
            CU_ASSERT_STRING_EQUAL(verify->data[1][0], "b");
        }
        cypher_result_free(verify);
    }
}

/**
 * Issue #49 Test b: UNWIND $param + MERGE should iterate all items
 * and resolve item.id per-item.
 * BUG: Creates 1 node with NULL id instead of 2 nodes with correct ids
 */
static void test_unwind_param_merge(void)
{
    const char *query =
        "UNWIND $items AS item MERGE (n:Uw49Merge {id: item.id})";
    const char *params = "{\"items\": [{\"id\": \"x\"}, {\"id\": \"y\"}]}";

    cypher_result *result = cypher_executor_execute_params(executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIssue #49b UNWIND $param+MERGE: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }

    /* Verify: should have 2 nodes with ids x and y */
    cypher_result *verify = cypher_executor_execute(executor,
        "MATCH (n:Uw49Merge) RETURN n.id ORDER BY n.id");
    CU_ASSERT_PTR_NOT_NULL(verify);
    if (verify) {
        CU_ASSERT_TRUE(verify->success);
        CU_ASSERT_EQUAL(verify->row_count, 2);
        if (verify->row_count >= 1) {
            /* The id values must not be NULL — that's the bug */
            CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]);
        }
        if (verify->row_count == 2 && verify->data[0][0] && verify->data[1][0]) {
            CU_ASSERT_STRING_EQUAL(verify->data[0][0], "x");
            CU_ASSERT_STRING_EQUAL(verify->data[1][0], "y");
        }
        cypher_result_free(verify);
    }
}

/**
 * Issue #49 Test c: UNWIND literal + SET should propagate bound item value.
 * BUG: Creates 2 nodes but both have NULL id
 */
static void test_unwind_literal_set_binding(void)
{
    const char *query =
        "UNWIND [\"a\", \"b\"] AS item CREATE (n:Uw49Set) SET n.id = item";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIssue #49c UNWIND literal+SET: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }

    /* Verify: should have 2 nodes with ids "a" and "b" */
    cypher_result *verify = cypher_executor_execute(executor,
        "MATCH (n:Uw49Set) RETURN n.id ORDER BY n.id");
    CU_ASSERT_PTR_NOT_NULL(verify);
    if (verify) {
        CU_ASSERT_TRUE(verify->success);
        CU_ASSERT_EQUAL(verify->row_count, 2);
        if (verify->row_count == 2) {
            /* The values must not be NULL — that's the bug */
            CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]);
            CU_ASSERT_PTR_NOT_NULL(verify->data[1][0]);
            if (verify->data[0][0] && verify->data[1][0]) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "a");
                CU_ASSERT_STRING_EQUAL(verify->data[1][0], "b");
            }
        }
        cypher_result_free(verify);
    }
}

/* Initialize the UNWIND executor test suite */

/* ============================================================
 * #124: UNWIND values keep their type; row.key reads a map
 * ============================================================ */

/* Run `writes` (each with `params`, may be NULL) on a fresh graph, then
 * `query`; return column 0 of all rows joined with ','. Caller frees. */
static char *unwind_fresh(const char **writes, const char *params, const char *query)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return NULL;
    cypher_executor *ex = cypher_executor_create(db);
    char *out = NULL;
    if (ex) {
        for (int i = 0; writes[i]; i++) {
            cypher_result *r = params ? cypher_executor_execute_params(ex, writes[i], params)
                                      : cypher_executor_execute(ex, writes[i]);
            if (r && !r->success) printf("\n%s\n  error: %s\n", writes[i], r->error_message);
            if (r) cypher_result_free(r);
        }
        cypher_result *r = cypher_executor_execute(ex, query);
        if (r && r->success) {
            size_t cap = 256, len = 0;
            out = calloc(1, cap);
            for (int i = 0; out && i < r->row_count; i++) {
                const char *v = r->data[i][0] ? r->data[i][0] : "null";
                size_t need = len + strlen(v) + 2;
                if (need > cap) { cap = need * 2; out = realloc(out, cap); if (!out) break; }
                len += (size_t)snprintf(out + len, cap - len, "%s%s", i ? "," : "", v);
            }
        } else if (r) {
            printf("\n%s\n  error: %s\n", query, r->error_message);
        }
        if (r) cypher_result_free(r);
        cypher_executor_free(ex);
    }
    sqlite3_close(db);
    return out;
}

#define ASSERT_UNWIND(writes, params, query, expected) do {               \
        char *got_ = unwind_fresh(writes, params, query);                 \
        CU_ASSERT_PTR_NOT_NULL(got_);                                     \
        if (got_) {                                                       \
            if (strcmp(got_, expected) != 0)                              \
                printf("\n%s\n  expected %s, got %s\n", query, expected, got_); \
            CU_ASSERT_STRING_EQUAL(got_, expected);                       \
            free(got_);                                                   \
        }                                                                 \
    } while (0)

static const char *ROWS_PARAMS =
    "{\"rows\": [{\"id\": \"a\", \"value\": 1, \"f\": 1.5, \"b\": true},"
    " {\"id\": \"b\", \"value\": 2}]}";

static void test_unwind_param_map_create_inline(void)
{
    const char *w[] = {"UNWIND $rows AS row CREATE (:Item {id: row.id, value: row.value, f: row.f, b: row.b})", NULL};
    ASSERT_UNWIND(w, ROWS_PARAMS,
        "MATCH (n:Item) RETURN [n.id, n.value, n.f, n.b] ORDER BY n.id",
        "[\"a\",1,1.5,true],[\"b\",2,null,null]");
    ASSERT_UNWIND(w, ROWS_PARAMS,
        "MATCH (n:Item {id: 'a'}) RETURN n.value + 1", "2");
}

static void test_unwind_literal_map_create_inline(void)
{
    const char *w[] = {"UNWIND [{id: 'c', value: 3}, {id: 'd', value: 4}] AS row CREATE (:Lit {id: row.id, value: row.value})", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:Lit) RETURN [n.id, n.value] ORDER BY n.id", "[\"c\",3],[\"d\",4]");
}

static void test_unwind_scalar_types_kept(void)
{
    const char *w[] = {"UNWIND [1, 2.5, true, null, 'x'] AS v CREATE (:S {v: v})", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:S) RETURN count(n)", "5");
    ASSERT_UNWIND(w, NULL, "MATCH (n:S) WHERE n.v = 2.5 RETURN n.v", "2.5");
    ASSERT_UNWIND(w, NULL, "MATCH (n:S) WHERE n.v = true RETURN n.v", "true");
}

static void test_unwind_param_map_create_relationship(void)
{
    const char *w[] = {"UNWIND $rows AS row CREATE (:A {id: row.id})-[:R {w: row.value}]->(:B {id: row.id})", NULL};
    ASSERT_UNWIND(w, ROWS_PARAMS, "MATCH (:A)-[r:R]->(b:B) RETURN r.w ORDER BY b.id", "1,2");
}

static void test_unwind_twice_map_create_inline(void)
{
    /* Two UNWINDs take the row-wise pipeline. */
    const char *w[] = {"UNWIND $rows AS row UNWIND [1] AS k CREATE (:Item {id: row.id, value: row.value, b: row.b})", NULL};
    ASSERT_UNWIND(w, ROWS_PARAMS,
        "MATCH (n:Item) RETURN [n.id, n.value, n.b] ORDER BY n.id",
        "[\"a\",1,true],[\"b\",2,null]");
}

static void test_unwind_param_map_merge_typed(void)
{
    /* Run twice: the second MERGE must match the typed values. */
    const char *w[] = {
        "UNWIND $rows AS row MERGE (:M {id: row.id, value: row.value})",
        "UNWIND $rows AS row MERGE (:M {id: row.id, value: row.value})",
        NULL
    };
    ASSERT_UNWIND(w, ROWS_PARAMS, "MATCH (n:M) RETURN count(n)", "2");
    ASSERT_UNWIND(w, ROWS_PARAMS, "MATCH (n:M {id: 'a'}) RETURN n.value + 1", "2");
}

static void test_unwind_expression_create_inline(void)
{
    const char *w[] = {
        "UNWIND range(0, 2) AS i CREATE (:A {id: toString(i), next: i + 1})",
        "UNWIND $rows AS row CREATE (:C {v: row.value * 2, s: toUpper(row.id), f: row.f + 0.5})",
        NULL
    };
    ASSERT_UNWIND(w, ROWS_PARAMS, "MATCH (n:A) RETURN [n.id, n.next] ORDER BY n.id",
                  "[\"0\",1],[\"1\",2],[\"2\",3]");
    ASSERT_UNWIND(w, ROWS_PARAMS, "MATCH (n:C) RETURN [n.v, n.s, n.f] ORDER BY n.v",
                  "[2,\"A\",2.0],[4,\"B\",null]");

    /* Text that looks like JSON stays text */
    const char *w2[] = {"UNWIND ['[draft]', '{x}'] AS s CREATE (:T {s: s, u: toUpper(s)})", NULL};
    ASSERT_UNWIND(w2, NULL, "MATCH (n:T {s: '[draft]'}) RETURN n.u", "[DRAFT]");
    ASSERT_UNWIND(w2, NULL, "MATCH (n:T {s: '{x}'}) RETURN n.u", "{X}");
}

/* GQLITE-T-0387: boolean expressions are stored as booleans */
static void test_create_boolean_expression(void)
{
    const char *w[] = {"CREATE (:X {t: NOT false, u: 1 = 1, v: 2 > 3, w: 1 + 1})", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:X) RETURN [n.t, n.u, n.v, n.w]", "[true,true,false,2]");
}

/* GQLITE-T-0388: UNWIND before MATCH ... CREATE keeps the UNWIND variable */
static void test_unwind_match_create(void)
{
    const char *w[] = {
        "CREATE (:A {id: '0'})",
        "UNWIND ['x', 'y'] AS s MATCH (n:A {id: '0'}) CREATE (n)-[:R {tag: s + '!'}]->(:D {name: s})",
        NULL
    };
    ASSERT_UNWIND(w, NULL, "MATCH (:A)-[r:R]->(d:D) RETURN [r.tag, d.name] ORDER BY d.name",
                  "[\"x!\",\"x\"],[\"y!\",\"y\"]");
    const char *w2[] = {"CREATE (:A {id: '0'})", NULL};
    ASSERT_UNWIND(w2, NULL,
        "UNWIND [1, 2] AS i MATCH (n:A) CREATE (n)-[:S {i: i}]->(m:M {i: i}) RETURN m.i AS mi ORDER BY mi",
        "1,2");
}

static void test_foreach_map_create_inline(void)
{
    const char *w[] = {"FOREACH (r IN [{id: 'f1', n: 1}] | CREATE (:F {id: r.id, n: r.n}))", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:F) RETURN [n.id, n.n]", "[\"f1\",1]");
}

/* ============================================================
 * #134 / #135 / #136: rollback, failed writes, UNWIND and FOREACH
 * write pipelines, path functions, boolean comparisons
 * ============================================================ */

static void exec_ok(cypher_executor *ex, const char *q, const char *params)
{
    cypher_result *r = params ? cypher_executor_execute_params(ex, q, params)
                              : cypher_executor_execute(ex, q);
    CU_ASSERT_PTR_NOT_NULL(r);
    if (r && !r->success) printf("\n%s\n  error: %s\n", q, r->error_message);
    if (r) { CU_ASSERT_TRUE(r->success); cypher_result_free(r); }
}

static char *first_cell(cypher_executor *ex, const char *q)
{
    cypher_result *r = cypher_executor_execute(ex, q);
    char *out = NULL;
    if (r && r->success && r->row_count > 0 && r->data[0][0]) out = strdup(r->data[0][0]);
    if (r && !r->success) printf("\n%s\n  error: %s\n", q, r->error_message);
    if (r) cypher_result_free(r);
    return out;
}

/* #134: a property key made inside a rolled-back transaction must not stay
 * in the key cache; the next key gets the same id. */
static void test_rollback_property_keys(void)
{
    sqlite3 *db = NULL;
    CU_ASSERT_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
    cypher_executor *ex = cypher_executor_create(db);
    CU_ASSERT_PTR_NOT_NULL(ex);
    if (ex) {
        exec_ok(ex, "CREATE (:Item {id: 'safe'})", NULL);
        sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
        exec_ok(ex, "CREATE (:Item {id: 'discard', flag: true})", NULL);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        exec_ok(ex, "CREATE (:Item {id: 'b', other: 123})", NULL);
        exec_ok(ex, "CREATE (:Item {id: 'a', flag: false})", NULL);
        char *v = first_cell(ex, "MATCH (n:Item {id: 'a'}) RETURN [n.flag, n.other]");
        CU_ASSERT_PTR_NOT_NULL(v);
        if (v) { CU_ASSERT_STRING_EQUAL(v, "[false,null]"); free(v); }
        v = first_cell(ex, "MATCH (n:Item {id: 'b'}) RETURN [n.flag, n.other]");
        CU_ASSERT_PTR_NOT_NULL(v);
        if (v) { CU_ASSERT_STRING_EQUAL(v, "[null,123]"); free(v); }
        cypher_executor_free(ex);
    }
    sqlite3_close(db);
}

/* #134: a write query that fails leaves no writes, also inside an open
 * transaction, which stays open. */
static void test_failed_write_is_rolled_back(void)
{
    /* The DELETE fails (the node has a relationship) after the CREATE ran. */
    const char *bad = "CREATE (n:Item {id: 'a'})-[:R]->(:B) WITH n DELETE n";
    for (int in_txn = 0; in_txn < 2; in_txn++) {
        sqlite3 *db = NULL;
        CU_ASSERT_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
        cypher_executor *ex = cypher_executor_create(db);
        CU_ASSERT_PTR_NOT_NULL(ex);
        if (ex) {
            exec_ok(ex, "CREATE (:Keep)", NULL);
            if (in_txn) sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
            cypher_result *r = cypher_executor_execute(ex, bad);
            CU_ASSERT_PTR_NOT_NULL(r);
            if (r) { CU_ASSERT_FALSE(r->success); cypher_result_free(r); }
            if (in_txn) {
                CU_ASSERT_EQUAL(sqlite3_get_autocommit(db), 0);
                sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
            }
            char *v = first_cell(ex, "MATCH (n) RETURN count(n)");
            CU_ASSERT_PTR_NOT_NULL(v);
            if (v) { CU_ASSERT_STRING_EQUAL(v, "1"); free(v); }
            cypher_executor_free(ex);
        }
        sqlite3_close(db);
    }
}

static const char *ROWS_A7 = "{\"rows\": [{\"id\": \"a\", \"value\": 7}], \"value\": 7}";

/* #136: UNWIND ... MATCH ... SET changes only the matched nodes, and SET
 * reads UNWIND / WITH values. */
static void test_unwind_match_set(void)
{
    const char *w[] = {
        "CREATE (:Item {id: 'a', value: 1}), (:Item {id: 'b', value: 2})",
        "UNWIND $rows AS row MATCH (n:Item {id: row.id}) SET n.value = $value",
        NULL
    };
    ASSERT_UNWIND(w, ROWS_A7, "MATCH (n:Item) RETURN [n.id, n.value] ORDER BY n.id",
                  "[\"a\",7],[\"b\",2]");
    const char *w2[] = {
        "CREATE (:Item {id: 'a', value: 1}), (:Item {id: 'b', value: 2})",
        "UNWIND $rows AS row MATCH (n:Item {id: row.id}) SET n.value = row.value",
        NULL
    };
    ASSERT_UNWIND(w2, ROWS_A7, "MATCH (n:Item) RETURN [n.id, n.value] ORDER BY n.id",
                  "[\"a\",7],[\"b\",2]");
    const char *w3[] = {
        "CREATE (:Item {id: 'a', value: 10})",
        "MATCH (n:Item) UNWIND $rows AS row WITH n, row WHERE n.id = row.id SET n.value = row.value",
        NULL
    };
    ASSERT_UNWIND(w3, ROWS_A7, "MATCH (n:Item) RETURN n.value", "7");
    const char *w4[] = {
        "CREATE (:Item {id: 'a', value: 10})",
        "UNWIND $rows AS row MATCH (n:Item {id: row.id}) WITH n, row SET n.value = $value",
        NULL
    };
    ASSERT_UNWIND(w4, ROWS_A7, "MATCH (n:Item) RETURN n.value", "7");
    const char *w5[] = {"WITH $row AS row CREATE (n:Item {id: 'a'}) SET n.value = row.value", NULL};
    ASSERT_UNWIND(w5, "{\"row\": {\"value\": 1}}", "MATCH (n:Item) RETURN n.value", "1");
    const char *w6[] = {"WITH 42 AS x CREATE (n:Item {id: 'a'}) SET n.value = x", NULL};
    ASSERT_UNWIND(w6, NULL, "MATCH (n:Item) RETURN n.value", "42");
}

/* #136: SET on a relationship made by CREATE / MERGE reads the row. */
static void test_unwind_match_rel_set(void)
{
    const char *verbs[] = {"CREATE", "MERGE"};
    for (int v = 0; v < 2; v++) {
        char q[256];
        snprintf(q, sizeof(q),
                 "UNWIND $rows AS row MATCH (a:A), (b:B) %s (a)-[r:LINK]->(b) SET r.value = row.value",
                 verbs[v]);
        const char *w[] = {"CREATE (:A), (:B)", q, NULL};
        ASSERT_UNWIND(w, "{\"rows\": [{\"value\": 1}]}", "MATCH ()-[r:LINK]->() RETURN r.value", "1");
    }
}

/* #136: UNWIND ... MERGE ... SET without RETURN runs the SET; values keep
 * their types; null removes the property. */
static void test_unwind_merge_set_types(void)
{
    const char *w[] = {
        "CREATE (:Item {id: 'b', value: 5})",
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value",
        NULL
    };
    ASSERT_UNWIND(w,
        "{\"rows\": [{\"id\": \"a\", \"value\": [false, true]}, {\"id\": \"b\", \"value\": {\"k\": 1}},"
        " {\"id\": \"c\", \"value\": true}]}",
        "MATCH (n:Item) RETURN n.value ORDER BY n.id", "[false,true],{\"k\":1},true");
    const char *w2[] = {
        "CREATE (:Item {id: 'a', value: 10})",
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value",
        NULL
    };
    ASSERT_UNWIND(w2, "{\"rows\": [{\"id\": \"a\", \"value\": null}]}",
                  "MATCH (n:Item) RETURN n.value IS NULL", "true");
    const char *w3[] = {"CREATE (:Item {id: 'a', value: 10})", "MATCH (n:Item) SET n.value = $value", NULL};
    ASSERT_UNWIND(w3, "{\"value\": null}", "MATCH (n:Item) RETURN n.value IS NULL", "true");
    const char *w4[] = {"UNWIND $rows AS row CREATE (n:Item) SET n.id = row.id, n.value = row.value", NULL};
    ASSERT_UNWIND(w4, "{\"rows\": [{\"id\": \"a\", \"value\": [1, 2]}]}",
                  "MATCH (n:Item) RETURN n.value", "[1,2]");
    const char *w5[] = {"WITH $value AS value CREATE (:Item {id: 'a', value: value})", NULL};
    ASSERT_UNWIND(w5, "{\"value\": {\"k\": 1}}", "MATCH (n:Item) RETURN n.value.k", "1");
}

/* #136: UNWIND $param CREATE ... RETURN */
static void test_unwind_param_create_return(void)
{
    const char *w[] = {"CREATE (:Seed)", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:Seed) RETURN count(n)", "1");
    sqlite3 *db = NULL;
    CU_ASSERT_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
    cypher_executor *ex = cypher_executor_create(db);
    if (ex) {
        cypher_result *r = cypher_executor_execute_params(ex,
            "UNWIND $rows AS row CREATE (n:Item {id: row.id, value: row.value}) RETURN n.value AS v",
            "{\"rows\": [{\"id\": \"a\", \"value\": 1}, {\"id\": \"b\", \"value\": 2}]}");
        CU_ASSERT_PTR_NOT_NULL(r);
        if (r) {
            if (!r->success) printf("\n  error: %s\n", r->error_message);
            CU_ASSERT_TRUE(r->success);
            CU_ASSERT_EQUAL(r->row_count, 2);
            cypher_result_free(r);
        }
        cypher_executor_free(ex);
    }
    sqlite3_close(db);
}

/* #135: FOREACH runs MERGE / REMOVE / DELETE / SET bodies, sees an outer
 * MATCH variable, and iterates a $param list. */
static void test_foreach_bodies(void)
{
    const char *w[] = {"FOREACH (x IN [1, 2] | MERGE (:Item {value: x}))", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:Item) RETURN count(n)", "2");
    const char *w2[] = {"FOREACH (x IN $xs | CREATE (:Item {value: x}))", NULL};
    ASSERT_UNWIND(w2, "{\"xs\": [1, 2]}", "MATCH (n:Item) RETURN n.value ORDER BY n.value", "1,2");
    const char *w3[] = {"CREATE (:Item {id: 'a', value: 10})",
                        "MATCH (n:Item {id: 'a'}) FOREACH (x IN [1] | REMOVE n.value)", NULL};
    ASSERT_UNWIND(w3, NULL, "MATCH (n:Item) RETURN n.value IS NULL", "true");
    const char *w4[] = {"CREATE (:Item {id: 'a', value: 10})",
                        "MATCH (n:Item {id: 'a'}) FOREACH (x IN [1] | DELETE n)", NULL};
    ASSERT_UNWIND(w4, NULL, "MATCH (n) RETURN count(n)", "0");
    const char *w5[] = {"CREATE (:Item {id: 'a', value: 10})",
                        "MATCH (n:Item {id: 'a'}) FOREACH (x IN [1] | SET n.value = x)", NULL};
    ASSERT_UNWIND(w5, NULL, "MATCH (n:Item) RETURN n.value", "1");
    const char *w6[] = {"CREATE (:A)",
                        "MATCH (a:A) FOREACH (x IN ['p', 'q'] | CREATE (a)-[:R]->(:B {name: x}))", NULL};
    ASSERT_UNWIND(w6, NULL, "MATCH (:A)-[:R]->(b:B) RETURN b.name ORDER BY b.name", "p,q");
}

static const char *PATH_SETUP =
    "CREATE (a:Item {id: 'a', name: 'A'}), (b:Item {name: 'B'}), (c:Item {id: 'c', name: 'C'}),"
    " (a)-[:LINK {cost: 1.0}]->(b), (b)-[:LINK {cost: 2.0}]->(c)";

/* #135: length / nodes / relationships of variable-length and fixed paths. */
static void test_path_functions(void)
{
    const char *w[] = {PATH_SETUP, NULL};
    ASSERT_UNWIND(w, NULL,
        "MATCH p = (a:Item {id: 'a'})-[:LINK*1..2]->(c:Item {id: 'c'}) "
        "RETURN [length(p), size(nodes(p))]", "[2,3]");
    ASSERT_UNWIND(w, NULL,
        "MATCH p = (a:Item {id: 'a'})-[:LINK*1..2]->(c:Item {id: 'c'}) "
        "RETURN [n IN nodes(p) | n.name]", "[\"A\",\"B\",\"C\"]");
    ASSERT_UNWIND(w, NULL,
        "MATCH p = (a:Item {id: 'a'})-[:LINK*1..2]->(c:Item {id: 'c'}) "
        "RETURN [r IN relationships(p) | r.cost]", "[1.0,2.0]");
    ASSERT_UNWIND(w, NULL,
        "MATCH p = (a:Item {id: 'a'})-[:LINK]->(b)-[:LINK]->(c) "
        "RETURN [n IN nodes(p) | n.name] + [r IN relationships(p) | r.cost]",
        "[\"A\",\"B\",\"C\",1.0,2.0]");
    ASSERT_UNWIND(w, NULL,
        "MATCH p = shortestPath((a:Item {id: 'a'})-[:LINK*]->(c:Item {id: 'c'})) "
        "RETURN [length(p), size(nodes(p))]", "[2,3]");
    ASSERT_UNWIND(w, NULL,
        "MATCH (x:Item {id: 'c'}) OPTIONAL MATCH p = (x)-[:LINK]->(y) "
        "RETURN [nodes(p), relationships(p)]", "[null,null]");
}

/* #135: a boolean is not equal to a number. */
static void test_boolean_number_equality(void)
{
    const char *w[] = {"CREATE (:Seed)", NULL};
    ASSERT_UNWIND(w, NULL,
        "RETURN [true = 1, false = 0, true <> 1, true IN [1], false IN [0],"
        " true IN [true], 1 IN [1], 1 = 1.0]",
        "[false,false,true,false,false,true,true,true]");
    /* A stored boolean still finds true in a list. */
    const char *w2[] = {"CREATE (:Item {flag: true})", NULL};
    ASSERT_UNWIND(w2, NULL, "MATCH (n:Item) RETURN n.flag IN [true]", "true");
}

/* #136 review: a write that runs row by row keeps RETURN ORDER BY, SKIP,
 * LIMIT, DISTINCT and grouped aggregation, and projects expressions. */
static void test_rowwise_return_modifiers(void)
{
    const char *w[] = {"CREATE (:Item {id: 'a', v: 1}), (:Item {id: 'b', v: 2}), (:Item {id: 'c', v: 3})", NULL};
    ASSERT_UNWIND(w, NULL,
        "MATCH (n:Item) WITH n SET n.x = 1 RETURN n.id AS id ORDER BY id DESC LIMIT 2", "c,b");
    ASSERT_UNWIND(w, NULL,
        "MATCH (n:Item) WITH n SET n.x = 1 RETURN n.id AS id ORDER BY n.v DESC SKIP 1", "b,a");
    ASSERT_UNWIND(w, NULL, "MATCH (n:Item) WITH n SET n.x = 1 RETURN DISTINCT n.x", "1");
    ASSERT_UNWIND(w, NULL,
        "MATCH (n:Item) WITH n SET n.x = n.v % 2 RETURN n.x AS x, count(*) AS c ORDER BY c DESC", "1,0");
    ASSERT_UNWIND(w, NULL,
        "UNWIND ['a', 'b'] AS i MATCH (n:Item) WHERE n.id = i SET n.x = 1 "
        "RETURN toUpper(n.id) + n.v AS u ORDER BY u DESC LIMIT 1", "B2");
    ASSERT_UNWIND(w, NULL, "UNWIND [] AS i MATCH (n:Item) SET n.x = i RETURN count(*)", "0");
}

/* #135 review: a zero-hop segment adds its node once. */
static void test_path_zero_hop(void)
{
    const char *w[] = {"CREATE (:Item {id: 'a', name: 'A'})-[:LINK]->(:Item {name: 'B'})", NULL};
    ASSERT_UNWIND(w, NULL,
        "MATCH p = (a:Item {id: 'a'})-[:LINK*0..1]->(c) "
        "RETURN [[n IN nodes(p) | n.name], length(p)] AS r ORDER BY r[1]",
        "[[\"A\"],0],[[\"A\",\"B\"],1]");
}

/* tail() keeps maps, lists and entities (SQLite before 3.46 gave strings). */
static void test_tail_keeps_types(void)
{
    const char *w[] = {"CREATE (:A {name: 'a'}), (:A {name: 'b'})", NULL};
    ASSERT_UNWIND(w, NULL, "WITH [{k: 1}, {k: 2}, [3]] AS l RETURN tail(l)", "[{\"k\":2},[3]]");
    ASSERT_UNWIND(w, NULL,
        "MATCH (a:A) WITH a ORDER BY a.name WITH collect(a) AS ns RETURN [n IN tail(ns) | n.name]",
        "[\"b\"]");
    ASSERT_UNWIND(w, NULL, "RETURN [tail([]), tail([1]), tail(null)]", "[[],[],null]");
}

/* personalizedPageRank with 100 steps (one CTE per step hit SQLite 3.54's
 * nesting limit). */
static void test_personalized_pagerank_steps(void)
{
    const char *w[] = {"CREATE (a:N)-[:R]->(b:N)-[:R]->(a)", NULL};
    ASSERT_UNWIND(w, NULL,
        "RETURN size(personalizedPageRank('[1]', 0.85, 100))", "2");
    ASSERT_UNWIND(w, NULL,
        "UNWIND personalizedPageRank('[1]', 0.85, 100) AS r RETURN round(r.score * 1000) ORDER BY r.node_id",
        "541.0,459.0");
}

/* GQLITE-T-0421: an inline property with an expression value filters. */
static void test_inline_property_expression(void)
{
    const char *w[] = {
        "CREATE (:Item {id: 'a', v: 1, f: true}), (:Item {id: 'b', v: 2}), (:Item {id: 'c', v: 3})",
        "MATCH (a:Item {id: 'a'}), (b:Item {id: 'b'}), (c:Item {id: 'c'}) "
        "CREATE (a)-[:R {w: 1}]->(b), (a)-[:R {w: 2}]->(c)",
        NULL
    };
    ASSERT_UNWIND(w, NULL, "UNWIND ['a', 'b'] AS i MATCH (n:Item {id: i}) RETURN n.id ORDER BY n.id", "a,b");
    ASSERT_UNWIND(w, NULL, "WITH 1 AS x MATCH (n:Item {v: x + 1}) RETURN n.id", "b");
    ASSERT_UNWIND(w, NULL, "WITH 'A' AS s MATCH (n:Item {id: toLower(s)}) RETURN n.id", "a");
    ASSERT_UNWIND(w, NULL, "UNWIND [true] AS t MATCH (n:Item {f: t}) RETURN n.id", "a");
    ASSERT_UNWIND(w, NULL, "UNWIND [1] AS i MATCH (n:Item {id: i}) RETURN count(n)", "0");
    ASSERT_UNWIND(w, NULL, "UNWIND [2] AS k MATCH (:Item)-[r:R {w: k}]->(b) RETURN b.id", "c");
    const char *w2[] = {
        "CREATE (:Item {id: 'a', v: 1}), (:Item {id: 'b', v: 2})",
        "UNWIND ['a'] AS i MATCH (n:Item {id: i}) SET n.v = 9",
        NULL
    };
    ASSERT_UNWIND(w2, NULL, "MATCH (n:Item) RETURN n.v ORDER BY n.id", "9,2");
}

/* GQLITE-T-0422: a stored boolean is not equal to a number. */
static void test_stored_boolean_vs_number(void)
{
    const char *w[] = {"CREATE (:Item {flag: true, v: 1})", NULL};
    ASSERT_UNWIND(w, NULL,
        "MATCH (n:Item) RETURN [n.flag = 1, n.flag IN [1], n.v = true, 1 IN [true], n.v IN [true],"
        " n.flag = true, n.flag IN [true], n.v = 1, n.v IN [1], n.flag <> 1]",
        "[false,false,false,false,false,true,true,true,true,true]");
    ASSERT_UNWIND(w, NULL, "MATCH (n:Item) WHERE n.flag = 1 RETURN count(*)", "0");
}

/* GQLITE-T-0423: FOREACH over entities, and after a leading CREATE. */
static void test_foreach_entities(void)
{
    const char *setup = "CREATE (:Item {name: 'A'})-[:LINK]->(:Item {name: 'B'})-[:LINK]->(:Item {name: 'C'})";
    const char *w[] = {setup,
        "MATCH p = (:Item {name: 'A'})-[:LINK*]->(:Item {name: 'C'}) FOREACH (n IN nodes(p) | SET n.seen = true)",
        NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (n:Item) WHERE n.seen RETURN count(n)", "3");
    const char *w2[] = {setup,
        "MATCH p = (:Item {name: 'A'})-[:LINK*]->(:Item {name: 'C'}) FOREACH (r IN relationships(p) | SET r.w = 5)",
        NULL};
    ASSERT_UNWIND(w2, NULL, "MATCH ()-[r:LINK]->() RETURN sum(r.w)", "10");
    const char *w3[] = {setup, "MATCH (a:Item) WITH collect(a) AS ns FOREACH (n IN ns | SET n.k = 1)", NULL};
    ASSERT_UNWIND(w3, NULL, "MATCH (n:Item) RETURN sum(n.k)", "3");
    const char *w4[] = {setup, "MATCH (c:Item {name: 'C'}) FOREACH (n IN [c] | DETACH DELETE n)", NULL};
    ASSERT_UNWIND(w4, NULL, "MATCH (n:Item) RETURN count(n)", "2");
    const char *w5[] = {"CREATE (x:X) FOREACH (i IN [1, 2] | CREATE (x)-[:R]->(:Y {i: i}))", NULL};
    ASSERT_UNWIND(w5, NULL, "MATCH (:X)-[:R]->(y:Y) RETURN y.i ORDER BY y.i", "1,2");
}

/* GQLITE-T-0424: CREATE ... WITH ... UNWIND ... CREATE runs every CREATE. */
static void test_create_with_unwind_create(void)
{
    const char *w[] = {"CREATE (n:Item {id: 'a'}) WITH n UNWIND [1, 2] AS x CREATE (n)-[:R]->(:B {v: x})", NULL};
    ASSERT_UNWIND(w, NULL, "MATCH (:Item {id: 'a'})-[:R]->(b:B) RETURN b.v ORDER BY b.v", "1,2");
    const char *w2[] = {"UNWIND [1, 2] AS x CREATE (:A {v: x}) CREATE (:B {v: x})", NULL};
    ASSERT_UNWIND(w2, NULL, "MATCH (n) RETURN count(n)", "4");
}

int init_executor_unwind_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor UNWIND", setup_executor_unwind_suite, teardown_executor_unwind_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "List literal parsing", test_list_literal_parsing) ||
        !CU_add_test(suite, "Empty list parsing", test_empty_list_parsing) ||
        !CU_add_test(suite, "UNWIND parsing", test_unwind_parsing) ||
        !CU_add_test(suite, "UNWIND integer list", test_unwind_integer_list) ||
        !CU_add_test(suite, "UNWIND string list", test_unwind_string_list) ||
        !CU_add_test(suite, "UNWIND empty list", test_unwind_empty_list) ||
        !CU_add_test(suite, "UNWIND single element", test_unwind_single_element) ||
        !CU_add_test(suite, "UNWIND mixed types", test_unwind_mixed_types) ||
        !CU_add_test(suite, "UNWIND range() function regression", test_unwind_range_function_regression) ||
        !CU_add_test(suite, "UNWIND range() with step regression", test_unwind_range_with_step_regression) ||

        /* Issue #49: UNWIND $param write paths */
        !CU_add_test(suite, "Issue #49: UNWIND $param+CREATE+SET", test_unwind_param_create_set) ||
        !CU_add_test(suite, "Issue #49: UNWIND $param+MERGE", test_unwind_param_merge) ||
        !CU_add_test(suite, "Issue #49: UNWIND literal+SET binding", test_unwind_literal_set_binding) ||
        !CU_add_test(suite, "#124: UNWIND $param map CREATE inline", test_unwind_param_map_create_inline) ||
        !CU_add_test(suite, "#124: UNWIND literal map CREATE inline", test_unwind_literal_map_create_inline) ||
        !CU_add_test(suite, "#124: UNWIND scalar types kept", test_unwind_scalar_types_kept) ||
        !CU_add_test(suite, "#124: UNWIND $param map CREATE relationship", test_unwind_param_map_create_relationship) ||
        !CU_add_test(suite, "#124: two UNWINDs map CREATE inline", test_unwind_twice_map_create_inline) ||
        !CU_add_test(suite, "#124: UNWIND $param map MERGE typed", test_unwind_param_map_merge_typed) ||
        !CU_add_test(suite, "#124: FOREACH map CREATE inline", test_foreach_map_create_inline) ||
        !CU_add_test(suite, "#124: UNWIND expression CREATE inline", test_unwind_expression_create_inline) ||
        !CU_add_test(suite, "GQLITE-T-0387: CREATE boolean expression", test_create_boolean_expression) ||
        !CU_add_test(suite, "GQLITE-T-0388: UNWIND + MATCH + CREATE", test_unwind_match_create) ||
        !CU_add_test(suite, "#134: rollback keeps property keys right", test_rollback_property_keys) ||
        !CU_add_test(suite, "#134: failed write query is rolled back", test_failed_write_is_rolled_back) ||
        !CU_add_test(suite, "#136: UNWIND MATCH SET", test_unwind_match_set) ||
        !CU_add_test(suite, "#136: UNWIND MATCH relationship SET", test_unwind_match_rel_set) ||
        !CU_add_test(suite, "#136: UNWIND MERGE SET types and null", test_unwind_merge_set_types) ||
        !CU_add_test(suite, "#136: UNWIND $param CREATE RETURN", test_unwind_param_create_return) ||
        !CU_add_test(suite, "#135: FOREACH bodies", test_foreach_bodies) ||
        !CU_add_test(suite, "#135: path functions", test_path_functions) ||
        !CU_add_test(suite, "#135: boolean vs number equality", test_boolean_number_equality) ||
        !CU_add_test(suite, "#136: row-wise RETURN modifiers", test_rowwise_return_modifiers) ||
        !CU_add_test(suite, "#135: zero-hop path", test_path_zero_hop) ||
        !CU_add_test(suite, "tail() keeps types", test_tail_keeps_types) ||
        !CU_add_test(suite, "personalizedPageRank 100 steps", test_personalized_pagerank_steps) ||
        !CU_add_test(suite, "GQLITE-T-0421: inline property expression", test_inline_property_expression) ||
        !CU_add_test(suite, "GQLITE-T-0422: stored boolean vs number", test_stored_boolean_vs_number) ||
        !CU_add_test(suite, "GQLITE-T-0423: FOREACH over entities", test_foreach_entities) ||
        !CU_add_test(suite, "GQLITE-T-0424: CREATE WITH UNWIND CREATE", test_create_with_unwind_create)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
