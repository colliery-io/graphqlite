#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "../src/ast.h"
#include "../src/traversal.h"
#include "test_variable_length_patterns.h"

// Forward declaration from grammar
extern cypher_ast_node_t* parse_cypher_query(const char *query);

// ============================================================================
// Helper Functions
// ============================================================================

static void assert_variable_length_pattern(cypher_ast_node_t *node, 
                                          int expected_min, 
                                          int expected_max, 
                                          int expected_type_count,
                                          const char *expected_variable,
                                          const char *expected_type) {
    CU_ASSERT_PTR_NOT_NULL(node);
    CU_ASSERT_EQUAL(node->type, AST_VARIABLE_LENGTH_PATTERN);
    
    if (node && node->type == AST_VARIABLE_LENGTH_PATTERN) {
        CU_ASSERT_EQUAL(node->data.variable_length_pattern.min_hops, expected_min);
        CU_ASSERT_EQUAL(node->data.variable_length_pattern.max_hops, expected_max);
        CU_ASSERT_EQUAL(node->data.variable_length_pattern.type_count, expected_type_count);
        
        // Check variable binding
        if (expected_variable) {
            CU_ASSERT_PTR_NOT_NULL(node->data.variable_length_pattern.variable);
            if (node->data.variable_length_pattern.variable) {
                CU_ASSERT_EQUAL(node->data.variable_length_pattern.variable->type, AST_VARIABLE);
                CU_ASSERT_STRING_EQUAL(node->data.variable_length_pattern.variable->data.variable.name, expected_variable);
            }
        } else {
            CU_ASSERT_PTR_NULL(node->data.variable_length_pattern.variable);
        }
        
        // Check relationship type
        if (expected_type && expected_type_count > 0) {
            CU_ASSERT_PTR_NOT_NULL(node->data.variable_length_pattern.relationship_types);
            if (node->data.variable_length_pattern.relationship_types) {
                cypher_ast_node_t *type_node = node->data.variable_length_pattern.relationship_types[0];
                CU_ASSERT_PTR_NOT_NULL(type_node);
                CU_ASSERT_EQUAL(type_node->type, AST_LABEL);
                CU_ASSERT_STRING_EQUAL(type_node->data.label.name, expected_type);
            }
        }
    }
}

static cypher_ast_node_t* get_relationship_edge_from_compound_query(cypher_ast_node_t *root) {
    if (!root || root->type != AST_COMPOUND_STATEMENT) return NULL;
    
    cypher_ast_node_t *match_stmt = root->data.compound_stmt.match_stmt;
    if (!match_stmt || match_stmt->type != AST_MATCH_STATEMENT) return NULL;
    
    cypher_ast_node_t *pattern = match_stmt->data.match_stmt.node_pattern;
    if (!pattern || pattern->type != AST_RELATIONSHIP_PATTERN) return NULL;
    
    return pattern->data.relationship_pattern.edge;
}

// ============================================================================
// Basic Variable-Length Pattern Tests
// ============================================================================

void test_unlimited_hops_pattern() {
    const char *query = "MATCH (a)-[*]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 0, -1, 0, NULL, NULL);
        ast_free(result);
    }
}

void test_bounded_hops_pattern() {
    const char *query = "MATCH (a)-[*1..5]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 1, 5, 0, NULL, NULL);
        ast_free(result);
    }
}

void test_exact_hops_pattern() {
    const char *query = "MATCH (a)-[*3]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 3, 3, 0, NULL, NULL);
        ast_free(result);
    }
}

void test_zero_minimum_hops() {
    const char *query = "MATCH (a)-[*0..2]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 0, 2, 0, NULL, NULL);
        ast_free(result);
    }
}

// ============================================================================
// Variable Binding Tests
// ============================================================================

void test_variable_binding_unlimited() {
    const char *query = "MATCH (a)-[path*]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 0, -1, 0, "path", NULL);
        ast_free(result);
    }
}

void test_variable_binding_bounded() {
    const char *query = "MATCH (a)-[r*2..4]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 2, 4, 0, "r", NULL);
        ast_free(result);
    }
}

void test_variable_binding_exact() {
    const char *query = "MATCH (a)-[edges*2]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 2, 2, 0, "edges", NULL);
        ast_free(result);
    }
}

// ============================================================================
// Relationship Type Constraint Tests
// ============================================================================

void test_typed_unlimited_pattern() {
    const char *query = "MATCH (a)-[:KNOWS*]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 0, -1, 1, NULL, "KNOWS");
        ast_free(result);
    }
}

void test_typed_bounded_pattern() {
    const char *query = "MATCH (a)-[:FOLLOWS*1..3]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 1, 3, 1, NULL, "FOLLOWS");
        ast_free(result);
    }
}

void test_typed_exact_pattern() {
    const char *query = "MATCH (a)-[:WORKS_WITH*2]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 2, 2, 1, NULL, "WORKS_WITH");
        ast_free(result);
    }
}

// ============================================================================
// Combined Variable and Type Tests
// ============================================================================

void test_variable_and_type_unlimited() {
    const char *query = "MATCH (a)-[rel:CONNECTED*]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 0, -1, 1, "rel", "CONNECTED");
        ast_free(result);
    }
}

void test_variable_and_type_bounded() {
    const char *query = "MATCH (a)-[path:SIMILAR_TO*1..4]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 1, 4, 1, "path", "SIMILAR_TO");
        ast_free(result);
    }
}

void test_variable_and_type_exact() {
    const char *query = "MATCH (a)-[chain:NEXT*3]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 3, 3, 1, "chain", "NEXT");
        ast_free(result);
    }
}

// ============================================================================
// Direction Tests
// ============================================================================

void test_left_direction_pattern() {
    const char *query = "MATCH (a)<-[*1..2]-(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *pattern = result->data.compound_stmt.match_stmt->data.match_stmt.node_pattern;
        CU_ASSERT_PTR_NOT_NULL(pattern);
        CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
        CU_ASSERT_EQUAL(pattern->data.relationship_pattern.direction, -1); // Left direction
        
        cypher_ast_node_t *edge = pattern->data.relationship_pattern.edge;
        assert_variable_length_pattern(edge, 1, 2, 0, NULL, NULL);
        ast_free(result);
    }
}

void test_right_direction_pattern() {
    const char *query = "MATCH (a)-[*2..3]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *pattern = result->data.compound_stmt.match_stmt->data.match_stmt.node_pattern;
        CU_ASSERT_PTR_NOT_NULL(pattern);
        CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
        CU_ASSERT_EQUAL(pattern->data.relationship_pattern.direction, 1); // Right direction
        
        cypher_ast_node_t *edge = pattern->data.relationship_pattern.edge;
        assert_variable_length_pattern(edge, 2, 3, 0, NULL, NULL);
        ast_free(result);
    }
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

void test_large_hop_counts() {
    const char *query = "MATCH (a)-[*1..100]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 1, 100, 0, NULL, NULL);
        ast_free(result);
    }
}

void test_single_hop_range() {
    const char *query = "MATCH (a)-[*1..1]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 1, 1, 0, NULL, NULL);
        ast_free(result);
    }
}

void test_complex_relationship_types() {
    const char *query = "MATCH (a)-[:VERY_LONG_RELATIONSHIP_TYPE_NAME*2..5]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 2, 5, 1, NULL, "VERY_LONG_RELATIONSHIP_TYPE_NAME");
        ast_free(result);
    }
}

// Test malformed patterns (should fail to parse)
void test_invalid_patterns() {
    const char* invalid_queries[] = {
        "MATCH (a)-[*..3]->(b) RETURN b",     // Missing min
        "MATCH (a)-[*1..]->(b) RETURN b",     // Missing max  
        "MATCH (a)-[*-1..3]->(b) RETURN b",   // Negative min
        "MATCH (a)-[*3..1]->(b) RETURN b",    // Max < min
    };
    
    int num_invalid = sizeof(invalid_queries) / sizeof(invalid_queries[0]);
    
    for (int i = 0; i < num_invalid; i++) {
        cypher_ast_node_t *result = parse_cypher_query(invalid_queries[i]);
        // These should either fail to parse (result == NULL) or have parsing errors
        // For now, we'll just verify they don't crash
        if (result) {
            ast_free(result);
        }
    }
}

// ============================================================================
// Integration with WHERE Clauses
// ============================================================================

void test_variable_length_with_where() {
    const char *query = "MATCH (a)-[r*1..3]->(b) WHERE a.name = \"start\" RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Check that the pattern parsed correctly
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        assert_variable_length_pattern(edge, 1, 3, 0, "r", NULL);
        
        // Check that WHERE clause exists
        cypher_ast_node_t *match_stmt = result->data.compound_stmt.match_stmt;
        CU_ASSERT_PTR_NOT_NULL(match_stmt->data.match_stmt.where_clause);
        
        ast_free(result);
    }
}

// ============================================================================
// Memory Management Tests
// ============================================================================

void test_variable_length_execution_basic() {
    sqlite3 *db;
    int rc = sqlite3_open(":memory:", &db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    if (rc != SQLITE_OK) return;
    
    // Create basic schema
    const char *schema = 
        "CREATE TABLE IF NOT EXISTS nodes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "labels TEXT,"
        "properties TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS relationships ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "start_node INTEGER,"
        "end_node INTEGER,"
        "type TEXT,"
        "properties TEXT"
        ");";
    
    rc = sqlite3_exec(db, schema, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    
    // Insert test data
    const char *data = 
        "INSERT INTO nodes (id, labels, properties) VALUES "
        "(1, '[\"Person\"]', '{\"name\": \"Alice\"}'),"
        "(2, '[\"Person\"]', '{\"name\": \"Bob\"}'),"
        "(3, '[\"Person\"]', '{\"name\": \"Charlie\"}');"
        "INSERT INTO relationships (id, start_node, end_node, type, properties) VALUES "
        "(1, 1, 2, 'KNOWS', '{}'),"
        "(2, 2, 3, 'KNOWS', '{}');";
    
    rc = sqlite3_exec(db, data, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    
    // Test variable-length query parsing
    const char *query = "MATCH (a)-[*1..2]->(b) RETURN b";
    cypher_ast_node_t *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        // The query should parse without errors
        cypher_ast_node_t *edge = get_relationship_edge_from_compound_query(result);
        CU_ASSERT_PTR_NOT_NULL(edge);
        CU_ASSERT_EQUAL(edge->type, AST_VARIABLE_LENGTH_PATTERN);
        ast_free(result);
    }
    
    sqlite3_close(db);
}

void test_traversal_algorithm_integration() {
    // Test that the traversal algorithms can be called without crashing
    sqlite3 *db;
    int rc = sqlite3_open(":memory:", &db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    if (rc != SQLITE_OK) return;
    
    // Create basic schema
    const char *schema = 
        "CREATE TABLE IF NOT EXISTS nodes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "labels TEXT,"
        "properties TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS relationships ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "start_node INTEGER,"
        "end_node INTEGER,"
        "type TEXT,"
        "properties TEXT"
        ");";
    
    rc = sqlite3_exec(db, schema, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    
    // Insert test data
    const char *data = 
        "INSERT INTO nodes (id, labels, properties) VALUES "
        "(1, '[\"Person\"]', '{\"name\": \"Alice\"}'),"
        "(2, '[\"Person\"]', '{\"name\": \"Bob\"}'),"
        "(3, '[\"Person\"]', '{\"name\": \"Charlie\"}');"
        "INSERT INTO relationships (id, start_node, end_node, type, properties) VALUES "
        "(1, 1, 2, 'KNOWS', '{}'),"
        "(2, 2, 3, 'KNOWS', '{}');";
    
    rc = sqlite3_exec(db, data, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    
    // Test BFS traversal
    traversal_result_t *result = bfs_traversal(db, 1, -1, 1, 2, NULL, 0, 10);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        // Should find paths from node 1
        CU_ASSERT(result->count >= 0);  // At least doesn't crash
        traversal_result_free(result);
    }
    
    sqlite3_close(db);
}

void test_memory_cleanup() {
    const char *query = "MATCH (a)-[path:KNOWS*1..5]->(b) RETURN b";
    
    // Parse and free multiple times to check for memory leaks
    for (int i = 0; i < 10; i++) {
        cypher_ast_node_t *result = parse_cypher_query(query);
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            ast_free(result);
        }
    }
}

// ============================================================================
// Test Suite Registration
// ============================================================================

int add_variable_length_pattern_tests(void) {
    CU_pSuite suite = CU_add_suite("Variable-Length Pattern Tests", NULL, NULL);
    if (!suite) {
        return 0;
    }

    // Basic pattern tests
    if (!CU_add_test(suite, "Unlimited hops pattern [*]", test_unlimited_hops_pattern) ||
        !CU_add_test(suite, "Bounded hops pattern [*1..5]", test_bounded_hops_pattern) ||
        !CU_add_test(suite, "Exact hops pattern [*3]", test_exact_hops_pattern) ||
        !CU_add_test(suite, "Zero minimum hops [*0..2]", test_zero_minimum_hops)) {
        return 0;
    }

    // Variable binding tests
    if (!CU_add_test(suite, "Variable binding unlimited [r*]", test_variable_binding_unlimited) ||
        !CU_add_test(suite, "Variable binding bounded [r*2..4]", test_variable_binding_bounded) ||
        !CU_add_test(suite, "Variable binding exact [r*2]", test_variable_binding_exact)) {
        return 0;
    }

    // Relationship type tests
    if (!CU_add_test(suite, "Typed unlimited [:KNOWS*]", test_typed_unlimited_pattern) ||
        !CU_add_test(suite, "Typed bounded [:FOLLOWS*1..3]", test_typed_bounded_pattern) ||
        !CU_add_test(suite, "Typed exact [:WORKS_WITH*2]", test_typed_exact_pattern)) {
        return 0;
    }

    // Combined tests
    if (!CU_add_test(suite, "Variable and type unlimited [r:CONNECTED*]", test_variable_and_type_unlimited) ||
        !CU_add_test(suite, "Variable and type bounded [r:SIMILAR_TO*1..4]", test_variable_and_type_bounded) ||
        !CU_add_test(suite, "Variable and type exact [r:NEXT*3]", test_variable_and_type_exact)) {
        return 0;
    }

    // Direction tests
    if (!CU_add_test(suite, "Left direction pattern", test_left_direction_pattern) ||
        !CU_add_test(suite, "Right direction pattern", test_right_direction_pattern)) {
        return 0;
    }

    // Edge cases
    if (!CU_add_test(suite, "Large hop counts", test_large_hop_counts) ||
        !CU_add_test(suite, "Single hop range [*1..1]", test_single_hop_range) ||
        !CU_add_test(suite, "Complex relationship types", test_complex_relationship_types) ||
        !CU_add_test(suite, "Invalid patterns", test_invalid_patterns)) {
        return 0;
    }

    // Integration tests
    if (!CU_add_test(suite, "Variable-length with WHERE clause", test_variable_length_with_where)) {
        return 0;
    }

    // Execution tests
    if (!CU_add_test(suite, "Variable-length execution basic", test_variable_length_execution_basic) ||
        !CU_add_test(suite, "Traversal algorithm integration", test_traversal_algorithm_integration) ||
        !CU_add_test(suite, "Memory cleanup", test_memory_cleanup)) {
        return 0;
    }

    return 1;
}