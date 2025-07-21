#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Final MATCH Query Test ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("❌ Failed to open database\n");
        return 1;
    }
    
    // Create test data
    printf("Creating test data...\n");
    const char *create1 = "CREATE (alice:Person {name: \"Alice\"})";
    const char *create2 = "CREATE (bob:Person {name: \"Bob\"})";
    
    gql_result_t *r1 = gql_execute_query(create1, db);
    gql_result_t *r2 = gql_execute_query(create2, db);
    
    if (r1 && r1->status == 0 && r1->nodes_created == 1) {
        printf("✅ Created Alice\n");
    } else {
        printf("❌ Failed to create Alice\n");
    }
    
    if (r2 && r2->status == 0 && r2->nodes_created == 1) {
        printf("✅ Created Bob\n");
    } else {
        printf("❌ Failed to create Bob\n");
    }
    
    if (r1) gql_result_destroy(r1);
    if (r2) gql_result_destroy(r2);
    
    // Test MATCH query
    printf("Testing MATCH query...\n");
    const char *match = "MATCH (n:Person) RETURN n";
    gql_result_t *match_result = gql_execute_query(match, db);
    
    if (match_result) {
        if (match_result->status == 0) {
            printf("✅ MATCH query executed successfully\n");
            printf("✅ Found %zu person(s)\n", match_result->row_count);
            
            if (match_result->row_count == 2) {
                printf("✅ Correct number of results\n");
            } else {
                printf("❌ Expected 2 results, got %zu\n", match_result->row_count);
            }
        } else {
            printf("❌ MATCH query failed: %s\n", 
                   match_result->error_message ? match_result->error_message : "Unknown error");
        }
        printf("About to destroy match result...\n");
        gql_result_destroy(match_result);
        printf("Match result destroyed\n");
    } else {
        printf("❌ MATCH query returned NULL\n");
    }
    
    printf("About to close database...\n");
    graphqlite_close(db);
    printf("Database closed\n");
    printf("=== Test Complete ===\n");
    return 0;
}