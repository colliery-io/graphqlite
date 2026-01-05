/*
 * GraphQLite Main Application
 * Interactive Cypher query execution with persistent SQLite storage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

#define MAX_QUERY_LENGTH 65536
#define MAX_LINE_LENGTH 4096
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
    printf("Enter Cypher queries terminated with semicolon (;)\n\n");
    printf("Cypher Examples:\n");
    printf("  CREATE (n:Person {name: 'Alice'});\n");
    printf("  MATCH (n:Person) RETURN n;\n");
    printf("  MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})\n");
    printf("      CREATE (a)-[:KNOWS]->(b);\n\n");
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

/* Check if a string ends with semicolon (ignoring trailing whitespace) */
static bool ends_with_semicolon(const char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r')) {
        len--;
    }
    return len > 0 && str[len-1] == ';';
}

/* Trim trailing semicolon and whitespace, return new string (caller must free) */
static char *trim_semicolon(const char *str)
{
    size_t len = strlen(str);

    /* Trim trailing whitespace */
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r')) {
        len--;
    }

    /* Trim semicolon */
    if (len > 0 && str[len-1] == ';') {
        len--;
    }

    /* Trim more whitespace after semicolon removal */
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r')) {
        len--;
    }

    char *result = malloc(len + 1);
    if (result) {
        memcpy(result, str, len);
        result[len] = '\0';
    }
    return result;
}

/* Execute a single Cypher statement */
static void execute_statement(cypher_executor *executor, const char *query, bool verbose)
{
    if (verbose) {
        printf("Executing: %s\n", query);
    }

    cypher_result *result = cypher_executor_execute(executor, query);

    if (result) {
        if (result->success) {
            /* Print statistics for modification queries */
            if (result->nodes_created > 0 || result->nodes_deleted > 0 ||
                result->relationships_created > 0 || result->relationships_deleted > 0 ||
                result->properties_set > 0) {
                printf("Query executed successfully\n");
                if (result->nodes_created > 0)
                    printf("  Nodes created: %d\n", result->nodes_created);
                if (result->nodes_deleted > 0)
                    printf("  Nodes deleted: %d\n", result->nodes_deleted);
                if (result->relationships_created > 0)
                    printf("  Relationships created: %d\n", result->relationships_created);
                if (result->relationships_deleted > 0)
                    printf("  Relationships deleted: %d\n", result->relationships_deleted);
                if (result->properties_set > 0)
                    printf("  Properties set: %d\n", result->properties_set);
            }

            /* Print result data for read queries */
            if (result->row_count > 0 && result->column_count > 0) {
                cypher_result_print(result);
            }

        } else {
            printf("Query failed: %s\n", result->error_message ? result->error_message : "Unknown error");
        }

        cypher_result_free(result);
    } else {
        printf("Failed to execute query\n");
    }
}

/* Main interactive loop */
static int run_interactive(cypher_executor *executor, sqlite3 *db, bool verbose)
{
    char line[MAX_LINE_LENGTH];
    char query[MAX_QUERY_LENGTH];
    bool in_statement = false;
    bool is_tty = isatty(fileno(stdin));

    if (is_tty) {
        printf("GraphQLite Interactive Shell\n");
        printf("Type .help for help, .quit to exit\n");
        printf("Queries must end with semicolon (;)\n\n");
    }

    query[0] = '\0';

    while (1) {
        if (is_tty) {
            printf("%s", in_statement ? "       ...> " : "graphqlite> ");
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), stdin)) {
            /* EOF - execute any pending statement */
            if (in_statement && strlen(query) > 0) {
                char *trimmed = trim_semicolon(query);
                if (trimmed && strlen(trimmed) > 0) {
                    execute_statement(executor, trimmed, verbose || is_tty);
                }
                free(trimmed);
            }
            break;
        }

        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }

        /* Skip empty lines when not in a statement */
        if (!in_statement && len == 0) {
            continue;
        }

        /* Handle dot commands (only at start of statement) */
        if (!in_statement && line[0] == '.') {
            if (strcmp(line, ".quit") == 0 || strcmp(line, ".exit") == 0) {
                break;
            } else if (strcmp(line, ".help") == 0) {
                print_interactive_help();
            } else if (strcmp(line, ".schema") == 0) {
                show_schema(db);
            } else if (strcmp(line, ".tables") == 0) {
                show_tables(db);
            } else if (strcmp(line, ".stats") == 0) {
                show_stats(db);
            } else {
                printf("Unknown command: %s\n", line);
                printf("Type .help for available commands\n");
            }
            continue;
        }

        /* Accumulate line into query buffer */
        size_t query_len = strlen(query);
        size_t line_len = strlen(line);

        /* Add space between lines if accumulating */
        if (in_statement && query_len > 0) {
            if (query_len + 1 < MAX_QUERY_LENGTH) {
                query[query_len] = ' ';
                query_len++;
            }
        }

        /* Check buffer overflow */
        if (query_len + line_len >= MAX_QUERY_LENGTH) {
            printf("Error: Query too long (max %d characters)\n", MAX_QUERY_LENGTH);
            query[0] = '\0';
            in_statement = false;
            continue;
        }

        /* Append line */
        memcpy(query + query_len, line, line_len + 1);
        in_statement = true;

        /* Check if statement is complete (ends with semicolon) */
        if (ends_with_semicolon(query)) {
            char *trimmed = trim_semicolon(query);
            if (trimmed && strlen(trimmed) > 0) {
                execute_statement(executor, trimmed, verbose || is_tty);
            }
            free(trimmed);

            /* Reset for next statement */
            query[0] = '\0';
            in_statement = false;

            if (is_tty) {
                printf("\n");
            }
        }
    }

    if (is_tty) {
        printf("Goodbye!\n");
    }
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
    int result = run_interactive(executor, db, verbose);
    
    /* Cleanup */
    cypher_executor_free(executor);
    sqlite3_close(db);
    
    return result;
}