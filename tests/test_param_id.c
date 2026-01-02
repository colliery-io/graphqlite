/**
 * Test parameterized queries returning correct node IDs
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

int main() {
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;

    // Open in-memory database
    rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Load graphqlite extension
    sqlite3_enable_load_extension(db, 1);
    rc = sqlite3_load_extension(db, "./build/graphqlite", NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot load extension: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    printf("Test: Parameterized queries should return unique node IDs\n\n");

    // Create three nodes with different names using parameters
    const char *names[] = {"Alice", "Bob", "Charlie"};
    int node_ids[3] = {0, 0, 0};

    for (int i = 0; i < 3; i++) {
        // Create node with parameterized query
        char query[512];
        snprintf(query, sizeof(query),
            "SELECT cypher('CREATE (a:Person {name: $name})', '{\"name\": \"%s\"}')",
            names[i]);
        
        rc = sqlite3_exec(db, query, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Create failed: %s\n", err_msg);
            sqlite3_free(err_msg);
            return 1;
        }
        printf("Created node with name: %s\n", names[i]);
    }

    printf("\nQuerying nodes back:\n");

    // Query each node back
    for (int i = 0; i < 3; i++) {
        char query[512];
        snprintf(query, sizeof(query),
            "SELECT cypher('MATCH (a:Person {name: $name}) RETURN id(a) AS node_id, a.name AS name', '{\"name\": \"%s\"}')",
            names[i]);

        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
            return 1;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *result = (const char *)sqlite3_column_text(stmt, 0);
            printf("  Query for '%s': %s\n", names[i], result);
        }
        sqlite3_finalize(stmt);
    }

    printf("\nQuerying all nodes without parameters:\n");
    
    // Query all nodes
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, 
        "SELECT cypher('MATCH (a:Person) RETURN id(a) AS node_id, a.name AS name')",
        -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *result = (const char *)sqlite3_column_text(stmt, 0);
            printf("  %s\n", result);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    printf("\nTest complete.\n");
    return 0;
}
