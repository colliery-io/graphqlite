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
    
    /* Initialize entity tracking (AGE-style) */
    ctx->entities = calloc(INITIAL_VARIABLE_CAPACITY, sizeof(ctx->entities[0]));
    if (!ctx->entities) {
        free(ctx->sql_buffer);
        free(ctx);
        return NULL;
    }
    ctx->entity_capacity = INITIAL_VARIABLE_CAPACITY;
    ctx->entity_count = 0;
    
    /* Initialize variable tracking */
    ctx->variables = calloc(INITIAL_VARIABLE_CAPACITY, sizeof(ctx->variables[0]));
    if (!ctx->variables) {
        free(ctx->entities);
        free(ctx->sql_buffer);
        free(ctx);
        return NULL;
    }
    ctx->variable_capacity = INITIAL_VARIABLE_CAPACITY;
    ctx->variable_count = 0;
    
    ctx->query_type = QUERY_TYPE_UNKNOWN;
    ctx->has_error = false;
    ctx->global_alias_counter = 0;
    ctx->error_message = NULL;
    ctx->in_comparison = false;
    
    /* Initialize SQL builder */
    ctx->sql_builder.from_clause = NULL;
    ctx->sql_builder.from_size = 0;
    ctx->sql_builder.from_capacity = 0;
    ctx->sql_builder.join_clauses = NULL;
    ctx->sql_builder.join_size = 0;
    ctx->sql_builder.join_capacity = 0;
    ctx->sql_builder.where_clauses = NULL;
    ctx->sql_builder.where_size = 0;
    ctx->sql_builder.where_capacity = 0;
    ctx->sql_builder.using_builder = false;
    
    CYPHER_DEBUG("Created transform context %p", (void*)ctx);
    
    return ctx;
}

void cypher_transform_free_context(cypher_transform_context *ctx)
{
    if (!ctx) {
        return;
    }
    
    CYPHER_DEBUG("Freeing transform context %p", (void*)ctx);
    
    /* Free entities (AGE-style) */
    for (int i = 0; i < ctx->entity_count; i++) {
        free(ctx->entities[i].name);
        free(ctx->entities[i].table_alias);
    }
    free(ctx->entities);
    
    /* Free variables */
    for (int i = 0; i < ctx->variable_count; i++) {
        free(ctx->variables[i].name);
        free(ctx->variables[i].table_alias);
    }
    free(ctx->variables);
    
    /* Free SQL builder buffers */
    free(ctx->sql_builder.from_clause);
    free(ctx->sql_builder.join_clauses);
    free(ctx->sql_builder.where_clauses);
    
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

/* SQL builder functions for two-pass generation */

#define INITIAL_BUILDER_SIZE 256

static void grow_builder_buffer(char **buffer, size_t *size, size_t *capacity, size_t needed)
{
    if (*size + needed + 1 > *capacity) {
        size_t new_capacity = *capacity == 0 ? INITIAL_BUILDER_SIZE : *capacity * 2;
        while (new_capacity < *size + needed + 1) {
            new_capacity *= 2;
        }
        
        char *new_buffer = realloc(*buffer, new_capacity);
        if (new_buffer) {
            *buffer = new_buffer;
            *capacity = new_capacity;
            if (*size == 0) {
                (*buffer)[0] = '\0';
            }
        }
    }
}

int init_sql_builder(cypher_transform_context *ctx)
{
    if (!ctx) return -1;
    
    /* Free any existing buffers */
    free_sql_builder(ctx);
    
    /* Initialize builder state */
    ctx->sql_builder.using_builder = true;
    return 0;
}

void free_sql_builder(cypher_transform_context *ctx)
{
    if (!ctx) return;
    
    free(ctx->sql_builder.from_clause);
    free(ctx->sql_builder.join_clauses);
    free(ctx->sql_builder.where_clauses);
    
    ctx->sql_builder.from_clause = NULL;
    ctx->sql_builder.from_size = 0;
    ctx->sql_builder.from_capacity = 0;
    ctx->sql_builder.join_clauses = NULL;
    ctx->sql_builder.join_size = 0;
    ctx->sql_builder.join_capacity = 0;
    ctx->sql_builder.where_clauses = NULL;
    ctx->sql_builder.where_size = 0;
    ctx->sql_builder.where_capacity = 0;
    ctx->sql_builder.using_builder = false;
}

void append_from_clause(cypher_transform_context *ctx, const char *format, ...)
{
    if (!ctx || !format) return;
    
    va_list args;
    va_start(args, format);
    
    /* Calculate required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    /* Grow buffer if needed */
    grow_builder_buffer(&ctx->sql_builder.from_clause, 
                       &ctx->sql_builder.from_size, 
                       &ctx->sql_builder.from_capacity, 
                       needed);
    
    /* Append to buffer */
    if (ctx->sql_builder.from_clause) {
        int written = vsnprintf(ctx->sql_builder.from_clause + ctx->sql_builder.from_size,
                               ctx->sql_builder.from_capacity - ctx->sql_builder.from_size,
                               format, args);
        ctx->sql_builder.from_size += written;
    }
    
    va_end(args);
}

void append_join_clause(cypher_transform_context *ctx, const char *format, ...)
{
    if (!ctx || !format) return;
    
    va_list args;
    va_start(args, format);
    
    /* Calculate required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    /* Grow buffer if needed */
    grow_builder_buffer(&ctx->sql_builder.join_clauses, 
                       &ctx->sql_builder.join_size, 
                       &ctx->sql_builder.join_capacity, 
                       needed);
    
    /* Append to buffer */
    if (ctx->sql_builder.join_clauses) {
        int written = vsnprintf(ctx->sql_builder.join_clauses + ctx->sql_builder.join_size,
                               ctx->sql_builder.join_capacity - ctx->sql_builder.join_size,
                               format, args);
        ctx->sql_builder.join_size += written;
    }
    
    va_end(args);
}

void append_where_clause(cypher_transform_context *ctx, const char *format, ...)
{
    if (!ctx || !format) return;
    
    va_list args;
    va_start(args, format);
    
    /* Calculate required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    /* Grow buffer if needed */
    grow_builder_buffer(&ctx->sql_builder.where_clauses, 
                       &ctx->sql_builder.where_size, 
                       &ctx->sql_builder.where_capacity, 
                       needed);
    
    /* Append to buffer */
    if (ctx->sql_builder.where_clauses) {
        int written = vsnprintf(ctx->sql_builder.where_clauses + ctx->sql_builder.where_size,
                               ctx->sql_builder.where_capacity - ctx->sql_builder.where_size,
                               format, args);
        ctx->sql_builder.where_size += written;
    }
    
    va_end(args);
}

int finalize_sql_generation(cypher_transform_context *ctx)
{
    if (!ctx || !ctx->sql_builder.using_builder) {
        return 0; /* Not using builder, nothing to do */
    }
    
    /* Clear existing SQL buffer */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';
    
    /* Combine: SELECT + FROM + JOINs + WHERE */
    append_sql(ctx, "SELECT * "); /* Will be replaced by RETURN clause */
    
    if (ctx->sql_builder.from_clause) {
        append_sql(ctx, "%s", ctx->sql_builder.from_clause);
    }
    
    if (ctx->sql_builder.join_clauses) {
        append_sql(ctx, "%s", ctx->sql_builder.join_clauses);
    }
    
    if (ctx->sql_builder.where_clauses) {
        append_sql(ctx, " WHERE %s", ctx->sql_builder.where_clauses);
    }
    
    return 0;
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
    ctx->variables[ctx->variable_count].type = VAR_TYPE_NODE; /* Default to node */
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

/* Register a node variable */
int register_node_variable(cypher_transform_context *ctx, const char *name, const char *alias)
{
    int result = register_variable(ctx, name, alias);
    if (result == 0) {
        /* Variable was just added, it's the last one */
        ctx->variables[ctx->variable_count - 1].type = VAR_TYPE_NODE;
    }
    return result;
}

/* Register an edge variable */
int register_edge_variable(cypher_transform_context *ctx, const char *name, const char *alias)
{
    int result = register_variable(ctx, name, alias);
    if (result == 0) {
        /* Variable was just added, it's the last one */
        ctx->variables[ctx->variable_count - 1].type = VAR_TYPE_EDGE;
    }
    return result;
}

/* Check if a variable is an edge variable */
bool is_edge_variable(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return ctx->variables[i].type == VAR_TYPE_EDGE;
        }
    }
    return false;
}

/* Entity management (AGE-style) */

/* Add a new entity (following AGE's add_entity pattern) */
int add_entity(cypher_transform_context *ctx, const char *name, entity_type type, bool is_current_clause)
{
    CYPHER_DEBUG("Adding entity %s, type %d, current_clause %d", name, type, is_current_clause);
    
    /* Check if entity already exists */
    for (int i = 0; i < ctx->entity_count; i++) {
        if (strcmp(ctx->entities[i].name, name) == 0) {
            /* Entity already exists, update its current clause status */
            ctx->entities[i].is_current_clause = is_current_clause;
            return 0;
        }
    }
    
    /* Grow array if needed */
    if (ctx->entity_count >= ctx->entity_capacity) {
        int new_capacity = ctx->entity_capacity * 2;
        void *new_entities = realloc(ctx->entities, 
                                   new_capacity * sizeof(ctx->entities[0]));
        if (!new_entities) {
            return -1;
        }
        ctx->entities = new_entities;
        ctx->entity_capacity = new_capacity;
    }
    
    /* Generate default alias using AGE's pattern */
    char *alias = get_next_default_alias(ctx);
    if (!alias) {
        return -1;
    }
    
    /* Add new entity */
    ctx->entities[ctx->entity_count].name = strdup(name);
    ctx->entities[ctx->entity_count].table_alias = alias;
    ctx->entities[ctx->entity_count].type = type;
    ctx->entities[ctx->entity_count].is_current_clause = is_current_clause;
    ctx->entity_count++;
    
    return 0;
}

/* Lookup an entity by name (following AGE's lookup pattern) */
transform_entity* lookup_entity(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->entity_count; i++) {
        if (strcmp(ctx->entities[i].name, name) == 0) {
            return &ctx->entities[i];
        }
    }
    return NULL;
}

/* Mark all current clause entities as inherited for next clause */
void mark_entities_as_inherited(cypher_transform_context *ctx)
{
    for (int i = 0; i < ctx->entity_count; i++) {
        ctx->entities[i].is_current_clause = false;
    }
}

/* Generate next unique alias (AGE's pattern with default prefix) */
char* get_next_default_alias(cypher_transform_context *ctx)
{
    char *alias = malloc(64);
    if (!alias) {
        return NULL;
    }
    snprintf(alias, 64, "_gql_default_alias_%d", ctx->global_alias_counter++);
    return alias;
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
    ctx->global_alias_counter = 0;
    
    /* Reset entity tracking for new query (AGE-style) */
    for (int i = 0; i < ctx->entity_count; i++) {
        free(ctx->entities[i].name);
        free(ctx->entities[i].table_alias);
    }
    ctx->entity_count = 0;
    
    /* Check if any MATCH clause is optional - if so, use SQL builder from start */
    bool has_optional_match = false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH) {
            cypher_match *match = (cypher_match*)clause;
            CYPHER_DEBUG("Found MATCH clause %d, optional = %s", i, match->optional ? "true" : "false");
            if (match->optional) {
                has_optional_match = true;
                break;
            }
        }
    }
    CYPHER_DEBUG("Query analysis complete: has_optional_match = %s", has_optional_match ? "true" : "false");
    
    if (has_optional_match) {
        CYPHER_DEBUG("Query contains OPTIONAL MATCH - using SQL builder from start");
        if (init_sql_builder(ctx) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to initialize SQL builder for OPTIONAL MATCH");
            goto error;
        }
        /* Ensure SQL buffer starts clean for builder mode */
        ctx->sql_size = 0;
        ctx->sql_buffer[0] = '\0';
    }
    
    /* Process each clause in order */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        
        /* Mark entities from previous clause as inherited (AGE pattern) */
        if (i > 0) {
            mark_entities_as_inherited(ctx);
        }
        
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
                
            case AST_NODE_SET:
                if (transform_set_clause(ctx, (cypher_set*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_DELETE:
                if (transform_delete_clause(ctx, (cypher_delete*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_RETURN:
                /* If using SQL builder, finalize SQL generation before RETURN */
                if (ctx->sql_builder.using_builder) {
                    CYPHER_DEBUG("Finalizing SQL generation before RETURN clause");
                    if (finalize_sql_generation(ctx) < 0) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to finalize SQL generation");
                        goto error;
                    }
                } else {
                    CYPHER_DEBUG("SQL builder NOT active before RETURN clause");
                }
                
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