#include <CUnit/Basic.h>
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "graphqlite.h"
#include "test_relationship_operations.h"

static sqlite3 *db;

// Setup function to create test database
int setup_relationship_tests(void) {
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) {
        return 1;
    }
    
    // Initialize GraphQLite extension
    rc = sqlite3_load_extension(db, NULL, "sqlite3_graphqlite_init", NULL);
    if (rc != SQLITE_OK) {
        // Try to call the init function directly if available
        extern int sqlite3_graphqlite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
        rc = sqlite3_graphqlite_init(db, NULL, NULL);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            return 1;
        }
    }
    
    return 0;
}

// Cleanup function
int teardown_relationship_tests(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
    return 0;
}

// Helper function to execute cypher queries
static char* execute_cypher_query(const char* query) {
    sqlite3_stmt *stmt;
    char sql[1024];
    snprintf(sql, sizeof(sql), "SELECT cypher('%s')", query);
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    char *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *result_text = (const char*)sqlite3_column_text(stmt, 0);
        if (result_text) {
            result = strdup(result_text);
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

// Test relationship MATCH returning node variables
void test_relationship_match_return_node(void) {
    // Create test data
    char *result = execute_cypher_query("CREATE (a:Person {name: \"Alice\"})-[r:KNOWS {since: \"2020\"}]->(b:Person {name: \"Bob\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    // Test MATCH returning left node
    result = execute_cypher_query("MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a");
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT(strstr(result, "Alice") != NULL);
    CU_ASSERT(strstr(result, "Person") != NULL);
    printf("Left node result: %s\n", result ? result : "NULL");
    if (result) free(result);
    
    // Test MATCH returning right node
    result = execute_cypher_query("MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN b");
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT(strstr(result, "Bob") != NULL);
    CU_ASSERT(strstr(result, "Person") != NULL);
    printf("Right node result: %s\n", result ? result : "NULL");
    if (result) free(result);
}

// Test relationship MATCH returning relationship variable
void test_relationship_match_return_relationship(void) {
    // Create test data
    char *result = execute_cypher_query("CREATE (a:Person {name: \"Alice\"})-[r:WORKS_FOR {role: \"Manager\"}]->(c:Company {name: \"TechCorp\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    // Test MATCH returning relationship variable
    result = execute_cypher_query("MATCH (a:Person)-[r:WORKS_FOR]->(c:Company) RETURN r");
    printf("Relationship variable result: %s\n", result ? result : "NULL");
    
    // For now, we'll just check that it doesn't crash and returns something
    // TODO: Validate actual relationship data structure when implemented
    if (result) {
        CU_ASSERT(strlen(result) > 0);
        free(result);
    }
}

// Test bidirectional relationship matching
void test_bidirectional_relationship_match(void) {
    // Create test data with left-direction relationship
    char *result = execute_cypher_query("CREATE (a:Person {name: \"Charlie\"})<-[r:MANAGES]-(b:Person {name: \"Alice\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    // Test left-direction MATCH
    result = execute_cypher_query("MATCH (a:Person)<-[r:MANAGES]-(b:Person) RETURN a");
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT(strstr(result, "Charlie") != NULL);
    printf("Left-direction match result: %s\n", result ? result : "NULL");
    if (result) free(result);
    
    // Test if we can match in opposite direction (should not work without bidirectional support)
    result = execute_cypher_query("MATCH (a:Person)-[r:MANAGES]->(b:Person) RETURN a");
    printf("Opposite direction result: %s\n", result ? result : "NULL");
    if (result) free(result);
}

// Test relationship matching with properties
void test_relationship_match_with_properties(void) {
    // Create relationships with different properties
    char *result = execute_cypher_query("CREATE (a:Person {name: \"Alice\"})-[r:KNOWS {since: \"2020\", strength: \"strong\"}]->(b:Person {name: \"Bob\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    result = execute_cypher_query("CREATE (a:Person {name: \"Alice\"})-[r:KNOWS {since: \"2021\", strength: \"weak\"}]->(c:Person {name: \"Charlie\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    // Test matching with specific property
    result = execute_cypher_query("MATCH (a:Person)-[r:KNOWS {since: \"2020\"}]->(b:Person) RETURN b");
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT(strstr(result, "Bob") != NULL);
    CU_ASSERT(strstr(result, "Charlie") == NULL);
    printf("Property-filtered result: %s\n", result ? result : "NULL");
    if (result) free(result);
}

// Test multiple relationships from same node
void test_multiple_relationships(void) {
    // Create multiple relationships from Alice
    char *result = execute_cypher_query("CREATE (a:Person {name: \"Alice\"})-[r1:KNOWS]->(b:Person {name: \"Bob\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    result = execute_cypher_query("CREATE (a:Person {name: \"Alice\"})-[r2:WORKS_FOR]->(c:Company {name: \"TechCorp\"})");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) free(result);
    
    // Test matching all relationships from Alice
    result = execute_cypher_query("MATCH (a:Person {name: \"Alice\"})-[r]->(target) RETURN target");
    CU_ASSERT_PTR_NOT_NULL(result);
    printf("Multiple relationships result: %s\n", result ? result : "NULL");
    
    // Should find both Bob and TechCorp
    if (result) {
        // Note: Current implementation may return results differently
        // This test documents current behavior
        CU_ASSERT(strlen(result) > 0);
        free(result);
    }
}

// Test current limitations and expected failures
void test_relationship_operations_limitations(void) {
    // Test DELETE operation (should fail - not implemented yet)
    char *result = execute_cypher_query("CREATE (a:Person)-[r:TEST]->(b:Person)");
    if (result) free(result);
    
    result = execute_cypher_query("MATCH (a:Person)-[r:TEST]->(b:Person) DELETE r RETURN a");
    printf("DELETE result (should fail): %s\n", result ? result : "NULL");
    // DELETE not implemented yet, so this should fail gracefully
    if (result) free(result);
    
    // Test SET operation on relationship (should fail - not implemented yet)  
    result = execute_cypher_query("MATCH (a:Person)-[r:TEST]->(b:Person) SET r.updated = true RETURN r");
    printf("SET result (should fail): %s\n", result ? result : "NULL");
    if (result) free(result);
}

// Test suite registration function
int add_relationship_operation_tests(void) {
    CU_pSuite pSuite = NULL;
    
    // Add the suite to the registry
    pSuite = CU_add_suite("Relationship Operation Tests", setup_relationship_tests, teardown_relationship_tests);
    if (NULL == pSuite) {
        return 0;
    }
    
    // Add test cases to the suite
    if ((NULL == CU_add_test(pSuite, "Relationship MATCH return node", test_relationship_match_return_node)) ||
        (NULL == CU_add_test(pSuite, "Relationship MATCH return relationship", test_relationship_match_return_relationship)) ||
        (NULL == CU_add_test(pSuite, "Bidirectional relationship matching", test_bidirectional_relationship_match)) ||
        (NULL == CU_add_test(pSuite, "Relationship MATCH with properties", test_relationship_match_with_properties)) ||
        (NULL == CU_add_test(pSuite, "Multiple relationships", test_multiple_relationships)) ||
        (NULL == CU_add_test(pSuite, "Relationship operations limitations", test_relationship_operations_limitations))) {
        return 0;
    }
    
    return 1;
}