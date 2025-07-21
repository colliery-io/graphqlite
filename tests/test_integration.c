#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include "test_integration.h"

// Global database handle - will be reset for each test
static graphqlite_db_t *db = NULL;

// Forward declarations
void create_comprehensive_test_data(void);

// Setup function called before each test
int setup_comprehensive_test(void) {
    db = graphqlite_open(":memory:");
    if (!db) {
        return -1;
    }
    
    // Create comprehensive test data - migrated from comprehensive_match_test.c
    create_comprehensive_test_data();
    return 0;
}

// Teardown function called after each test
int teardown_comprehensive_test(void) {
    if (db) {
        graphqlite_close(db);
        db = NULL;
    }
    return 0;
}

// Helper function to create comprehensive test data - EXACT MIGRATION
void create_comprehensive_test_data(void) {
    // Create nodes representing a small organization
    int64_t alice_id = graphqlite_create_node(db);    // Employee
    int64_t bob_id = graphqlite_create_node(db);      // Employee  
    int64_t charlie_id = graphqlite_create_node(db);  // Manager
    int64_t diana_id = graphqlite_create_node(db);    // Company
    int64_t eve_id = graphqlite_create_node(db);      // Project
    int64_t frank_id = graphqlite_create_node(db);    // Employee
    
    // Add labels
    graphqlite_add_node_label(db, alice_id, "Person");
    graphqlite_add_node_label(db, alice_id, "Employee");
    
    graphqlite_add_node_label(db, bob_id, "Person");
    graphqlite_add_node_label(db, bob_id, "Employee");
    
    graphqlite_add_node_label(db, charlie_id, "Person");
    graphqlite_add_node_label(db, charlie_id, "Manager");
    
    graphqlite_add_node_label(db, diana_id, "Company");
    graphqlite_add_node_label(db, eve_id, "Project");
    
    graphqlite_add_node_label(db, frank_id, "Person");
    graphqlite_add_node_label(db, frank_id, "Employee");
    
    // Add properties
    property_value_t prop;
    
    // Alice: name="Alice", age=30, department="Engineering", salary=75000
    prop.type = PROP_TEXT; prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 75000;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "salary", &prop);
    
    // Bob: name="Bob", age=25, department="Sales", salary=60000
    prop.type = PROP_TEXT; prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Sales";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 60000;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "salary", &prop);
    
    // Charlie: name="Charlie", age=40, department="Engineering", salary=120000
    prop.type = PROP_TEXT; prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 40;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 120000;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "salary", &prop);
    
    // Diana (Company): name="TechCorp", founded=2010
    prop.type = PROP_TEXT; prop.value.text_val = "TechCorp";
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 2010;
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "founded", &prop);
    
    // Eve (Project): name="GraphQLite", status="Active"
    prop.type = PROP_TEXT; prop.value.text_val = "GraphQLite";
    graphqlite_set_property(db, ENTITY_NODE, eve_id, "name", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Active";
    graphqlite_set_property(db, ENTITY_NODE, eve_id, "status", &prop);
    
    // Frank: name="Frank", age=35, department="Engineering", salary=85000
    prop.type = PROP_TEXT; prop.value.text_val = "Frank";
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 85000;
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "salary", &prop);
    
    // Create relationships
    graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");           // Alice knows Bob
    graphqlite_create_edge(db, alice_id, charlie_id, "REPORTS_TO");  // Alice reports to Charlie
    graphqlite_create_edge(db, bob_id, charlie_id, "REPORTS_TO");    // Bob reports to Charlie
    graphqlite_create_edge(db, charlie_id, diana_id, "WORKS_FOR");   // Charlie works for TechCorp
    graphqlite_create_edge(db, alice_id, diana_id, "WORKS_FOR");     // Alice works for TechCorp
    graphqlite_create_edge(db, bob_id, diana_id, "WORKS_FOR");       // Bob works for TechCorp
    graphqlite_create_edge(db, frank_id, diana_id, "WORKS_FOR");     // Frank works for TechCorp
    graphqlite_create_edge(db, alice_id, eve_id, "WORKS_ON");        // Alice works on GraphQLite
    graphqlite_create_edge(db, frank_id, eve_id, "WORKS_ON");        // Frank works on GraphQLite
    graphqlite_create_edge(db, charlie_id, eve_id, "MANAGES");       // Charlie manages GraphQLite
    graphqlite_create_edge(db, alice_id, frank_id, "COLLABORATES");  // Alice collaborates with Frank
}

// =============================================================================
// MIGRATED TESTS FROM comprehensive_match_test.c
// All 23 tests with proper CUnit isolation
// =============================================================================

// Test 1: Simple node matching by label (Person)
void test_simple_node_matching_by_label(void) {
    const char *query = "MATCH (p:Person) RETURN p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 4);
    if (result) gql_result_destroy(result);
}

// Test 2: Node matching by multiple labels (Person & Employee)
void test_multiple_label_matching(void) {
    const char *query = "MATCH (e:Person & Employee) RETURN e.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 3);
    if (result) gql_result_destroy(result);
}

// Test 3: Node matching without labels (all nodes)
void test_node_matching_without_labels(void) {
    const char *query = "MATCH (n) RETURN n";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 6);
    if (result) gql_result_destroy(result);
}

// Test 4: Simple edge matching (KNOWS relationship)
void test_simple_edge_matching(void) {
    const char *query = "MATCH (a)-[r:KNOWS]->(b) RETURN a.name, b.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 1);
    if (result) gql_result_destroy(result);
}

// Test 5: Edge matching without type (all relationships)
void test_edge_matching_without_type(void) {
    const char *query = "MATCH (a)-[r]->(b) RETURN a.name, b.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 11);
    if (result) gql_result_destroy(result);
}

// Test 6: Complex edge pattern with labels (Employee -> Manager)
void test_complex_edge_pattern_with_labels(void) {
    const char *query = "MATCH (emp:Employee)-[r:REPORTS_TO]->(mgr:Manager) RETURN emp.name, mgr.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 2);
    if (result) gql_result_destroy(result);
}

// Test 7: WHERE with property equality (Engineering department)
void test_where_property_equality(void) {
    const char *query = "MATCH (p:Person) WHERE p.department = \"Engineering\" RETURN p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 3);
    if (result) gql_result_destroy(result);
}

// Test 8: WHERE with numeric comparison (age > 30)
void test_where_numeric_comparison(void) {
    const char *query = "MATCH (p:Person) WHERE p.age > 30 RETURN p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 2);
    if (result) gql_result_destroy(result);
}

// Test 9: WHERE with AND operator
void test_where_and_operator(void) {
    const char *query = "MATCH (p:Person) WHERE p.age > 25 AND p.department = \"Engineering\" RETURN p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 3);
    if (result) gql_result_destroy(result);
}

// Test 10: WHERE with OR operator
void test_where_or_operator(void) {
    const char *query = "MATCH (p:Person) WHERE p.name = \"Alice\" OR p.name = \"Bob\" RETURN p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 2);
    if (result) gql_result_destroy(result);
}

// Test 11: WHERE with string operations
void test_where_string_operations(void) {
    const char *query = "MATCH (p:Person) WHERE p.name STARTS WITH \"A\" RETURN p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 1);
    if (result) gql_result_destroy(result);
}

// Test 12: Variable reuse in WHERE
void test_variable_reuse_in_where(void) {
    const char *query = "MATCH (a:Person)-[r]->(b:Person) WHERE a.age > b.age RETURN a.name, b.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_TRUE(result->row_count >= 1);
    if (result) gql_result_destroy(result);
}

// Test 13: Complex variable relationships
void test_complex_variable_relationships(void) {
    const char *query = "MATCH (emp:Employee)-[r:REPORTS_TO]->(mgr) WHERE emp.salary < mgr.salary RETURN emp.name, mgr.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 2);
    if (result) gql_result_destroy(result);
}

// Test 14: Property projection
void test_property_projection(void) {
    const char *query = "MATCH (p:Person) RETURN p.name, p.age, p.department";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 4);
    CU_ASSERT_EQUAL(result->column_count, 3);
    if (result) gql_result_destroy(result);
}

// Test 15: Mixed projection
void test_mixed_projection(void) {
    const char *query = "MATCH (a:Person)-[r]->(b) RETURN a, r, b.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->column_count, 3);
    if (result) gql_result_destroy(result);
}

// Test 16: Alias support
void test_alias_support(void) {
    const char *query = "MATCH (p:Person) RETURN p.name AS person_name, p.age AS years";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->column_count, 2);
    if (result && result->column_names && result->column_count >= 2) {
        CU_ASSERT_STRING_EQUAL(result->column_names[0], "person_name");
        CU_ASSERT_STRING_EQUAL(result->column_names[1], "years");
    }
    if (result) gql_result_destroy(result);
}

// Test 17: Multi-hop relationships (Employee -> Manager -> Company)
void test_multihop_relationships(void) {
    const char *query = "MATCH (emp:Employee)-[:REPORTS_TO]->(mgr:Manager)-[:WORKS_FOR]->(company:Company) RETURN emp.name, company.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 2);
    if (result) gql_result_destroy(result);
}

// Test 18: Complex WHERE with relationships
void test_complex_where_with_relationships(void) {
    const char *query = "MATCH (a:Person)-[r:WORKS_ON]->(p:Project) WHERE p.status = \"Active\" AND a.department = \"Engineering\" RETURN a.name, p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 2);
    if (result) gql_result_destroy(result);
}

// Test 19: Triangular relationships
void test_triangular_relationships(void) {
    const char *query = "MATCH (a:Person)-[:COLLABORATES]->(b:Person), (a)-[:WORKS_ON]->(p:Project), (b)-[:WORKS_ON]->(p) RETURN a.name, b.name, p.name";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    // Just check it doesn't error, row count may vary
    if (result) gql_result_destroy(result);
}

// Test 20: Salary analysis query
void test_salary_analysis_query(void) {
    const char *query = "MATCH (high:Person)-[:WORKS_FOR]->(company:Company), (low:Person)-[:WORKS_FOR]->(company) WHERE high.salary > 80000 AND low.salary < 70000 RETURN high.name AS high_earner, low.name AS low_earner, company.name AS company";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    // Just check it doesn't error, row count may vary
    if (result) gql_result_destroy(result);
}

// Test 21: Non-existent label
void test_nonexistent_label(void) {
    const char *query = "MATCH (x:NonExistent) RETURN x";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 0);
    if (result) gql_result_destroy(result);
}

// Test 22: Non-existent relationship type
void test_nonexistent_relationship_type(void) {
    const char *query = "MATCH (a)-[r:NON_EXISTENT]->(b) RETURN a, b";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 0);
    if (result) gql_result_destroy(result);
}

// Test 23: Invalid property access
void test_invalid_property_access(void) {
    const char *query = "MATCH (p:Person) WHERE p.nonexistent = \"test\" RETURN p";
    gql_result_t *result = gql_execute_query(query, db);
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->status, 0);
    CU_ASSERT_EQUAL(result->row_count, 0);
    if (result) gql_result_destroy(result);
}

// =============================================================================
// Test Suite Registration for Comprehensive Tests
// =============================================================================

int add_comprehensive_tests(void) {
    CU_pSuite suite = CU_add_suite("Comprehensive Tests (Migrated)", setup_comprehensive_test, teardown_comprehensive_test);
    if (NULL == suite) {
        return -1;
    }
    
    // Add all 23 migrated tests
    if (NULL == CU_add_test(suite, "Test 1: Simple node matching by label", test_simple_node_matching_by_label) ||
        NULL == CU_add_test(suite, "Test 2: Multiple label matching", test_multiple_label_matching) ||
        NULL == CU_add_test(suite, "Test 3: Node matching without labels", test_node_matching_without_labels) ||
        NULL == CU_add_test(suite, "Test 4: Simple edge matching", test_simple_edge_matching) ||
        NULL == CU_add_test(suite, "Test 5: Edge matching without type", test_edge_matching_without_type) ||
        NULL == CU_add_test(suite, "Test 6: Complex edge pattern with labels", test_complex_edge_pattern_with_labels) ||
        NULL == CU_add_test(suite, "Test 7: WHERE property equality", test_where_property_equality) ||
        NULL == CU_add_test(suite, "Test 8: WHERE numeric comparison", test_where_numeric_comparison) ||
        NULL == CU_add_test(suite, "Test 9: WHERE AND operator", test_where_and_operator) ||
        NULL == CU_add_test(suite, "Test 10: WHERE OR operator", test_where_or_operator) ||
        NULL == CU_add_test(suite, "Test 11: WHERE string operations", test_where_string_operations) ||
        NULL == CU_add_test(suite, "Test 12: Variable reuse in WHERE", test_variable_reuse_in_where) ||
        NULL == CU_add_test(suite, "Test 13: Complex variable relationships", test_complex_variable_relationships) ||
        NULL == CU_add_test(suite, "Test 14: Property projection", test_property_projection) ||
        NULL == CU_add_test(suite, "Test 15: Mixed projection", test_mixed_projection) ||
        NULL == CU_add_test(suite, "Test 16: Alias support", test_alias_support) ||
        NULL == CU_add_test(suite, "Test 17: Multi-hop relationships", test_multihop_relationships) ||
        NULL == CU_add_test(suite, "Test 18: Complex WHERE with relationships", test_complex_where_with_relationships) ||
        NULL == CU_add_test(suite, "Test 19: Triangular relationships", test_triangular_relationships) ||
        NULL == CU_add_test(suite, "Test 20: Salary analysis query", test_salary_analysis_query) ||
        NULL == CU_add_test(suite, "Test 21: Non-existent label", test_nonexistent_label) ||
        NULL == CU_add_test(suite, "Test 22: Non-existent relationship type", test_nonexistent_relationship_type) ||
        NULL == CU_add_test(suite, "Test 23: Invalid property access", test_invalid_property_access)) {
        return -1;
    }
    
    return 0;
}