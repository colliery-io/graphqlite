#include "cypher_executor.h"
#include "cypher_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// STEP 1: Minimal Result Management (Working Foundation)
// ============================================================================

cypher_result_t* cypher_result_create(void) {
    cypher_result_t *result = calloc(1, sizeof(cypher_result_t));
    if (!result) return NULL;
    
    // Zero-initialized by calloc - all pointers NULL, counts 0, no errors
    return result;
}

void cypher_result_destroy(cypher_result_t *result) {
    if (!result) return;
    
    // Free columns
    if (result->columns) {
        for (size_t i = 0; i < result->column_count; i++) {
            free(result->columns[i].name);
        }
        free(result->columns);
    }
    
    // Free rows
    if (result->rows) {
        for (size_t i = 0; i < result->row_count; i++) {
            if (result->rows[i].values) {
                for (size_t j = 0; j < result->column_count; j++) {
                    cypher_value_free(&result->rows[i].values[j]);
                }
                free(result->rows[i].values);
            }
        }
        free(result->rows);
    }
    
    free(result->error_message);
    free(result);
}

static void cypher_result_set_error(cypher_result_t *result, const char *message) {
    if (!result) return;
    
    result->has_error = true;
    free(result->error_message);
    result->error_message = strdup(message);
}

// ============================================================================
// STEP 1: Basic Value Management (Safe & Simple)
// ============================================================================

void cypher_value_free(cypher_value_t *value) {
    if (!value) return;
    
    switch (value->type) {
        case PROP_TEXT:
            free(value->value.text_val);
            value->value.text_val = NULL;
            break;
        case PROP_INT:
        case PROP_REAL:
        case PROP_BOOL:
        case PROP_NULL:
            // No additional cleanup needed
            break;
    }
    
    value->type = PROP_NULL;
}

// ============================================================================
// STEP 1: Result Access Functions (Read-Only, Safe)
// ============================================================================

size_t cypher_result_get_column_count(cypher_result_t *result) {
    return result ? result->column_count : 0;
}

const char* cypher_result_get_column_name(cypher_result_t *result, size_t column_index) {
    if (!result || column_index >= result->column_count) return NULL;
    return result->columns[column_index].name;
}

size_t cypher_result_get_row_count(cypher_result_t *result) {
    return result ? result->row_count : 0;
}

cypher_value_t* cypher_result_get_value(cypher_result_t *result, size_t row_index, size_t column_index) {
    if (!result || row_index >= result->row_count || column_index >= result->column_count) {
        return NULL;
    }
    return &result->rows[row_index].values[column_index];
}

bool cypher_result_has_error(cypher_result_t *result) {
    return result ? result->has_error : true;
}

const char* cypher_result_get_error(cypher_result_t *result) {
    return result ? result->error_message : "Invalid result";
}

// ============================================================================
// STEP 1: Minimal Query Interface (Returns Empty Results For Now)
// ============================================================================

// ============================================================================
// STEP 2: Add Query Parsing (Safe Integration with LEMON Parser)
// ============================================================================

cypher_result_t* cypher_execute_query(graphqlite_db_t *db, const char *query) {
    if (!db || !query) {
        cypher_result_t *result = cypher_result_create();
        if (result) {
            cypher_result_set_error(result, "Invalid database or query string");
        }
        return result;
    }
    
    // STEP 2: Try to parse the query first
    cypher_parse_result_t *parse_result = cypher_parse(query);
    if (!parse_result) {
        cypher_result_t *result = cypher_result_create();
        if (result) {
            cypher_result_set_error(result, "Parse failed - could not create parser");
        }
        return result;
    }
    
    // Check for parse errors
    if (cypher_parse_result_has_error(parse_result)) {
        cypher_result_t *result = cypher_result_create();
        if (result) {
            const char *parse_error = cypher_parse_result_get_error(parse_result);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Parse error: %s", parse_error ? parse_error : "Unknown");
            cypher_result_set_error(result, error_msg);
        }
        // Clean up parse result (now with improved AST cleanup)
        cypher_parse_result_destroy(parse_result);
        return result;
    }
    
    // Parse succeeded! For now, still return empty result but with success
    cypher_result_t *result = cypher_result_create();
    if (!result) {
        cypher_parse_result_destroy(parse_result);
        return NULL;
    }
    
    // Set up one column named "n" (basic MATCH (n) RETURN n expectation)
    result->column_count = 1;
    result->columns = calloc(1, sizeof(cypher_column_t));
    if (result->columns) {
        result->columns[0].name = strdup("n");
        result->columns[0].type = PROP_INT;
    }
    
    // Zero rows for now (Step 3 will add actual node retrieval)
    result->row_count = 0;
    result->rows = NULL;
    
    // Clean up parse result (now with improved AST cleanup)
    cypher_parse_result_destroy(parse_result);
    
    return result;
}