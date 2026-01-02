/**
 * Test: Parameters in MATCH WHERE clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

static int callback(void *data, int argc, char **argv, char **colnames) {
    for (int i = 0; i < argc; i++) {
        printf("  %s = %s\n", colnames[i], argv[i] ? argv[i] : "NULL");
    }
    return 0;
}

int main() {
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    sqlite3_enable_load_extension(db, 1);
    rc = sqlite3_load_extension(db, "./build/graphqlite", NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Load failed: %s\n", err_msg);
        return 1;
    }

    printf("=== Test 1: CREATE with literal values ===\n");
    sqlite3_exec(db, "SELECT cypher('CREATE (a:Person {name: \"Alice\"})')", callback, NULL, &err_msg);
    sqlite3_exec(db, "SELECT cypher('CREATE (a:Person {name: \"Bob\"})')", callback, NULL, &err_msg);
    
    printf("\n=== Query all nodes ===\n");
    sqlite3_exec(db, "SELECT cypher('MATCH (a:Person) RETURN a.name AS name')", callback, NULL, &err_msg);

    printf("\n=== Test 2: MATCH with literal filter ===\n");
    sqlite3_exec(db, "SELECT cypher('MATCH (a:Person {name: \"Alice\"}) RETURN a.name AS name')", callback, NULL, &err_msg);

    printf("\n=== Test 3: MATCH with parameter filter ===\n");
    sqlite3_exec(db, "SELECT cypher('MATCH (a:Person {name: $name}) RETURN a.name AS name', '{\"name\": \"Alice\"}')", callback, NULL, &err_msg);

    printf("\n=== Test 4: CREATE with parameter ===\n");
    sqlite3_exec(db, "SELECT cypher('CREATE (a:Person {name: $name})', '{\"name\": \"Charlie\"}')", callback, NULL, &err_msg);
    
    printf("\n=== Query all after param CREATE ===\n");
    sqlite3_exec(db, "SELECT cypher('MATCH (a:Person) RETURN a.name AS name')", callback, NULL, &err_msg);

    sqlite3_close(db);
    return 0;
}
