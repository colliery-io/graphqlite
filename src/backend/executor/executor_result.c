/*
 * Executor Result Implementation
 * Creates and manages execution result structures
 */

#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"

/* Create empty result */
cypher_result* create_empty_result(void)
{
    cypher_result *result = calloc(1, sizeof(cypher_result));
    if (!result) {
        return NULL;
    }

    result->success = false;
    result->error_message = NULL;
    result->row_count = 0;
    result->column_count = 0;
    result->column_names = NULL;
    result->data = NULL;
    result->data_types = NULL;
    result->agtype_data = NULL;
    result->use_agtype = false;
    result->nodes_created = 0;
    result->nodes_deleted = 0;
    result->relationships_created = 0;
    result->relationships_deleted = 0;
    result->properties_set = 0;

    return result;
}

/* Set error message in result */
void set_result_error(cypher_result *result, const char *error_msg)
{
    if (!result || !error_msg) {
        return;
    }

    result->success = false;
    result->error_message = strdup(error_msg);
}
