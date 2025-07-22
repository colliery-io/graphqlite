#ifndef GRAPHQLITE_H
#define GRAPHQLITE_H

#include <stdint.h>
#include <stddef.h>

// Forward declare SQLite types
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_context sqlite3_context;
typedef struct sqlite3_value sqlite3_value;
typedef struct sqlite3_api_routines sqlite3_api_routines;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Types
// ============================================================================

typedef enum {
    GRAPHQLITE_OK = 0,
    GRAPHQLITE_ERROR = 1,
    GRAPHQLITE_NOMEM = 2,
    GRAPHQLITE_NOTFOUND = 3,
    GRAPHQLITE_INVALID = 4
} graphqlite_result_code_t;

typedef enum {
    GRAPHQLITE_TYPE_NULL = 0,
    GRAPHQLITE_TYPE_INTEGER = 1,
    GRAPHQLITE_TYPE_FLOAT = 2,
    GRAPHQLITE_TYPE_TEXT = 3,
    GRAPHQLITE_TYPE_BLOB = 4,
    GRAPHQLITE_TYPE_BOOLEAN = 5
} graphqlite_value_type_t;

// Forward declarations
typedef struct graphqlite_result graphqlite_result_t;
typedef struct graphqlite_column graphqlite_column_t;
typedef struct graphqlite_row graphqlite_row_t;
typedef struct graphqlite_value graphqlite_value_t;

// ============================================================================
// Result Management
// ============================================================================

struct graphqlite_value {
    graphqlite_value_type_t type;
    union {
        int64_t integer;
        double float_val;
        char *text;
        struct {
            void *data;
            size_t size;
        } blob;
        int boolean;
    } data;
};

struct graphqlite_column {
    char *name;
    graphqlite_value_type_t type;
};

struct graphqlite_row {
    graphqlite_value_t *values;
    size_t column_count;
};

struct graphqlite_result {
    graphqlite_column_t *columns;
    graphqlite_row_t *rows;
    size_t column_count;
    size_t row_count;
    char *error_message;
    graphqlite_result_code_t result_code;
};

// ============================================================================
// SQLite Extension API
// ============================================================================

// Main extension entry point
int sqlite3_graphqlite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

// Core query execution function
void graphqlite_cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv);

// ============================================================================
// Result Management Functions
// ============================================================================

graphqlite_result_t* graphqlite_result_create(void);
void graphqlite_result_free(graphqlite_result_t *result);
void graphqlite_result_set_error(graphqlite_result_t *result, const char *error_msg);
int graphqlite_result_add_column(graphqlite_result_t *result, const char *name, graphqlite_value_type_t type);
int graphqlite_result_add_row(graphqlite_result_t *result);
int graphqlite_result_set_value(graphqlite_result_t *result, size_t row, size_t col, const graphqlite_value_t *value);

// ============================================================================
// Value Management Functions
// ============================================================================

graphqlite_value_t* graphqlite_value_create_null(void);
graphqlite_value_t* graphqlite_value_create_integer(int64_t value);
graphqlite_value_t* graphqlite_value_create_float(double value);
graphqlite_value_t* graphqlite_value_create_text(const char *value);
graphqlite_value_t* graphqlite_value_create_boolean(int value);
void graphqlite_value_free(graphqlite_value_t *value);

#ifdef __cplusplus
}
#endif

#endif // GRAPHQLITE_H