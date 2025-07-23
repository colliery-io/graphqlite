#include "graphqlite.h"
#include "graphqlite_internal.h"
#include "../cypher/cypher_parser.h"
#include "../cypher/cypher_executor.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Public API Implementation
// ============================================================================

int graphqlite_exec(graphqlite_t *db, const char *query, graphqlite_result_t **result) {
    if (!db || !query || !result) {
        return GRAPHQLITE_INVALID;
    }
    
    // Cast to internal database type
    graphqlite_db_t *internal_db = (graphqlite_db_t*)db;
    
    // Parse the openCypher query
    cypher_parse_result_t *parse_result = cypher_parse(query);
    if (!parse_result) {
        internal_db->last_error_code = GRAPHQLITE_NOMEM;
        internal_db->last_error_message = strdup("Failed to allocate parser");
        return GRAPHQLITE_NOMEM;
    }
    
    // Check for parse errors
    if (cypher_parse_result_has_error(parse_result)) {
        internal_db->last_error_code = GRAPHQLITE_ERROR;
        const char *error_msg = cypher_parse_result_get_error(parse_result);
        internal_db->last_error_message = strdup(error_msg ? error_msg : "Parse error");
        cypher_parse_result_destroy(parse_result);
        return GRAPHQLITE_ERROR;
    }
    
    // Execute the parsed query
    cypher_result_t *cypher_result = cypher_execute_query(internal_db, query);
    cypher_parse_result_destroy(parse_result);
    
    if (!cypher_result) {
        internal_db->last_error_code = GRAPHQLITE_ERROR;
        internal_db->last_error_message = strdup("Query execution failed");
        return GRAPHQLITE_ERROR;
    }
    
    // Check for execution errors
    if (cypher_result_has_error(cypher_result)) {
        internal_db->last_error_code = GRAPHQLITE_ERROR;
        const char *error_msg = cypher_result_get_error(cypher_result);
        internal_db->last_error_message = strdup(error_msg ? error_msg : "Execution error");
        cypher_result_destroy(cypher_result);
        return GRAPHQLITE_ERROR;
    }
    
    // Convert cypher_result_t to graphqlite_result_t
    // For now, we'll use a simple wrapper approach
    *result = (graphqlite_result_t*)cypher_result;
    
    return GRAPHQLITE_OK;
}

// ============================================================================
// openCypher is now the primary and only query language
// ============================================================================

// ============================================================================
// Result interface functions (bridge cypher_result_t to public API)
// ============================================================================

int graphqlite_result_step(graphqlite_result_t *result) {
    if (!result) return GRAPHQLITE_INVALID;
    
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    
    // For now, return OK if we have rows, otherwise end of data
    // This will need proper iteration implementation later
    size_t row_count = cypher_result_get_row_count(cypher_result);
    return (row_count > 0) ? GRAPHQLITE_OK : GRAPHQLITE_NOTFOUND;
}

int graphqlite_result_column_count(graphqlite_result_t *result) {
    if (!result) return 0;
    
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    return (int)cypher_result_get_column_count(cypher_result);
}

const char* graphqlite_result_column_name(graphqlite_result_t *result, int col) {
    if (!result || col < 0) return NULL;
    
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    return cypher_result_get_column_name(cypher_result, (size_t)col);
}

graphqlite_value_type_t graphqlite_result_column_type(graphqlite_result_t *result, int col) {
    if (!result || col < 0) return GRAPHQLITE_TYPE_NULL;
    
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    size_t row_count = cypher_result_get_row_count(cypher_result);
    
    if (row_count == 0) return GRAPHQLITE_TYPE_NULL;
    
    // Get the first row's value to determine type
    cypher_value_t *value = cypher_result_get_value(cypher_result, 0, (size_t)col);
    if (!value) return GRAPHQLITE_TYPE_NULL;
    
    // Convert cypher property type to graphqlite type
    switch (value->type) {
        case PROP_INT: return GRAPHQLITE_TYPE_INTEGER;
        case PROP_TEXT: return GRAPHQLITE_TYPE_TEXT;
        case PROP_REAL: return GRAPHQLITE_TYPE_FLOAT;
        case PROP_BOOL: return GRAPHQLITE_TYPE_BOOLEAN;
        case PROP_NULL: return GRAPHQLITE_TYPE_NULL;
        default: return GRAPHQLITE_TYPE_NULL;
    }
}

const void* graphqlite_result_column_value(graphqlite_result_t *result, int col) {
    if (!result || col < 0) return NULL;
    
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    size_t row_count = cypher_result_get_row_count(cypher_result);
    
    if (row_count == 0) return NULL;
    
    // Get the first row's value (for now, we'll need proper iteration later)
    cypher_value_t *value = cypher_result_get_value(cypher_result, 0, (size_t)col);
    if (!value) return NULL;
    
    // Return pointer to the value based on type
    switch (value->type) {
        case PROP_INT: return &value->value.int_val;
        case PROP_TEXT: return value->value.text_val;
        case PROP_REAL: return &value->value.real_val;
        case PROP_BOOL: return &value->value.bool_val;
        case PROP_NULL: return NULL;
        default: return NULL;
    }
}

int64_t graphqlite_result_column_int64(graphqlite_result_t *result, int col) {
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    if (!cypher_result || col < 0) return 0;
    
    size_t row_count = cypher_result_get_row_count(cypher_result);
    if (row_count == 0) return 0;
    
    cypher_value_t *value = cypher_result_get_value(cypher_result, 0, (size_t)col);
    return (value && value->type == PROP_INT) ? value->value.int_val : 0;
}

double graphqlite_result_column_double(graphqlite_result_t *result, int col) {
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    if (!cypher_result || col < 0) return 0.0;
    
    size_t row_count = cypher_result_get_row_count(cypher_result);
    if (row_count == 0) return 0.0;
    
    cypher_value_t *value = cypher_result_get_value(cypher_result, 0, (size_t)col);
    return (value && value->type == PROP_REAL) ? value->value.real_val : 0.0;
}

const char* graphqlite_result_column_text(graphqlite_result_t *result, int col) {
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    if (!cypher_result || col < 0) return NULL;
    
    size_t row_count = cypher_result_get_row_count(cypher_result);
    if (row_count == 0) return NULL;
    
    cypher_value_t *value = cypher_result_get_value(cypher_result, 0, (size_t)col);
    return (value && value->type == PROP_TEXT) ? value->value.text_val : NULL;
}

const void* graphqlite_result_column_blob(graphqlite_result_t *result, int col, int *size) {
    // Not implemented yet - openCypher doesn't have blob types
    if (size) *size = 0;
    return NULL;
}

int graphqlite_result_column_bool(graphqlite_result_t *result, int col) {
    cypher_result_t *cypher_result = (cypher_result_t*)result;
    if (!cypher_result || col < 0) return 0;
    
    size_t row_count = cypher_result_get_row_count(cypher_result);
    if (row_count == 0) return 0;
    
    cypher_value_t *value = cypher_result_get_value(cypher_result, 0, (size_t)col);
    return (value && value->type == PROP_BOOL) ? value->value.bool_val : 0;
}

void graphqlite_result_free(graphqlite_result_t *result) {
    if (result) {
        cypher_result_t *cypher_result = (cypher_result_t*)result;
        cypher_result_destroy(cypher_result);
    }
}