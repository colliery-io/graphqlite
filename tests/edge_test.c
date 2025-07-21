#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Edge Pattern Matching Test ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("❌ Failed to open database\n");
        return 1;
    }
    
    // Create test data: Alice KNOWS Bob
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
    
    // Create edge manually using the C API since CREATE edge syntax isn't implemented yet
    printf("Creating edge relationship...\n");
    int64_t alice_id = 1; // Assume first node
    int64_t bob_id = 2;   // Assume second node
    int64_t edge_id = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    
    if (edge_id > 0) {
        printf("✅ Created KNOWS edge (id=%lld)\n", (long long)edge_id);
    } else {
        printf("❌ Failed to create KNOWS edge\n");
        graphqlite_close(db);
        return 1;
    }
    
    // Test edge pattern matching: (a:Person)-[r:KNOWS]->(b:Person)
    printf("Testing edge pattern matching...\n");
    const char *match = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a, r, b";
    gql_result_t *match_result = gql_execute_query(match, db);
    
    if (match_result) {
        if (match_result->status == 0) {
            printf("✅ Edge pattern query executed successfully\n");
            printf("✅ Found %zu result(s)\n", match_result->row_count);
            
            if (match_result->row_count == 1) {
                printf("✅ Correct number of results\n");
                
                // Print the result
                // gql_result_print(match_result); // Temporarily disable to isolate crash
            } else {
                printf("❌ Expected 1 result, got %zu\n", match_result->row_count);
            }
        } else {
            printf("❌ Edge pattern query failed: %s\n", 
                   match_result->error_message ? match_result->error_message : "Unknown error");
        }
        gql_result_destroy(match_result);
    } else {
        printf("❌ Edge pattern query returned NULL\n");
    }
    
    // Test simple edge matching without type constraint
    printf("Testing edge pattern without type constraint...\n");
    const char *match2 = "MATCH (a:Person)-[r]->(b:Person) RETURN a, r, b";
    gql_result_t *match_result2 = gql_execute_query(match2, db);
    
    if (match_result2) {
        if (match_result2->status == 0) {
            printf("✅ Simple edge pattern query executed successfully\n");
            printf("✅ Found %zu result(s)\n", match_result2->row_count);
            
            if (match_result2->row_count == 1) {
                printf("✅ Correct number of results for simple pattern\n");
            } else {
                printf("❌ Expected 1 result for simple pattern, got %zu\n", match_result2->row_count);
            }
        } else {
            printf("❌ Simple edge pattern query failed: %s\n", 
                   match_result2->error_message ? match_result2->error_message : "Unknown error");
        }
        gql_result_destroy(match_result2);
    } else {
        printf("❌ Simple edge pattern query returned NULL\n");
    }
    
    graphqlite_close(db);
    printf("=== Edge Test Complete ===\n");
    return 0;
}