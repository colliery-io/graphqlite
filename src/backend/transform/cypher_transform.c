/*
 * Cypher AST to SQL transformation
 * Converts parsed Cypher queries into executable SQL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Initial buffer sizes */
#define INITIAL_SQL_BUFFER_SIZE 1024
#define INITIAL_VARIABLE_CAPACITY 16

/* Transform context management */

cypher_transform_context* cypher_transform_create_context(sqlite3 *db)
{
    cypher_transform_context *ctx = calloc(1, sizeof(cypher_transform_context));
    if (!ctx) {
        return NULL;
    }
    
    ctx->db = db;
    
    /* Initialize SQL buffer */
    ctx->sql_buffer = malloc(INITIAL_SQL_BUFFER_SIZE);
    if (!ctx->sql_buffer) {
        free(ctx);
        return NULL;
    }
    ctx->sql_buffer[0] = '\0';
    ctx->sql_capacity = INITIAL_SQL_BUFFER_SIZE;
    ctx->sql_size = 0;
    
    /* Initialize variable tracking */
    ctx->variables = calloc(INITIAL_VARIABLE_CAPACITY, sizeof(ctx->variables[0]));
    if (!ctx->variables) {
        free(ctx->sql_buffer);
        free(ctx);
        return NULL;
    }
    ctx->variable_capacity = INITIAL_VARIABLE_CAPACITY;
    ctx->variable_count = 0;
    
    ctx->query_type = QUERY_TYPE_UNKNOWN;
    ctx->has_error = false;
    ctx->error_message = NULL;
    
    CYPHER_DEBUG("Created transform context %p", (void*)ctx);
    
    return ctx;
}

void cypher_transform_free_context(cypher_transform_context *ctx)
{
    if (!ctx) {
        return;
    }
    
    CYPHER_DEBUG("Freeing transform context %p", (void*)ctx);
    
    /* Free variables */
    for (int i = 0; i < ctx->variable_count; i++) {
        free(ctx->variables[i].name);
        free(ctx->variables[i].table_alias);
    }
    free(ctx->variables);
    
    /* Free buffers */
    free(ctx->sql_buffer);
    free(ctx->error_message);
    
    free(ctx);
}

/* SQL generation helpers */

void append_sql(cypher_transform_context *ctx, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    
    /* Calculate required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    /* Grow buffer if needed */
    size_t new_size = ctx->sql_size + required + 1;
    if (new_size > ctx->sql_capacity) {
        size_t new_capacity = ctx->sql_capacity * 2;
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }
        
        char *new_buffer = realloc(ctx->sql_buffer, new_capacity);
        if (!new_buffer) {
            ctx->has_error = true;
            ctx->error_message = strdup("Out of memory during SQL generation");
            va_end(args);
            return;
        }
        
        ctx->sql_buffer = new_buffer;
        ctx->sql_capacity = new_capacity;
    }
    
    /* Append to buffer */
    int written = vsnprintf(ctx->sql_buffer + ctx->sql_size, 
                           ctx->sql_capacity - ctx->sql_size, 
                           format, args);
    ctx->sql_size += written;
    
    va_end(args);
    
    CYPHER_DEBUG("SQL buffer now: %s", ctx->sql_buffer);
}

void append_identifier(cypher_transform_context *ctx, const char *name)
{
    /* SQLite uses double quotes for identifiers */
    append_sql(ctx, "\"%s\"", name);
}

void append_string_literal(cypher_transform_context *ctx, const char *value)
{
    /* SQLite uses single quotes for strings */
    /* TODO: Proper escaping */
    append_sql(ctx, "'%s'", value);
}

/* Variable management */

int register_variable(cypher_transform_context *ctx, const char *name, const char *alias)
{
    CYPHER_DEBUG("Registering variable %s with alias %s", name, alias);
    
    /* Check if variable already exists */
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            /* Update existing variable */
            free(ctx->variables[i].table_alias);
            ctx->variables[i].table_alias = strdup(alias);
            return 0;
        }
    }
    
    /* Grow array if needed */
    if (ctx->variable_count >= ctx->variable_capacity) {
        int new_capacity = ctx->variable_capacity * 2;
        void *new_vars = realloc(ctx->variables, 
                                new_capacity * sizeof(ctx->variables[0]));
        if (!new_vars) {
            return -1;
        }
        ctx->variables = new_vars;
        ctx->variable_capacity = new_capacity;
    }
    
    /* Add new variable */
    ctx->variables[ctx->variable_count].name = strdup(name);
    ctx->variables[ctx->variable_count].table_alias = strdup(alias);
    ctx->variables[ctx->variable_count].is_bound = false;
    ctx->variables[ctx->variable_count].node_id = -1;
    ctx->variable_count++;
    
    return 0;
}

const char* lookup_variable_alias(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return ctx->variables[i].table_alias;
        }
    }
    return NULL;
}

bool is_variable_bound(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return ctx->variables[i].is_bound;
        }
    }
    return false;
}

/* Main transform dispatcher */

cypher_query_result* cypher_transform_query(cypher_transform_context *ctx, cypher_query *query)
{
    CYPHER_DEBUG("Starting query transformation");
    
    if (!ctx || !query) {
        return NULL;
    }
    
    /* Reset context for new query */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';
    ctx->has_error = false;
    free(ctx->error_message);
    ctx->error_message = NULL;
    
    /* Process each clause in order */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        
        CYPHER_DEBUG("Processing clause type %s", ast_node_type_name(clause->type));
        
        switch (clause->type) {
            case AST_NODE_MATCH:
                if (transform_match_clause(ctx, (cypher_match*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_CREATE:
                if (transform_create_clause(ctx, (cypher_create*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_RETURN:
                if (transform_return_clause(ctx, (cypher_return*)clause) < 0) {
                    goto error;
                }
                break;
                
            default:
                ctx->has_error = true;
                ctx->error_message = strdup("Unsupported clause type");
                goto error;
        }
    }
    
    /* Create result structure */
    cypher_query_result *result = calloc(1, sizeof(cypher_query_result));
    if (!result) {
        goto error;
    }
    
    /* Prepare the SQL statement */
    CYPHER_DEBUG("Generated SQL: %s", ctx->sql_buffer);
    
    int rc = sqlite3_prepare_v2(ctx->db, ctx->sql_buffer, -1, &result->stmt, NULL);
    if (rc != SQLITE_OK) {
        result->has_error = true;
        result->error_message = strdup(sqlite3_errmsg(ctx->db));
        return result;
    }
    
    return result;
    
error:
    CYPHER_DEBUG("Transform error: %s", ctx->error_message ? ctx->error_message : "Unknown error");
    cypher_query_result *error_result = calloc(1, sizeof(cypher_query_result));
    if (error_result) {
        error_result->has_error = true;
        error_result->error_message = strdup(ctx->error_message ? ctx->error_message : "Transform failed");
    }
    return error_result;
}

/* Result management */

void cypher_free_result(cypher_query_result *result)
{
    if (!result) {
        return;
    }
    
    if (result->stmt) {
        sqlite3_finalize(result->stmt);
    }
    
    for (int i = 0; i < result->column_count; i++) {
        free(result->column_names[i]);
    }
    free(result->column_names);
    
    free(result->error_message);
    free(result);
}

bool cypher_result_next(cypher_query_result *result)
{
    if (!result || !result->stmt) {
        return false;
    }
    
    int rc = sqlite3_step(result->stmt);
    return rc == SQLITE_ROW;
}

const char* cypher_result_get_string(cypher_query_result *result, int column)
{
    if (!result || !result->stmt) {
        return NULL;
    }
    
    return (const char*)sqlite3_column_text(result->stmt, column);
}

int cypher_result_get_int(cypher_query_result *result, int column)
{
    if (!result || !result->stmt) {
        return 0;
    }
    
    return sqlite3_column_int(result->stmt, column);
}