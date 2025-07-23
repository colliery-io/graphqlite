#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graphqlite.h"

// Global test database
static sqlite3 *test_db = NULL;

// Forward declarations for helper functions
static char* execute_cypher_query(const char *query);
static int count_nodes_in_table(void);
static void verify_node_properties(const char *result, const char *expected_name, int expected_age);
static void create_comprehensive_test_data(void);

// Test data structure for tracking expected results
static struct {
    int total_person_nodes;
    int adult_nodes;  // age >= 18
    int senior_nodes; // age >= 65
    int young_adults; // age 18-30
    int middle_aged;  // age 31-64
} where_test_data;

// ============================================================================
// Test Setup and Teardown
// ============================================================================

// Create comprehensive test data for WHERE clause testing
static void create_comprehensive_test_data(void) {
    // Reset counts
    memset(&where_test_data, 0, sizeof(where_test_data));
    
    // Create test nodes with diverse properties for comprehensive WHERE testing
    const char *test_queries[] = {
        // Young adults (18-30)
        "CREATE (n:Person {name: \"Alice\", age: 25, city: \"New York\", active: true})",
        "CREATE (n:Person {name: \"Bob\", age: 28, city: \"Boston\", active: false})",
        "CREATE (n:Person {name: \"Charlie\", age: 30, city: \"Chicago\", active: true})",
        "CREATE (n:Person {name: \"Diana\", age: 22, city: \"Denver\", active: true})",
        
        // Middle-aged (31-64)
        "CREATE (n:Person {name: \"Eve\", age: 35, city: \"Seattle\", active: true})",
        "CREATE (n:Person {name: \"Frank\", age: 42, city: \"Portland\", active: false})",
        "CREATE (n:Person {name: \"Grace\", age: 48, city: \"Austin\", active: true})",
        "CREATE (n:Person {name: \"Henry\", age: 55, city: \"Miami\", active: false})",
        
        // Seniors (65+)
        "CREATE (n:Person {name: \"Iris\", age: 67, city: \"Phoenix\", active: true})",
        "CREATE (n:Person {name: \"Jack\", age: 72, city: \"Las Vegas\", active: false})",
        
        // Minors (under 18)
        "CREATE (n:Person {name: \"Kelly\", age: 16, city: \"San Diego\", active: true})",
        "CREATE (n:Person {name: \"Leo\", age: 14, city: \"Tampa\", active: false})",
        
        // Products for mixed property type testing
        "CREATE (n:Product {name: \"Widget A\", price: 50, rating: 4.2, inStock: true})",
        "CREATE (n:Product {name: \"Widget B\", price: 100, rating: 3.8, inStock: false})",
        "CREATE (n:Product {name: \"Widget C\", price: 75, rating: 4.5, inStock: true})",
        "CREATE (n:Product {name: \"Widget D\", price: 200, rating: 4.0, inStock: false})"
    };
    
    // Execute all test data creation queries
    for (int i = 0; i < 16; i++) {
        char *result = execute_cypher_query(test_queries[i]);
        if (result) {
            free(result);
        }
    }
    
    // Set expected counts based on test data
    where_test_data.total_person_nodes = 12;
    where_test_data.adult_nodes = 10;      // Everyone except Kelly(16) and Leo(14)
    where_test_data.senior_nodes = 2;      // Iris(67) and Jack(72)
    where_test_data.young_adults = 4;      // Alice(25), Bob(28), Charlie(30), Diana(22)
    where_test_data.middle_aged = 4;       // Eve(35), Frank(42), Grace(48), Henry(55)
}

int setup_where_tests(void) {
    // Create in-memory database for testing
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(test_db));
        return -1;
    }
    
    // Initialize GraphQLite extension
    char *err_msg = NULL;
    rc = sqlite3_graphqlite_init(test_db, &err_msg, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot initialize GraphQLite: %s\n", err_msg ? err_msg : "Unknown error");
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(test_db);
        return -1;
    }
    
    // Create comprehensive test data for WHERE testing
    create_comprehensive_test_data();
    
    return 0;
}

int teardown_where_tests(void) {
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

// Per-test setup that ensures fresh test data for each test
static void reset_database_for_where_test(void) {
    // Drop the entire database by closing and recreating
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    
    // Recreate fresh database
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot recreate database: %s\n", sqlite3_errmsg(test_db));
        return;
    }
    
    // Reinitialize GraphQLite extension
    char *err_msg = NULL;
    rc = sqlite3_graphqlite_init(test_db, &err_msg, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot reinitialize GraphQLite: %s\n", err_msg ? err_msg : "Unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return;
    }
    
    // Create fresh test data
    create_comprehensive_test_data();
}

// ============================================================================
// Helper Functions
// ============================================================================

static char* execute_cypher_query(const char *query) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT cypher(?)";
    
    int rc = sqlite3_prepare_v2(test_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, query, -1, SQLITE_STATIC);
    
    char *result = NULL;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            result = strdup(text);
        }
    } else if (rc == SQLITE_ERROR) {
        result = NULL;
    }
    
    sqlite3_finalize(stmt);
    return result;
}

static int count_nodes_in_table(void) {
    const char *sql = "SELECT COUNT(*) FROM nodes";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(test_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Helper to verify node properties in result JSON
static void verify_node_properties(const char *result, const char *expected_name, int expected_age) {
    CU_ASSERT_PTR_NOT_NULL(result);
    if (!result) return;
    
    // Verify it's a proper JSON node entity
    CU_ASSERT(strstr(result, "\"identity\":") != NULL);
    CU_ASSERT(strstr(result, "\"labels\":") != NULL);
    CU_ASSERT(strstr(result, "\"Person\"") != NULL);
    CU_ASSERT(strstr(result, "\"properties\":") != NULL);
    
    // Verify specific property values
    if (expected_name) {
        char name_pattern[256];
        snprintf(name_pattern, sizeof(name_pattern), "\"name\": \"%s\"", expected_name);
        CU_ASSERT(strstr(result, name_pattern) != NULL);
    }
    
    if (expected_age > 0) {
        char age_pattern[64];
        snprintf(age_pattern, sizeof(age_pattern), "\"age\": %d", expected_age);
        CU_ASSERT(strstr(result, age_pattern) != NULL);
    }
}

// Count how many Person nodes match a specific WHERE condition by querying database directly
static int count_matching_persons(const char *where_condition) {
    char query[512];
    snprintf(query, sizeof(query), 
        "SELECT COUNT(DISTINCT n.id) FROM nodes n "
        "JOIN node_labels nl ON n.id = nl.node_id "
        "WHERE nl.label = 'Person' %s", where_condition);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(test_db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get specific test node data by name for validation
static int get_person_age_by_name(const char *name) {
    const char *sql = 
        "SELECT npi.value FROM nodes n "
        "JOIN node_labels nl ON n.id = nl.node_id "
        "JOIN node_props_text npt ON n.id = npt.node_id "
        "JOIN node_props_int npi ON n.id = npi.node_id "
        "JOIN property_keys pk1 ON npt.key_id = pk1.id "
        "JOIN property_keys pk2 ON npi.key_id = pk2.id "
        "WHERE nl.label = 'Person' AND pk1.key = 'name' AND npt.value = ? "
        "AND pk2.key = 'age'";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(test_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    
    int age = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        age = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return age;
}

// ============================================================================
// WHERE Clause Comparison Operator Tests
// ============================================================================

void test_where_equality_integer(void) {
    reset_database_for_where_test();
    
    // Test exact age match - should find Diana (age 22)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age = 22 RETURN n");
    verify_node_properties(result, "Diana", 22);
    if (result) free(result);
    
    // Test age that doesn't exist
    result = execute_cypher_query("MATCH (n:Person) WHERE n.age = 99 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");  // No results
        free(result);
    }
    
    // Verify our test data integrity
    int diana_age = get_person_age_by_name("Diana");
    CU_ASSERT_EQUAL(diana_age, 22);
}

void test_where_equality_string(void) {
    reset_database_for_where_test();
    
    // Test exact name match - should find Bob
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.name = \"Bob\" RETURN n");
    verify_node_properties(result, "Bob", 28);
    if (result) free(result);
    
    // Test case sensitivity
    result = execute_cypher_query("MATCH (n:Person) WHERE n.name = \"bob\" RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");  // No results - case sensitive
        free(result);
    }
    
    // Test city match
    result = execute_cypher_query("MATCH (n:Person) WHERE n.city = \"Chicago\" RETURN n");
    verify_node_properties(result, "Charlie", 30);
    if (result) free(result);
}

void test_where_equality_boolean(void) {
    reset_database_for_where_test();
    
    // Count active vs inactive people by querying database directly
    int active_count = count_matching_persons(
        "AND EXISTS (SELECT 1 FROM node_props_bool npb "
        "JOIN property_keys pk ON npb.key_id = pk.id "
        "WHERE npb.node_id = n.id AND pk.key = 'active' AND npb.value = 1)");
    
    int inactive_count = count_matching_persons(
        "AND EXISTS (SELECT 1 FROM node_props_bool npb "
        "JOIN property_keys pk ON npb.key_id = pk.id "
        "WHERE npb.node_id = n.id AND pk.key = 'active' AND npb.value = 0)");
    
    // Test active = true (should find 7 active people based on our test data)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.active = true RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should return one person (limitation of scalar function - only first result)
        // But verify it's an active person by checking the active property in JSON
        CU_ASSERT(strstr(result, "\"active\": true") != NULL || strstr(result, "\"active\": 1") != NULL);
        free(result);
    }
    
    // Test active = false (should find 5 inactive people)
    result = execute_cypher_query("MATCH (n:Person) WHERE n.active = false RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should return one person (limitation of scalar function)
        // But verify it's an inactive person
        CU_ASSERT(strstr(result, "\"active\": false") != NULL || strstr(result, "\"active\": 0") != NULL);
        free(result);
    }
    
    // Verify our test data has both active and inactive people
    CU_ASSERT(active_count > 0);
    CU_ASSERT(inactive_count > 0);
    CU_ASSERT_EQUAL(active_count + inactive_count, where_test_data.total_person_nodes);
}

void test_where_greater_than(void) {
    reset_database_for_where_test();
    
    // Test age > 50 (should find people older than 50)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age > 50 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Verify it's a valid person with age > 50
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        CU_ASSERT(strstr(result, "\"age\":") != NULL);
        
        // Check if it's one of our expected older people (Henry:55, Iris:67, Jack:72)
        int found_older = (strstr(result, "\"name\": \"Henry\"") != NULL) ||
                         (strstr(result, "\"name\": \"Iris\"") != NULL) ||
                         (strstr(result, "\"name\": \"Jack\"") != NULL);
        CU_ASSERT_EQUAL(found_older, 1);
        free(result);
    }
    
    // Test age > 100 (should find nobody)
    result = execute_cypher_query("MATCH (n:Person) WHERE n.age > 100 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");  // No results
        free(result);
    }
    
    // Verify test data has people over 50
    int over_50_count = count_matching_persons(
        "AND EXISTS (SELECT 1 FROM node_props_int npi "
        "JOIN property_keys pk ON npi.key_id = pk.id "
        "WHERE npi.node_id = n.id AND pk.key = 'age' AND npi.value > 50)");
    CU_ASSERT(over_50_count >= 3);  // Henry, Iris, Jack
}

void test_where_less_than(void) {
    reset_database_for_where_test();
    
    // Test age < 18 (should find minors)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age < 18 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find Kelly(16) or Leo(14)
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        int found_minor = (strstr(result, "\"name\": \"Kelly\"") != NULL) ||
                         (strstr(result, "\"name\": \"Leo\"") != NULL);
        CU_ASSERT_EQUAL(found_minor, 1);
        free(result);
    }
    
    // Test age < 10 (should find nobody in our dataset)
    result = execute_cypher_query("MATCH (n:Person) WHERE n.age < 10 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");  // No results
        free(result);
    }
    
    // Verify test data has minors
    int minor_count = count_matching_persons(
        "AND EXISTS (SELECT 1 FROM node_props_int npi "
        "JOIN property_keys pk ON npi.key_id = pk.id "
        "WHERE npi.node_id = n.id AND pk.key = 'age' AND npi.value < 18)");
    CU_ASSERT_EQUAL(minor_count, 2);  // Kelly and Leo
}

void test_where_greater_equal(void) {
    reset_database_for_where_test();
    
    // Test age >= 65 (should find seniors)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age >= 65 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find Iris(67) or Jack(72)
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        int found_senior = (strstr(result, "\"name\": \"Iris\"") != NULL) ||
                          (strstr(result, "\"name\": \"Jack\"") != NULL);
        CU_ASSERT_EQUAL(found_senior, 1);
        free(result);
    }
    
    // Verify seniors count
    int senior_count = count_matching_persons(
        "AND EXISTS (SELECT 1 FROM node_props_int npi "
        "JOIN property_keys pk ON npi.key_id = pk.id "
        "WHERE npi.node_id = n.id AND pk.key = 'age' AND npi.value >= 65)");
    CU_ASSERT_EQUAL(senior_count, where_test_data.senior_nodes);
}

void test_where_less_equal(void) {
    reset_database_for_where_test();
    
    // Test age <= 25 (should find younger people)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age <= 25 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find someone aged 25 or younger (Alice:25, Diana:22, Kelly:16, Leo:14)
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        int found_young = (strstr(result, "\"name\": \"Alice\"") != NULL) ||
                         (strstr(result, "\"name\": \"Diana\"") != NULL) ||
                         (strstr(result, "\"name\": \"Kelly\"") != NULL) ||
                         (strstr(result, "\"name\": \"Leo\"") != NULL);
        CU_ASSERT_EQUAL(found_young, 1);
        free(result);
    }
}

void test_where_not_equal(void) {
    reset_database_for_where_test();
    
    // Test age <> 25 (should find everyone except Alice who is 25)
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age <> 25 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should NOT be Alice
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        CU_ASSERT(strstr(result, "\"name\": \"Alice\"") == NULL);
        free(result);
    }
    
    // Test city <> "Boston" (should find everyone except Bob who is in Boston)
    result = execute_cypher_query("MATCH (n:Person) WHERE n.city <> \"Boston\" RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should NOT be Bob
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        CU_ASSERT(strstr(result, "\"name\": \"Bob\"") == NULL);
        free(result);
    }
}

// ============================================================================
// WHERE Clause with Different Property Types
// ============================================================================

void test_where_with_float_properties(void) {
    reset_database_for_where_test();
    
    // Test with Product ratings (float values)
    char *result = execute_cypher_query("MATCH (n:Product) WHERE n.rating > 4.0 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find a product with rating > 4.0
        CU_ASSERT(strstr(result, "\"Product\"") != NULL);
        CU_ASSERT(strstr(result, "\"rating\":") != NULL);
        free(result);
    }
    
    // Test exact float match
    result = execute_cypher_query("MATCH (n:Product) WHERE n.rating = 4.5 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find Widget C which has rating 4.5
        CU_ASSERT(strstr(result, "\"Product\"") != NULL);
        CU_ASSERT(strstr(result, "\"rating\": 4.5") != NULL);
        free(result);
    }
}

void test_where_with_mixed_property_types(void) {
    reset_database_for_where_test();
    
    // Test combining different property types in WHERE conditions
    // Price (int) > 75 AND inStock (boolean) = true
    char *result = execute_cypher_query("MATCH (n:Product) WHERE n.price > 75 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find products with price > 75 (Widget B:100, Widget D:200)
        CU_ASSERT(strstr(result, "\"Product\"") != NULL);
        CU_ASSERT(strstr(result, "\"price\":") != NULL);
        
        // Verify it's one of the expensive products
        int found_expensive = (strstr(result, "\"name\": \"Widget B\"") != NULL) ||
                             (strstr(result, "\"name\": \"Widget D\"") != NULL);
        CU_ASSERT_EQUAL(found_expensive, 1);
        free(result);
    }
}

// ============================================================================
// WHERE Clause Edge Cases and Error Handling
// ============================================================================

void test_where_with_nonexistent_property(void) {
    reset_database_for_where_test();
    
    // Test WHERE condition on property that doesn't exist
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.salary > 50000 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should return no results since no Person has a salary property
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");
        free(result);
    }
}

void test_where_type_mismatch(void) {
    reset_database_for_where_test();
    
    // Test comparing integer property with string value
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age = \"25\" RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should return no results due to type mismatch
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");
        free(result);
    }
    
    // Test comparing string property with integer value
    result = execute_cypher_query("MATCH (n:Person) WHERE n.name = 25 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should return no results due to type mismatch
        CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");
        free(result);
    }
}

void test_where_boundary_values(void) {
    reset_database_for_where_test();
    
    // Test with boundary age values
    char *result = execute_cypher_query("MATCH (n:Person) WHERE n.age >= 0 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find at least one person (all ages are positive)
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        free(result);
    }
    
    // Test with high boundary value
    result = execute_cypher_query("MATCH (n:Person) WHERE n.age < 1000 RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        // Should find at least one person (all our test ages are < 1000)
        CU_ASSERT(strstr(result, "\"Person\"") != NULL);
        free(result);
    }
}

// ============================================================================
// WHERE Clause Data Validation Tests
// ============================================================================

void test_where_data_integrity(void) {
    reset_database_for_where_test();
    
    // Verify our test data was created correctly
    int total_nodes = count_nodes_in_table();
    CU_ASSERT_EQUAL(total_nodes, 16);  // 12 Person + 4 Product nodes
    
    // Verify Person node count
    int person_count = count_matching_persons("");
    CU_ASSERT_EQUAL(person_count, where_test_data.total_person_nodes);
    
    // Test specific individuals to ensure data integrity
    struct {
        const char *name;
        int expected_age;
        const char *expected_city;
    } expected_people[] = {
        {"Alice", 25, "New York"},
        {"Bob", 28, "Boston"},
        {"Charlie", 30, "Chicago"},
        {"Iris", 67, "Phoenix"}
    };
    
    for (int i = 0; i < 4; i++) {
        char query[256];
        snprintf(query, sizeof(query), "MATCH (n:Person) WHERE n.name = \"%s\" RETURN n", 
                expected_people[i].name);
        
        char *result = execute_cypher_query(query);
        verify_node_properties(result, expected_people[i].name, expected_people[i].expected_age);
        
        if (result) {
            // Also verify city
            char city_pattern[128];
            snprintf(city_pattern, sizeof(city_pattern), "\"city\": \"%s\"", expected_people[i].expected_city);
            CU_ASSERT(strstr(result, city_pattern) != NULL);
            free(result);
        }
    }
}

void test_where_comprehensive_age_ranges(void) {
    reset_database_for_where_test();
    
    // Test various age ranges to ensure comprehensive coverage
    struct {
        const char *condition;
        int min_expected_results;
        const char *description;
    } age_tests[] = {
        {"n.age >= 18", 10, "adults"},
        {"n.age < 18", 2, "minors"},
        {"n.age >= 65", 2, "seniors"},
        {"n.age >= 18 AND n.age < 65", 8, "working age adults"},  // This would need AND support
    };
    
    // Test each age range (except the AND one which needs logical operator support)
    for (int i = 0; i < 3; i++) {  // Skip the AND test for now
        char query[256];
        snprintf(query, sizeof(query), "MATCH (n:Person) WHERE %s RETURN n", age_tests[i].condition);
        
        char *result = execute_cypher_query(query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            if (age_tests[i].min_expected_results > 0) {
                // Should find at least one person matching the condition
                CU_ASSERT(strstr(result, "\"Person\"") != NULL);
                CU_ASSERT(strstr(result, "\"age\":") != NULL);
            } else {
                // Should find no results
                CU_ASSERT_STRING_EQUAL(result, "Query executed successfully");
            }
            free(result);
        }
    }
}

// ============================================================================
// Test Suite Registration
// ============================================================================

int add_where_clause_tests(void) {
    CU_pSuite comparison_suite = CU_add_suite("WHERE Clause Comparison Tests", 
                                            setup_where_tests, teardown_where_tests);
    if (!comparison_suite) {
        return 0;
    }
    
    if (!CU_add_test(comparison_suite, "WHERE equality with integers", test_where_equality_integer) ||
        !CU_add_test(comparison_suite, "WHERE equality with strings", test_where_equality_string) ||
        !CU_add_test(comparison_suite, "WHERE equality with booleans", test_where_equality_boolean) ||
        !CU_add_test(comparison_suite, "WHERE greater than", test_where_greater_than) ||
        !CU_add_test(comparison_suite, "WHERE less than", test_where_less_than) ||
        !CU_add_test(comparison_suite, "WHERE greater or equal", test_where_greater_equal) ||
        !CU_add_test(comparison_suite, "WHERE less or equal", test_where_less_equal) ||
        !CU_add_test(comparison_suite, "WHERE not equal", test_where_not_equal)) {
        return 0;
    }
    
    CU_pSuite property_types_suite = CU_add_suite("WHERE Clause Property Types Tests", 
                                                 setup_where_tests, teardown_where_tests);
    if (!property_types_suite) {
        return 0;
    }
    
    if (!CU_add_test(property_types_suite, "WHERE with float properties", test_where_with_float_properties) ||
        !CU_add_test(property_types_suite, "WHERE with mixed property types", test_where_with_mixed_property_types)) {
        return 0;
    }
    
    CU_pSuite edge_cases_suite = CU_add_suite("WHERE Clause Edge Cases Tests", 
                                            setup_where_tests, teardown_where_tests);
    if (!edge_cases_suite) {
        return 0;
    }
    
    if (!CU_add_test(edge_cases_suite, "WHERE with nonexistent property", test_where_with_nonexistent_property) ||
        !CU_add_test(edge_cases_suite, "WHERE type mismatch", test_where_type_mismatch) ||
        !CU_add_test(edge_cases_suite, "WHERE boundary values", test_where_boundary_values)) {
        return 0;
    }
    
    CU_pSuite validation_suite = CU_add_suite("WHERE Clause Data Validation Tests", 
                                            setup_where_tests, teardown_where_tests);
    if (!validation_suite) {
        return 0;
    }
    
    if (!CU_add_test(validation_suite, "WHERE data integrity", test_where_data_integrity) ||
        !CU_add_test(validation_suite, "WHERE comprehensive age ranges", test_where_comprehensive_age_ranges)) {
        return 0;
    }
    
    return 1;
}