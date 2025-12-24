/*
 * Output Format Tests
 *
 * These tests verify the consistency of cypher() function output formats
 * for use by language bindings. The output contract is:
 *
 * 1. SELECT queries (MATCH...RETURN): JSON array of objects
 *    - Empty result: []
 *    - Single row: [{"col":"value"}]
 *    - Multiple rows: [{"col":"val1"},{"col":"val2"}]
 *
 * 2. Modification queries (CREATE, SET, DELETE, MERGE):
 *    - Success text message (not JSON)
 *
 * 3. Errors: SQLite error result (not a return value)
 *
 * 4. Graph algorithms: JSON array directly
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"

static sqlite3 *test_db = NULL;
static cypher_executor *test_executor = NULL;

static int setup_output_format_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) return -1;

    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) return -1;
    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }
    cypher_schema_free_manager(schema_mgr);

    test_executor = cypher_executor_create(test_db);
    if (!test_executor) return -1;

    return 0;
}

static int teardown_output_format_suite(void)
{
    if (test_executor) {
        cypher_executor_free(test_executor);
        test_executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper to execute and get result as string (simulating extension behavior) */
static char* execute_and_format(const char *query)
{
    cypher_result *result = cypher_executor_execute(test_executor, query);
    if (!result) return strdup("ERROR: null result");

    if (!result->success) {
        char *err = strdup(result->error_message ? result->error_message : "ERROR: unknown");
        cypher_result_free(result);
        return err;
    }

    /* Format like extension.c does - check agtype data FIRST */
    if (result->row_count > 0 && result->use_agtype && result->agtype_data) {
        /* Use AGE-compatible format with agtype values */
        int buffer_size = 1024;
        for (int row = 0; row < result->row_count; row++) {
            for (int col = 0; col < result->column_count; col++) {
                if (result->agtype_data[row][col]) {
                    char *temp_str = agtype_value_to_string(result->agtype_data[row][col]);
                    if (temp_str) {
                        buffer_size += strlen(temp_str) + 50;
                        free(temp_str);
                    }
                }
            }
        }

        char *json = malloc(buffer_size);
        strcpy(json, "[");

        for (int row = 0; row < result->row_count; row++) {
            if (row > 0) strcat(json, ",");
            strcat(json, "{");

            for (int col = 0; col < result->column_count; col++) {
                if (col > 0) strcat(json, ",");

                strcat(json, "\"");
                if (result->column_names && result->column_names[col]) {
                    strcat(json, result->column_names[col]);
                } else {
                    char col_name[32];
                    snprintf(col_name, sizeof(col_name), "column_%d", col);
                    strcat(json, col_name);
                }
                strcat(json, "\":");

                char *agtype_str = agtype_value_to_string(result->agtype_data[row][col]);
                if (agtype_str) {
                    strcat(json, agtype_str);
                    free(agtype_str);
                } else {
                    strcat(json, "null");
                }
            }
            strcat(json, "}");
        }
        strcat(json, "]");

        cypher_result_free(result);
        return json;
    }

    /* Fallback to raw data */
    if (result->row_count > 0 && result->data) {
        /* Check for single-cell JSON array (graph algorithms) */
        if (result->row_count == 1 && result->column_count == 1 &&
            result->data[0][0] && result->data[0][0][0] == '[') {
            char *out = strdup(result->data[0][0]);
            cypher_result_free(result);
            return out;
        }

        /* Build JSON array of objects */
        int buffer_size = 1024;
        for (int row = 0; row < result->row_count; row++) {
            for (int col = 0; col < result->column_count; col++) {
                if (result->data[row][col]) {
                    buffer_size += strlen(result->data[row][col]) * 2 + 50;
                }
            }
        }

        char *json = malloc(buffer_size);
        strcpy(json, "[");

        for (int row = 0; row < result->row_count; row++) {
            if (row > 0) strcat(json, ",");
            strcat(json, "{");

            for (int col = 0; col < result->column_count; col++) {
                if (col > 0) strcat(json, ",");

                strcat(json, "\"");
                if (result->column_names && result->column_names[col]) {
                    strcat(json, result->column_names[col]);
                } else {
                    char col_name[32];
                    snprintf(col_name, sizeof(col_name), "column_%d", col);
                    strcat(json, col_name);
                }
                strcat(json, "\":");

                if (result->data[row][col]) {
                    const char *val = result->data[row][col];
                    if (val[0] == '[' || val[0] == '{') {
                        strcat(json, val);
                    } else {
                        strcat(json, "\"");
                        char *p = json + strlen(json);
                        while (*val) {
                            if (*val == '"' || *val == '\\') *p++ = '\\';
                            *p++ = *val++;
                        }
                        *p = '\0';
                        strcat(json, "\"");
                    }
                } else {
                    strcat(json, "null");
                }
            }
            strcat(json, "}");
        }
        strcat(json, "]");

        cypher_result_free(result);
        return json;
    }

    /* Modification query - return stats message */
    char *msg = malloc(256);
    snprintf(msg, 256, "Query executed successfully - nodes created: %d, relationships created: %d",
             result->nodes_created, result->relationships_created);
    cypher_result_free(result);
    return msg;
}

/* Helper to check if string is valid JSON array */
static int is_json_array(const char *str)
{
    if (!str || str[0] != '[') return 0;
    return str[strlen(str)-1] == ']';
}

/* Helper to check if string starts with text (not JSON) */
static int is_text_message(const char *str)
{
    if (!str) return 0;
    return str[0] != '[' && str[0] != '{';
}

/*=== Tests for RETURN scalar values ===*/

static void test_return_integer(void)
{
    char *result = execute_and_format("RETURN 42 as num");
    printf("\nRETURN 42: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"num\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "42"));

    free(result);
}

static void test_return_string(void)
{
    char *result = execute_and_format("RETURN \"hello\" as msg");
    printf("\nRETURN string: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"msg\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "hello"));

    free(result);
}

static void test_return_float(void)
{
    char *result = execute_and_format("RETURN 3.14 as pi");
    printf("\nRETURN 3.14: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"pi\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "3.14"));

    free(result);
}

static void test_return_boolean_true(void)
{
    char *result = execute_and_format("RETURN true as flag");
    printf("\nRETURN true: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"flag\""));

    free(result);
}

static void test_return_boolean_false(void)
{
    char *result = execute_and_format("RETURN false as flag");
    printf("\nRETURN false: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"flag\""));

    free(result);
}

static void test_return_null(void)
{
    char *result = execute_and_format("RETURN null as val");
    printf("\nRETURN null: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"val\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "null"));

    free(result);
}

static void test_return_multiple_columns(void)
{
    char *result = execute_and_format("RETURN 1 as a, 2 as b, 3 as c");
    printf("\nRETURN multi-col: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"a\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"b\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"c\""));

    free(result);
}

/*=== Tests for CREATE operations ===*/

static void test_create_node_output(void)
{
    char *result = execute_and_format("CREATE (n:TestNode {name: \"test\"})");
    printf("\nCREATE node: %s\n", result);

    CU_ASSERT_TRUE(is_text_message(result));
    /* Check for success message format (not specific counts due to test isolation) */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "Query executed successfully"));

    free(result);
}

static void test_create_multiple_nodes_output(void)
{
    char *result = execute_and_format("CREATE (a:A), (b:B), (c:C)");
    printf("\nCREATE multi: %s\n", result);

    CU_ASSERT_TRUE(is_text_message(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "Query executed successfully"));

    free(result);
}

static void test_create_relationship_output(void)
{
    /* First create nodes */
    execute_and_format("CREATE (a:RelTest1 {id: 1})");
    execute_and_format("CREATE (b:RelTest2 {id: 2})");

    char *result = execute_and_format(
        "MATCH (a:RelTest1), (b:RelTest2) CREATE (a)-[:KNOWS]->(b)");
    printf("\nCREATE rel: %s\n", result);

    CU_ASSERT_TRUE(is_text_message(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "Query executed successfully"));

    free(result);
}

/*=== Tests for MATCH queries ===*/

static void test_match_empty_result(void)
{
    char *result = execute_and_format("MATCH (n:NonExistentLabel999) RETURN n");
    printf("\nMATCH empty: %s\n", result);

    /* Empty result should be [] or no rows */
    CU_ASSERT_TRUE(is_json_array(result) || is_text_message(result));

    free(result);
}

static void test_match_single_row(void)
{
    execute_and_format("CREATE (n:SingleRow {val: 123})");

    char *result = execute_and_format("MATCH (n:SingleRow) RETURN n.val as val");
    printf("\nMATCH single: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"val\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "123"));

    free(result);
}

static void test_match_multiple_rows(void)
{
    execute_and_format("CREATE (n:MultiRow {v: 1})");
    execute_and_format("CREATE (n:MultiRow {v: 2})");
    execute_and_format("CREATE (n:MultiRow {v: 3})");

    char *result = execute_and_format("MATCH (n:MultiRow) RETURN n.v ORDER BY n.v");
    printf("\nMATCH multi: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should have multiple objects in array */
    int count = 0;
    for (const char *p = result; *p; p++) {
        if (*p == '{') count++;
    }
    CU_ASSERT_TRUE(count >= 3);

    free(result);
}

static void test_match_with_properties(void)
{
    execute_and_format("CREATE (n:PropTest {name: \"Alice\", age: 30})");

    char *result = execute_and_format("MATCH (n:PropTest) RETURN n.name, n.age");
    printf("\nMATCH props: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "Alice"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "30"));

    free(result);
}

static void test_property_column_names(void)
{
    /* Test that property access expressions return proper column names
     * e.g., n.name should produce column name "n.name", not just "name" */
    execute_and_format("CREATE (p:ColNameTest {first: \"John\", last: \"Doe\"})");

    char *result = execute_and_format("MATCH (p:ColNameTest) RETURN p.first, p.last");
    printf("\nColumn names: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Verify column names include variable prefix (n.property format) */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"p.first\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"p.last\""));
    /* Ensure we don't have bare property names as column names */
    CU_ASSERT_PTR_NULL(strstr(result, "\"first\":"));
    CU_ASSERT_PTR_NULL(strstr(result, "\"last\":"));

    free(result);
}

static void test_explicit_alias_overrides_auto(void)
{
    /* Test that explicit aliases take precedence over auto-generated ones */
    execute_and_format("CREATE (x:AliasTest {val: 42})");

    char *result = execute_and_format("MATCH (x:AliasTest) RETURN x.val AS my_value");
    printf("\nExplicit alias: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should use explicit alias, not auto-generated x.val */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"my_value\""));
    CU_ASSERT_PTR_NULL(strstr(result, "\"x.val\""));

    free(result);
}

/*=== Tests for aggregation ===*/

static void test_count_aggregation(void)
{
    execute_and_format("CREATE (n:CountTest)");
    execute_and_format("CREATE (n:CountTest)");

    char *result = execute_and_format("MATCH (n:CountTest) RETURN count(n) as cnt");
    printf("\nCOUNT: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"cnt\""));

    free(result);
}

static void test_sum_aggregation(void)
{
    execute_and_format("CREATE (n:SumTest {v: 10})");
    execute_and_format("CREATE (n:SumTest {v: 20})");
    execute_and_format("CREATE (n:SumTest {v: 30})");

    char *result = execute_and_format("MATCH (n:SumTest) RETURN sum(n.v) as total");
    printf("\nSUM: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"total\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "60"));

    free(result);
}

/*=== Tests for special characters ===*/

static void test_string_with_quotes(void)
{
    execute_and_format("CREATE (n:QuoteTest {msg: \"He said \\\"hello\\\"\"})");

    char *result = execute_and_format("MATCH (n:QuoteTest) RETURN n.msg as msg");
    printf("\nQuoted string: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* The result should have properly escaped quotes */

    free(result);
}

static void test_string_with_newline(void)
{
    execute_and_format("CREATE (n:NewlineTest {msg: \"line1\\nline2\"})");

    char *result = execute_and_format("MATCH (n:NewlineTest) RETURN n.msg as msg");
    printf("\nNewline string: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));

    free(result);
}

/*=== Tests for graph algorithms output ===*/

static void test_pagerank_output_format(void)
{
    /* Create a small graph */
    execute_and_format("CREATE (a:PRNode {name: \"A\"})");
    execute_and_format("CREATE (b:PRNode {name: \"B\"})");
    execute_and_format("MATCH (a:PRNode {name: \"A\"}), (b:PRNode {name: \"B\"}) CREATE (a)-[:LINK]->(b)");

    char *result = execute_and_format("RETURN pageRank(0.85, 5)");
    printf("\nPageRank: %.100s...\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should contain node_id and score fields */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "node_id"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "score"));

    free(result);
}

static void test_label_propagation_output_format(void)
{
    /* Reuse nodes from pagerank test */
    char *result = execute_and_format("RETURN labelPropagation(5)");
    printf("\nLabelProp: %.100s...\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should contain node_id and community fields */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "node_id"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "community"));

    free(result);
}

/*=== Tests for returning whole nodes/relationships ===*/

static void test_return_whole_node(void)
{
    /* RETURN n should return node as object with id, labels, properties */
    execute_and_format("CREATE (n:WholeNode {name: \"Test\", value: 42})");

    char *result = execute_and_format("MATCH (n:WholeNode) RETURN n");
    printf("\nRETURN whole node: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Must have column name "n" */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"n\":"));
    /* Node must have id field */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"id\":"));
    /* Node must have labels array */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"labels\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"WholeNode\""));
    /* Node must have properties object */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"properties\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"name\""));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"Test\""));

    free(result);
}

static void test_return_whole_relationship(void)
{
    /* RETURN r should return relationship as object with id, type, properties */
    execute_and_format("CREATE (a:RelSource {id: \"src\"})-[:KNOWS {since: 2020}]->(b:RelTarget {id: \"tgt\"})");

    char *result = execute_and_format("MATCH ()-[r:KNOWS]->() RETURN r");
    printf("\nRETURN whole relationship: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Must have column name "r" */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"r\":"));
    /* Relationship must have id field */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"id\":"));
    /* Relationship must have type field */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"type\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"KNOWS\""));
    /* Relationship must have properties */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"properties\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"since\""));

    free(result);
}

static void test_return_node_and_properties(void)
{
    /* Test mixing whole node and property access in same query */
    execute_and_format("CREATE (n:MixedReturn {name: \"Mixed\", score: 100})");

    char *result = execute_and_format("MATCH (n:MixedReturn) RETURN n, n.name, n.score");
    printf("\nRETURN node + properties: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should have all three columns */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"n\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"n.name\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"n.score\":"));

    free(result);
}

static void test_return_multiple_nodes(void)
{
    /* Test returning multiple whole nodes */
    execute_and_format("CREATE (a:MultiNode {name: \"A\"})-[:LINK]->(b:MultiNode {name: \"B\"})");

    char *result = execute_and_format("MATCH (a:MultiNode)-[]->(b:MultiNode) RETURN a, b");
    printf("\nRETURN multiple nodes: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should have both node columns */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"a\":"));
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"b\":"));
    /* Both should be node objects with id/labels/properties */
    /* Count occurrences of "id": - should be at least 2 */
    int id_count = 0;
    const char *p = result;
    while ((p = strstr(p, "\"id\":")) != NULL) {
        id_count++;
        p++;
    }
    CU_ASSERT_TRUE(id_count >= 2);

    free(result);
}

static void test_return_path(void)
{
    /* Test returning a path */
    execute_and_format("CREATE (a:PathNode {name: \"Start\"})-[:STEP]->(b:PathNode {name: \"End\"})");

    char *result = execute_and_format("MATCH p=(a:PathNode)-[]->(b:PathNode) RETURN p");
    printf("\nRETURN path: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Path should be returned with column name "p" */
    CU_ASSERT_PTR_NOT_NULL(strstr(result, "\"p\":"));

    free(result);
}

static void test_node_not_double_encoded(void)
{
    /* Ensure node is not returned as escaped JSON string */
    execute_and_format("CREATE (n:NoDoubleEncode {val: 1})");

    char *result = execute_and_format("MATCH (n:NoDoubleEncode) RETURN n");
    printf("\nNo double encoding: %s\n", result);

    CU_ASSERT_TRUE(is_json_array(result));
    /* Should NOT have escaped quotes like \\\" which indicates double-encoding */
    CU_ASSERT_PTR_NULL(strstr(result, "\\\"id\\\""));
    CU_ASSERT_PTR_NULL(strstr(result, "\\\"labels\\\""));

    free(result);
}

/*=== Tests for error handling ===*/

static void test_syntax_error_format(void)
{
    char *result = execute_and_format("INVALID SYNTAX HERE");
    printf("\nSyntax error: %s\n", result);

    /* Errors should not be JSON arrays */
    CU_ASSERT_FALSE(is_json_array(result));

    free(result);
}

/* Initialize the output format test suite */
int init_output_format_suite(void)
{
    CU_pSuite suite = CU_add_suite("Output Format",
                                    setup_output_format_suite,
                                    teardown_output_format_suite);
    if (!suite) return CU_get_error();

    /* Scalar returns */
    if (!CU_add_test(suite, "RETURN integer", test_return_integer) ||
        !CU_add_test(suite, "RETURN string", test_return_string) ||
        !CU_add_test(suite, "RETURN float", test_return_float) ||
        !CU_add_test(suite, "RETURN true", test_return_boolean_true) ||
        !CU_add_test(suite, "RETURN false", test_return_boolean_false) ||
        !CU_add_test(suite, "RETURN null", test_return_null) ||
        !CU_add_test(suite, "RETURN multiple columns", test_return_multiple_columns)) {
        return CU_get_error();
    }

    /* CREATE operations */
    if (!CU_add_test(suite, "CREATE node output", test_create_node_output) ||
        !CU_add_test(suite, "CREATE multiple nodes", test_create_multiple_nodes_output) ||
        !CU_add_test(suite, "CREATE relationship output", test_create_relationship_output)) {
        return CU_get_error();
    }

    /* MATCH queries */
    if (!CU_add_test(suite, "MATCH empty result", test_match_empty_result) ||
        !CU_add_test(suite, "MATCH single row", test_match_single_row) ||
        !CU_add_test(suite, "MATCH multiple rows", test_match_multiple_rows) ||
        !CU_add_test(suite, "MATCH with properties", test_match_with_properties) ||
        !CU_add_test(suite, "Property column names", test_property_column_names) ||
        !CU_add_test(suite, "Explicit alias overrides auto", test_explicit_alias_overrides_auto)) {
        return CU_get_error();
    }

    /* Aggregation */
    if (!CU_add_test(suite, "COUNT aggregation", test_count_aggregation) ||
        !CU_add_test(suite, "SUM aggregation", test_sum_aggregation)) {
        return CU_get_error();
    }

    /* Special characters */
    if (!CU_add_test(suite, "String with quotes", test_string_with_quotes) ||
        !CU_add_test(suite, "String with newline", test_string_with_newline)) {
        return CU_get_error();
    }

    /* Graph algorithms */
    if (!CU_add_test(suite, "PageRank output format", test_pagerank_output_format) ||
        !CU_add_test(suite, "LabelPropagation output format", test_label_propagation_output_format)) {
        return CU_get_error();
    }

    /* Whole node/relationship returns */
    if (!CU_add_test(suite, "RETURN whole node", test_return_whole_node) ||
        !CU_add_test(suite, "RETURN whole relationship", test_return_whole_relationship) ||
        !CU_add_test(suite, "RETURN node and properties", test_return_node_and_properties) ||
        !CU_add_test(suite, "RETURN multiple nodes", test_return_multiple_nodes) ||
        !CU_add_test(suite, "RETURN path", test_return_path) ||
        !CU_add_test(suite, "Node not double encoded", test_node_not_double_encoded)) {
        return CU_get_error();
    }

    /* Errors */
    if (!CU_add_test(suite, "Syntax error format", test_syntax_error_format)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
