#include "graphqlite.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Result Management Implementation
// ============================================================================

graphqlite_result_t* graphqlite_result_create(void) {
    graphqlite_result_t *result = malloc(sizeof(graphqlite_result_t));
    if (!result) return NULL;
    
    memset(result, 0, sizeof(graphqlite_result_t));
    result->result_code = GRAPHQLITE_OK;
    return result;
}

void graphqlite_result_free(graphqlite_result_t *result) {
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
                for (size_t j = 0; j < result->rows[i].column_count; j++) {
                    graphqlite_value_free(&result->rows[i].values[j]);
                }
                free(result->rows[i].values);
            }
        }
        free(result->rows);
    }
    
    // Free error message
    free(result->error_message);
    
    free(result);
}

void graphqlite_result_set_error(graphqlite_result_t *result, const char *error_msg) {
    if (!result) return;
    
    free(result->error_message);
    result->error_message = error_msg ? strdup(error_msg) : NULL;
    result->result_code = GRAPHQLITE_ERROR;
}

int graphqlite_result_add_column(graphqlite_result_t *result, const char *name, graphqlite_value_type_t type) {
    if (!result || !name) return GRAPHQLITE_INVALID;
    
    // Reallocate columns array
    graphqlite_column_t *new_columns = realloc(result->columns, 
        (result->column_count + 1) * sizeof(graphqlite_column_t));
    if (!new_columns) return GRAPHQLITE_NOMEM;
    
    result->columns = new_columns;
    
    // Initialize new column
    result->columns[result->column_count].name = strdup(name);
    result->columns[result->column_count].type = type;
    
    if (!result->columns[result->column_count].name) {
        return GRAPHQLITE_NOMEM;
    }
    
    result->column_count++;
    return GRAPHQLITE_OK;
}

int graphqlite_result_add_row(graphqlite_result_t *result) {
    if (!result) return GRAPHQLITE_INVALID;
    
    // Reallocate rows array
    graphqlite_row_t *new_rows = realloc(result->rows, 
        (result->row_count + 1) * sizeof(graphqlite_row_t));
    if (!new_rows) return GRAPHQLITE_NOMEM;
    
    result->rows = new_rows;
    
    // Initialize new row
    result->rows[result->row_count].values = NULL;
    result->rows[result->row_count].column_count = result->column_count;
    
    if (result->column_count > 0) {
        result->rows[result->row_count].values = calloc(result->column_count, sizeof(graphqlite_value_t));
        if (!result->rows[result->row_count].values) {
            return GRAPHQLITE_NOMEM;
        }
    }
    
    result->row_count++;
    return GRAPHQLITE_OK;
}

int graphqlite_result_set_value(graphqlite_result_t *result, size_t row, size_t col, const graphqlite_value_t *value) {
    if (!result || !value || row >= result->row_count || col >= result->column_count) {
        return GRAPHQLITE_INVALID;
    }
    
    // Free existing value if it has allocated memory
    graphqlite_value_free(&result->rows[row].values[col]);
    
    // Copy new value
    result->rows[row].values[col] = *value;
    
    // For text values, we need to duplicate the string
    if (value->type == GRAPHQLITE_TYPE_TEXT && value->data.text) {
        result->rows[row].values[col].data.text = strdup(value->data.text);
        if (!result->rows[row].values[col].data.text) {
            result->rows[row].values[col].type = GRAPHQLITE_TYPE_NULL;
            return GRAPHQLITE_NOMEM;
        }
    }
    
    return GRAPHQLITE_OK;
}

// ============================================================================
// Value Management Implementation
// ============================================================================

graphqlite_value_t* graphqlite_value_create_null(void) {
    graphqlite_value_t *value = malloc(sizeof(graphqlite_value_t));
    if (!value) return NULL;
    
    value->type = GRAPHQLITE_TYPE_NULL;
    return value;
}

graphqlite_value_t* graphqlite_value_create_integer(int64_t val) {
    graphqlite_value_t *value = malloc(sizeof(graphqlite_value_t));
    if (!value) return NULL;
    
    value->type = GRAPHQLITE_TYPE_INTEGER;
    value->data.integer = val;
    return value;
}

graphqlite_value_t* graphqlite_value_create_float(double val) {
    graphqlite_value_t *value = malloc(sizeof(graphqlite_value_t));
    if (!value) return NULL;
    
    value->type = GRAPHQLITE_TYPE_FLOAT;
    value->data.float_val = val;
    return value;
}

graphqlite_value_t* graphqlite_value_create_text(const char *val) {
    graphqlite_value_t *value = malloc(sizeof(graphqlite_value_t));
    if (!value) return NULL;
    
    value->type = GRAPHQLITE_TYPE_TEXT;
    value->data.text = val ? strdup(val) : NULL;
    return value;
}

graphqlite_value_t* graphqlite_value_create_boolean(int val) {
    graphqlite_value_t *value = malloc(sizeof(graphqlite_value_t));
    if (!value) return NULL;
    
    value->type = GRAPHQLITE_TYPE_BOOLEAN;
    value->data.boolean = val ? 1 : 0;
    return value;
}

void graphqlite_value_free(graphqlite_value_t *value) {
    if (!value) return;
    
    switch (value->type) {
        case GRAPHQLITE_TYPE_TEXT:
            free(value->data.text);
            break;
        case GRAPHQLITE_TYPE_BLOB:
            free(value->data.blob.data);
            break;
        default:
            // Other types don't need special cleanup
            break;
    }
    
    // Note: Don't free the value itself here since it might be stack-allocated
    // Only free the contents
}