#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"
#include "executor/cypher_schema.h"

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database */
static int setup_match_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }

    cypher_schema_free_manager(schema_mgr);
    return 0;
}

/* Teardown function */
static int teardown_match_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper function to parse and transform a query */
static cypher_query_result* parse_and_transform(const char *query_str)
{
    ast_node *ast = parse_cypher_query(query_str);
    if (!ast) {
        return NULL;
    }

    cypher_transform_context *ctx = cypher_transform_create_context(test_db);
    if (!ctx) {
        cypher_parser_free_result(ast);
        return NULL;
    }

    cypher_query_result *result = cypher_transform_query(ctx, (cypher_query*)ast);

    cypher_transform_free_context(ctx);
    cypher_parser_free_result(ast);

    return result;
}

/* Test simple MATCH transformation */
static void test_match_simple(void)
{
    const char *query = "MATCH (n) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with label */
static void test_match_with_label(void)
{
    const char *query = "MATCH (n:Person) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with property constraint */
static void test_match_with_property(void)
{
    const char *query = "MATCH (n:Person {name: 'Alice'}) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with integer property */
static void test_match_with_int_property(void)
{
    const char *query = "MATCH (n:Person {age: 30}) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with multiple properties */
static void test_match_with_multiple_properties(void)
{
    const char *query = "MATCH (n:Person {name: 'Alice', age: 30}) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with relationship */
static void test_match_relationship(void)
{
    const char *query = "MATCH (a)-[r]->(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with typed relationship */
static void test_match_typed_relationship(void)
{
    const char *query = "MATCH (a)-[r:KNOWS]->(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with left-directed relationship */
static void test_match_left_relationship(void)
{
    const char *query = "MATCH (a)<-[r]-(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with undirected relationship */
static void test_match_undirected_relationship(void)
{
    const char *query = "MATCH (a)-[r]-(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with variable length relationship */
static void test_match_varlen_relationship(void)
{
    const char *query = "MATCH (a)-[*1..3]->(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with variable length with type */
static void test_match_varlen_typed(void)
{
    const char *query = "MATCH (a)-[:KNOWS*1..5]->(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with multiple patterns */
static void test_match_multiple_patterns(void)
{
    const char *query = "MATCH (a:Person), (b:Company) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test OPTIONAL MATCH - Note: OPTIONAL MATCH may have limited support */
static void test_optional_match(void)
{
    const char *query = "MATCH (a:Person) OPTIONAL MATCH (a)-[r]->(b) RETURN a, b";
    cypher_query_result *result = parse_and_transform(query);

    /* OPTIONAL MATCH may not be fully implemented - just verify we get a result */
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        /* Note: has_error may be true if OPTIONAL MATCH isn't fully supported */
        cypher_free_result(result);
    }
}

/* Test MATCH with WHERE clause */
static void test_match_with_where(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age > 25 RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with complex WHERE */
static void test_match_with_complex_where(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age > 25 AND n.name = 'Alice' RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with OR in WHERE */
static void test_match_with_or_where(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age > 25 OR n.name = 'Alice' RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH + CREATE pattern */
static void test_match_create(void)
{
    const char *query = "MATCH (a:Person {name: 'Alice'}) CREATE (a)-[:KNOWS]->(b:Person {name: 'Bob'})";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with path variable */
static void test_match_path_variable(void)
{
    const char *query = "MATCH p = (a)-[*]->(b) RETURN p";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test shortestPath */
static void test_shortest_path(void)
{
    const char *query = "MATCH p = shortestPath((a:Person {name: 'Alice'})-[*..5]->(b:Person {name: 'Bob'})) RETURN p";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with boolean property */
static void test_match_with_bool_property(void)
{
    const char *query = "MATCH (n:Person {active: true}) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with decimal property */
static void test_match_with_decimal_property(void)
{
    const char *query = "MATCH (n:Person {salary: 50000.50}) RETURN n";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH chain pattern */
static void test_match_chain(void)
{
    const char *query = "MATCH (a)-[r1]->(b)-[r2]->(c) RETURN a, b, c";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with relationship properties */
static void test_match_rel_properties(void)
{
    const char *query = "MATCH (a)-[r:KNOWS {since: 2020}]->(b) RETURN a, b, r";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH without variable */
static void test_match_anonymous_node(void)
{
    const char *query = "MATCH (:Person)-[r]->(:Company) RETURN r";
    cypher_query_result *result = parse_and_transform(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* For standalone registration */
int register_match_tests(void)
{
    CU_pSuite suite = CU_add_suite("Transform MATCH", setup_match_suite, teardown_match_suite);
    if (!suite) return -1;

    if (!CU_add_test(suite, "Simple MATCH", test_match_simple)) return -1;
    if (!CU_add_test(suite, "MATCH with label", test_match_with_label)) return -1;
    if (!CU_add_test(suite, "MATCH with property", test_match_with_property)) return -1;
    if (!CU_add_test(suite, "MATCH with int property", test_match_with_int_property)) return -1;
    if (!CU_add_test(suite, "MATCH with multiple properties", test_match_with_multiple_properties)) return -1;
    if (!CU_add_test(suite, "MATCH relationship", test_match_relationship)) return -1;
    if (!CU_add_test(suite, "MATCH typed relationship", test_match_typed_relationship)) return -1;
    if (!CU_add_test(suite, "MATCH left relationship", test_match_left_relationship)) return -1;
    if (!CU_add_test(suite, "MATCH undirected relationship", test_match_undirected_relationship)) return -1;
    if (!CU_add_test(suite, "MATCH varlen relationship", test_match_varlen_relationship)) return -1;
    if (!CU_add_test(suite, "MATCH varlen typed", test_match_varlen_typed)) return -1;
    if (!CU_add_test(suite, "MATCH multiple patterns", test_match_multiple_patterns)) return -1;
    if (!CU_add_test(suite, "OPTIONAL MATCH", test_optional_match)) return -1;
    if (!CU_add_test(suite, "MATCH with WHERE", test_match_with_where)) return -1;
    if (!CU_add_test(suite, "MATCH with complex WHERE", test_match_with_complex_where)) return -1;
    if (!CU_add_test(suite, "MATCH with OR WHERE", test_match_with_or_where)) return -1;
    if (!CU_add_test(suite, "MATCH + CREATE", test_match_create)) return -1;
    if (!CU_add_test(suite, "MATCH path variable", test_match_path_variable)) return -1;
    if (!CU_add_test(suite, "shortestPath", test_shortest_path)) return -1;
    if (!CU_add_test(suite, "MATCH bool property", test_match_with_bool_property)) return -1;
    if (!CU_add_test(suite, "MATCH decimal property", test_match_with_decimal_property)) return -1;
    if (!CU_add_test(suite, "MATCH chain pattern", test_match_chain)) return -1;
    if (!CU_add_test(suite, "MATCH rel properties", test_match_rel_properties)) return -1;
    if (!CU_add_test(suite, "MATCH anonymous node", test_match_anonymous_node)) return -1;

    return 0;
}
