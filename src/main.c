/*
 * GraphQLite Main Application
 * Interactive Cypher query execution with persistent SQLite storage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

#define MAX_QUERY_LENGTH 4096
#define DEFAULT_DB_PATH "graphqlite.db"

/* Print usage information */
static void print_usage(const char *program_name)
{
    printf("Usage: %s [options] [database_file]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --verbose  Enable verbose debug output\n");
    printf("  -i, --init     Initialize new database (will overwrite existing)\n");
    printf("\nArguments:\n");
    printf("  database_file  SQLite database file (default: %s)\n", DEFAULT_DB_PATH);
    printf("\nInteractive Commands:\n");
    printf("  .help          Show available commands\n");
    printf("  .schema        Show database schema\n");
    printf("  .quit          Exit the application\n");
    printf("  .tables        Show all tables\n");
    printf("  .stats         Show database statistics\n");
}

/* Print interactive help */
static void print_interactive_help(void)
{
    printf("\nGraphQLite Interactive Shell\n");
    printf("Enter Cypher queries or dot commands:\n\n");
    printf("Cypher Examples:\n");
    printf("  CREATE (n:Person {name: 'Alice'})\n");
    printf("  CREATE (a:Person)-[:KNOWS]->(b:Person)\n");
    printf("  MATCH (n:Person) RETURN n\n");
    printf("  MATCH (a)-[:KNOWS]->(b) RETURN a, b\n\n");
    printf("Dot Commands:\n");
    printf("  .help     - Show this help\n");
    printf("  .schema   - Show database schema\n");
    printf("  .tables   - List all tables\n");
    printf("  .stats    - Show database statistics\n");
    printf("  .quit     - Exit\n\n");
}

/* Show database schema */
static void show_schema(sqlite3 *db)
{
    const char *sql = "SELECT name, sql FROM sqlite_master WHERE type='table' ORDER BY name";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to query schema: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    printf("\nDatabase Schema:\n");
    printf("================\n");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *table_name = (const char*)sqlite3_column_text(stmt, 0);
        const char *create_sql = (const char*)sqlite3_column_text(stmt, 1);
        
        printf("\nTable: %s\n", table_name);
        printf("%s;\n", create_sql);
    }
    
    sqlite3_finalize(stmt);
}

/* Show database tables */
static void show_tables(sqlite3 *db)
{
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to query tables: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    printf("\nTables:\n");
    printf("=======\n");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *table_name = (const char*)sqlite3_column_text(stmt, 0);
        printf("  %s\n", table_name);
    }
    
    sqlite3_finalize(stmt);
}

/* Show database statistics */
static void show_stats(sqlite3 *db)
{
    printf("\nDatabase Statistics:\n");
    printf("===================\n");
    
    const char *queries[] = {
        "SELECT COUNT(*) FROM nodes",
        "SELECT COUNT(*) FROM edges", 
        "SELECT COUNT(*) FROM node_labels",
        "SELECT COUNT(*) FROM property_keys"
    };
    
    const char *labels[] = {
        "Nodes",
        "Edges", 
        "Node Labels",
        "Property Keys"
    };
    
    for (int i = 0; i < 4; i++) {
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db, queries[i], -1, &stmt, NULL);
        if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            printf("  %-15s: %d\n", labels[i], count);
        } else {
            printf("  %-15s: Error querying\n", labels[i]);
        }
        sqlite3_finalize(stmt);
    }
    
    /* Show distinct edge types */
    const char *edge_types_sql = "SELECT DISTINCT type FROM edges ORDER BY type";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, edge_types_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        printf("  Edge Types      : ");
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) printf(", ");
            printf("%s", sqlite3_column_text(stmt, 0));
            first = false;
        }
        if (first) printf("(none)");
        printf("\n");
    }
    sqlite3_finalize(stmt);
}

/* Initialize database by removing existing file */
static int initialize_database(const char *db_path)
{
    printf("Initializing database: %s\n", db_path);
    
    /* Remove existing file */
    if (remove(db_path) == 0) {
        printf("Removed existing database file\n");
    }
    
    return 0;
}

/* Main interactive loop */
static int run_interactive(cypher_executor *executor, sqlite3 *db)
{
    char query[MAX_QUERY_LENGTH];
    
    printf("GraphQLite Interactive Shell\n");
    printf("Type .help for help, .quit to exit\n\n");
    
    while (1) {
        printf("graphqlite> ");
        fflush(stdout);
        
        if (!fgets(query, sizeof(query), stdin)) {
            break; /* EOF */
        }
        
        /* Remove trailing newline */
        size_t len = strlen(query);
        if (len > 0 && query[len-1] == '\n') {
            query[len-1] = '\0';
        }
        
        /* Skip empty lines */
        if (strlen(query) == 0) {
            continue;
        }
        
        /* Handle dot commands */
        if (query[0] == '.') {
            if (strcmp(query, ".quit") == 0 || strcmp(query, ".exit") == 0) {
                break;
            } else if (strcmp(query, ".help") == 0) {
                print_interactive_help();
            } else if (strcmp(query, ".schema") == 0) {
                show_schema(db);
            } else if (strcmp(query, ".tables") == 0) {
                show_tables(db);
            } else if (strcmp(query, ".stats") == 0) {
                show_stats(db);
            } else {
                printf("Unknown command: %s\n", query);
                printf("Type .help for available commands\n");
            }
            continue;
        }
        
        /* Execute Cypher query */
        printf("Executing: %s\n", query);
        cypher_result *result = cypher_executor_execute(executor, query);
        
        if (result) {
            if (result->success) {
                printf("Query executed successfully\n");
                
                /* Print statistics for modification queries */
                if (result->nodes_created > 0 || result->relationships_created > 0 || result->properties_set > 0) {
                    printf("  Nodes created: %d\n", result->nodes_created);
                    printf("  Relationships created: %d\n", result->relationships_created);
                    printf("  Properties set: %d\n", result->properties_set);
                }
                
                /* Print result data for read queries */
                if (result->row_count > 0 && result->column_count > 0) {
                    printf("\nResults:\n");
                    cypher_result_print(result);
                }
                
            } else {
                printf("Query failed: %s\n", result->error_message ? result->error_message : "Unknown error");
            }
            
            cypher_result_free(result);
        } else {
            printf("Failed to execute query\n");
        }
        
        printf("\n");
    }
    
    printf("Goodbye!\n");
    return 0;
}

int main(int argc, char *argv[])
{
    const char *db_path = DEFAULT_DB_PATH;
    bool verbose = false;
    bool init_db = false;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--init") == 0) {
            init_db = true;
        } else if (argv[i][0] != '-') {
            /* Database file path */
            db_path = argv[i];
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    /* Initialize database if requested */
    if (init_db) {
        initialize_database(db_path);
    }
    
    /* Open SQLite database */
    sqlite3 *db;
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        printf("Failed to open database '%s': %s\n", db_path, sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    
    printf("Opened database: %s\n", db_path);
    
    /* Enable foreign key constraints */
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to enable foreign keys: %s\n", sqlite3_errmsg(db));
    }
    
    /* Create executor */
    cypher_executor *executor = cypher_executor_create(db);
    if (!executor) {
        printf("Failed to create Cypher executor\n");
        sqlite3_close(db);
        return 1;
    }
    
    printf("GraphQLite executor initialized\n");
    
    if (verbose) {
        printf("Debug mode enabled\n");
    }
    
    /* Run interactive shell */
    int result = run_interactive(executor, db);
    
    /* Cleanup */
    cypher_executor_free(executor);
    sqlite3_close(db);
    
    return result;
}