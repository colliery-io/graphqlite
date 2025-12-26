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
    
    /* Initialize path variable tracking */
    ctx->path_variables = NULL;
    ctx->path_variable_count = 0;
    ctx->path_variable_capacity = 0;
    
    ctx->query_type = QUERY_TYPE_UNKNOWN;
    ctx->has_error = false;
    ctx->global_alias_counter = 0;
    ctx->error_message = NULL;
    ctx->in_comparison = false;

    /* Initialize parameter tracking */
    ctx->param_names = NULL;
    ctx->param_count = 0;
    ctx->param_capacity = 0;
    
    /* Initialize CTE prefix buffer (for variable-length relationships) */
    ctx->cte_prefix = NULL;
    ctx->cte_prefix_size = 0;
    ctx->cte_prefix_capacity = 0;
    ctx->cte_count = 0;

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
    ctx->sql_builder.cte_clause = NULL;
    ctx->sql_builder.cte_size = 0;
    ctx->sql_builder.cte_capacity = 0;
    ctx->sql_builder.cte_count = 0;
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
    
    /* Free path variables */
    for (int i = 0; i < ctx->path_variable_count; i++) {
        free(ctx->path_variables[i].name);
        /* Note: elements list is owned by AST, don't free it */
    }
    free(ctx->path_variables);

    /* Free parameter names */
    for (int i = 0; i < ctx->param_count; i++) {
        free(ctx->param_names[i]);
    }
    free(ctx->param_names);

    /* Free CTE prefix buffer */
    free(ctx->cte_prefix);

    /* Free SQL builder buffers */
    free(ctx->sql_builder.from_clause);
    free(ctx->sql_builder.join_clauses);
    free(ctx->sql_builder.where_clauses);
    free(ctx->sql_builder.cte_clause);

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

/* Parameter tracking */

int register_parameter(cypher_transform_context *ctx, const char *name)
{
    /* Check if parameter already registered */
    for (int i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->param_names[i], name) == 0) {
            return i;  /* Already registered, return existing index */
        }
    }

    /* Grow capacity if needed */
    if (ctx->param_count >= ctx->param_capacity) {
        int new_capacity = ctx->param_capacity == 0 ? 8 : ctx->param_capacity * 2;
        char **new_names = realloc(ctx->param_names, new_capacity * sizeof(char*));
        if (!new_names) {
            ctx->has_error = true;
            ctx->error_message = strdup("Out of memory registering parameter");
            return -1;
        }
        ctx->param_names = new_names;
        ctx->param_capacity = new_capacity;
    }

    /* Register new parameter */
    ctx->param_names[ctx->param_count] = strdup(name);
    if (!ctx->param_names[ctx->param_count]) {
        ctx->has_error = true;
        ctx->error_message = strdup("Out of memory registering parameter");
        return -1;
    }

    return ctx->param_count++;
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
    free(ctx->sql_builder.cte_clause);

    ctx->sql_builder.from_clause = NULL;
    ctx->sql_builder.from_size = 0;
    ctx->sql_builder.from_capacity = 0;
    ctx->sql_builder.join_clauses = NULL;
    ctx->sql_builder.join_size = 0;
    ctx->sql_builder.join_capacity = 0;
    ctx->sql_builder.where_clauses = NULL;
    ctx->sql_builder.where_size = 0;
    ctx->sql_builder.where_capacity = 0;
    ctx->sql_builder.cte_clause = NULL;
    ctx->sql_builder.cte_size = 0;
    ctx->sql_builder.cte_capacity = 0;
    ctx->sql_builder.cte_count = 0;
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

void append_cte_clause(cypher_transform_context *ctx, const char *format, ...)
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
    grow_builder_buffer(&ctx->sql_builder.cte_clause,
                       &ctx->sql_builder.cte_size,
                       &ctx->sql_builder.cte_capacity,
                       needed);

    /* Append to buffer */
    if (ctx->sql_builder.cte_clause) {
        int written = vsnprintf(ctx->sql_builder.cte_clause + ctx->sql_builder.cte_size,
                               ctx->sql_builder.cte_capacity - ctx->sql_builder.cte_size,
                               format, args);
        ctx->sql_builder.cte_size += written;
    }

    va_end(args);
}

int finalize_sql_generation(cypher_transform_context *ctx)
{
    if (!ctx || !ctx->sql_builder.using_builder) {
        return 0; /* Not using builder, nothing to do */
    }

    /*
     * If from_clause is empty but sql_buffer has content, the MATCH clause
     * wrote directly to sql_buffer (common path). In this case, just prepend
     * "SELECT * " to the existing content.
     */
    if ((!ctx->sql_builder.from_clause || ctx->sql_builder.from_size == 0) &&
        ctx->sql_size > 0) {
        /* Save existing content (FROM ... JOIN ... WHERE ...) */
        char *existing = strdup(ctx->sql_buffer);
        if (!existing) {
            ctx->has_error = true;
            ctx->error_message = strdup("Memory allocation failed");
            return -1;
        }

        /* Clear buffer and rebuild with SELECT * prefix */
        ctx->sql_size = 0;
        ctx->sql_buffer[0] = '\0';

        /* Add CTE clause if present */
        if (ctx->sql_builder.cte_clause && ctx->sql_builder.cte_size > 0) {
            append_sql(ctx, "%s ", ctx->sql_builder.cte_clause);
        }

        append_sql(ctx, "SELECT * %s", existing);

        /* Add any join_clauses from builder */
        if (ctx->sql_builder.join_clauses && ctx->sql_builder.join_size > 0) {
            append_sql(ctx, "%s", ctx->sql_builder.join_clauses);
        }

        /* Add WHERE clause from builder if present */
        if (ctx->sql_builder.where_clauses && ctx->sql_builder.where_size > 0) {
            append_sql(ctx, " WHERE %s", ctx->sql_builder.where_clauses);
        }

        free(existing);
        return 0;
    }

    /* Standard path: combine builder buffers */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';

    /* Add CTE clause if present (for variable-length relationships) */
    if (ctx->sql_builder.cte_clause && ctx->sql_builder.cte_size > 0) {
        append_sql(ctx, "%s ", ctx->sql_builder.cte_clause);
    }

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

/* CTE prefix functions for variable-length relationships */
/* These work with both builder and non-builder SQL generation modes */

void append_cte_prefix(cypher_transform_context *ctx, const char *format, ...)
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
    grow_builder_buffer(&ctx->cte_prefix,
                       &ctx->cte_prefix_size,
                       &ctx->cte_prefix_capacity,
                       needed);

    /* Append to buffer */
    if (ctx->cte_prefix) {
        int written = vsnprintf(ctx->cte_prefix + ctx->cte_prefix_size,
                               ctx->cte_prefix_capacity - ctx->cte_prefix_size,
                               format, args);
        ctx->cte_prefix_size += written;
    }

    va_end(args);
}

void prepend_cte_to_sql(cypher_transform_context *ctx)
{
    if (!ctx || !ctx->cte_prefix || ctx->cte_prefix_size == 0) {
        return;
    }

    CYPHER_DEBUG("Prepending CTE prefix (%zu bytes) to SQL", ctx->cte_prefix_size);

    /* Calculate new size needed */
    size_t new_size = ctx->cte_prefix_size + ctx->sql_size + 2; /* +2 for space and null */

    /* Allocate new buffer */
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
        ctx->has_error = true;
        ctx->error_message = strdup("Memory allocation failed during CTE prepend");
        return;
    }

    /* Copy CTE prefix, then space, then original SQL */
    memcpy(new_buffer, ctx->cte_prefix, ctx->cte_prefix_size);
    new_buffer[ctx->cte_prefix_size] = ' ';
    memcpy(new_buffer + ctx->cte_prefix_size + 1, ctx->sql_buffer, ctx->sql_size + 1);

    /* Replace old buffer */
    free(ctx->sql_buffer);
    ctx->sql_buffer = new_buffer;
    ctx->sql_size = ctx->cte_prefix_size + 1 + ctx->sql_size;
    ctx->sql_capacity = new_size;

    CYPHER_DEBUG("New SQL after CTE prepend: %s", ctx->sql_buffer);
}

/* Variable management */

int register_variable(cypher_transform_context *ctx, const char *name, const char *alias)
{
    CYPHER_DEBUG("Registering variable %s with alias %s", name, alias);
    
    /* Check if variable already exists */
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            /* Update existing variable */
            char *new_alias = strdup(alias);
            if (!new_alias) {
                return -1;
            }
            free(ctx->variables[i].table_alias);
            ctx->variables[i].table_alias = new_alias;
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
    char *name_copy = strdup(name);
    char *alias_copy = strdup(alias);
    if (!name_copy || !alias_copy) {
        free(name_copy);
        free(alias_copy);
        return -1;
    }
    ctx->variables[ctx->variable_count].name = name_copy;
    ctx->variables[ctx->variable_count].table_alias = alias_copy;
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
        /* Find the variable and set its type to node */
        for (int i = 0; i < ctx->variable_count; i++) {
            if (strcmp(ctx->variables[i].name, name) == 0) {
                ctx->variables[i].type = VAR_TYPE_NODE;
                break;
            }
        }
    }
    return result;
}

/* Register an edge variable */
int register_edge_variable(cypher_transform_context *ctx, const char *name, const char *alias)
{
    int result = register_variable(ctx, name, alias);
    if (result == 0) {
        /* Find the variable and set its type to edge */
        for (int i = 0; i < ctx->variable_count; i++) {
            if (strcmp(ctx->variables[i].name, name) == 0) {
                ctx->variables[i].type = VAR_TYPE_EDGE;
                break;
            }
        }
    }
    return result;
}

/* Register a projected variable (from WITH clause - value is direct, no .id needed) */
int register_projected_variable(cypher_transform_context *ctx, const char *name, const char *cte_name, const char *column_name)
{
    /* Build the full column reference: cte_name.column_name or just column_name */
    char alias[128];
    if (cte_name) {
        snprintf(alias, sizeof(alias), "%s.%s", cte_name, column_name);
    } else {
        snprintf(alias, sizeof(alias), "%s", column_name);
    }

    int result = register_variable(ctx, name, alias);
    if (result == 0) {
        /* Find the variable and set its type to projected */
        for (int i = 0; i < ctx->variable_count; i++) {
            if (strcmp(ctx->variables[i].name, name) == 0) {
                ctx->variables[i].type = VAR_TYPE_PROJECTED;
                break;
            }
        }
    }
    return result;
}

/* Register a path variable */
int register_path_variable(cypher_transform_context *ctx, const char *name, cypher_path *path)
{
    /* For path variables, we use a special alias that references the path elements */
    char alias[64];
    snprintf(alias, sizeof(alias), "_path_%s", name);

    int result = register_variable(ctx, name, alias);
    if (result == 0) {
        /* Find the variable and set its type to path */
        for (int i = 0; i < ctx->variable_count; i++) {
            if (strcmp(ctx->variables[i].name, name) == 0) {
                ctx->variables[i].type = VAR_TYPE_PATH;
                break;
            }
        }

        /* Store path variable metadata */
        if (ctx->path_variable_count >= ctx->path_variable_capacity) {
            int new_capacity = ctx->path_variable_capacity == 0 ? 4 : ctx->path_variable_capacity * 2;
            path_variable *new_path_vars = realloc(ctx->path_variables, new_capacity * sizeof(path_variable));
            if (!new_path_vars) {
                return -1;
            }
            ctx->path_variables = new_path_vars;
            ctx->path_variable_capacity = new_capacity;
        }

        char *name_copy = strdup(name);
        if (!name_copy) {
            return -1;
        }
        ctx->path_variables[ctx->path_variable_count].name = name_copy;
        ctx->path_variables[ctx->path_variable_count].elements = path->elements;
        /* Map AST path_type to transform path_type */
        switch (path->type) {
            case PATH_TYPE_SHORTEST:
                ctx->path_variables[ctx->path_variable_count].path_type = TRANSFORM_PATH_SHORTEST;
                break;
            case PATH_TYPE_ALL_SHORTEST:
                ctx->path_variables[ctx->path_variable_count].path_type = TRANSFORM_PATH_ALL_SHORTEST;
                break;
            default:
                ctx->path_variables[ctx->path_variable_count].path_type = TRANSFORM_PATH_NORMAL;
                break;
        }
        ctx->path_variables[ctx->path_variable_count].cte_name = NULL; /* Will be set during varlen CTE generation */
        ctx->path_variable_count++;
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

/* Check if a variable is a path variable */
bool is_path_variable(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return ctx->variables[i].type == VAR_TYPE_PATH;
        }
    }
    return false;
}

bool is_projected_variable(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return ctx->variables[i].type == VAR_TYPE_PROJECTED;
        }
    }
    return false;
}

/* Get path variable metadata */
path_variable* get_path_variable(cypher_transform_context *ctx, const char *name)
{
    for (int i = 0; i < ctx->path_variable_count; i++) {
        if (strcmp(ctx->path_variables[i].name, name) == 0) {
            return &ctx->path_variables[i];
        }
    }
    return NULL;
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
    char *name_copy = strdup(name);
    if (!name_copy) {
        free(alias);
        return -1;
    }
    ctx->entities[ctx->entity_count].name = name_copy;
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

/* Forward declarations for UNION support */
static int transform_single_query_sql(cypher_transform_context *ctx, cypher_query *query);
static int transform_union_sql(cypher_transform_context *ctx, cypher_union *union_node);

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

    /* Check if this is a UNION query */
    ast_node *root = (ast_node*)query;
    if (root->type == AST_NODE_UNION) {
        CYPHER_DEBUG("Processing UNION query");
        if (transform_union_sql(ctx, (cypher_union*)root) < 0) {
            goto error;
        }
        prepend_cte_to_sql(ctx);

        /* Create result structure and prepare statement */
        cypher_query_result *result = calloc(1, sizeof(cypher_query_result));
        if (!result) {
            goto error;
        }

        CYPHER_DEBUG("Generated SQL (UNION): %s", ctx->sql_buffer);
        int rc = sqlite3_prepare_v2(ctx->db, ctx->sql_buffer, -1, &result->stmt, NULL);
        if (rc != SQLITE_OK) {
            result->has_error = true;
            result->error_message = strdup(sqlite3_errmsg(ctx->db));
            return result;
        }
        return result;
    }

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

            case AST_NODE_REMOVE:
                if (transform_remove_clause(ctx, (cypher_remove*)clause) < 0) {
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

            case AST_NODE_WITH:
                /* If using SQL builder, finalize SQL generation before WITH */
                if (ctx->sql_builder.using_builder) {
                    CYPHER_DEBUG("Finalizing SQL generation before WITH clause");
                    if (finalize_sql_generation(ctx) < 0) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to finalize SQL generation");
                        goto error;
                    }
                }

                if (transform_with_clause(ctx, (cypher_with*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_UNWIND:
                /* If using SQL builder, finalize SQL generation before UNWIND */
                if (ctx->sql_builder.using_builder) {
                    CYPHER_DEBUG("Finalizing SQL generation before UNWIND clause");
                    if (finalize_sql_generation(ctx) < 0) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to finalize SQL generation");
                        goto error;
                    }
                }

                if (transform_unwind_clause(ctx, (cypher_unwind*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_FOREACH:
                if (transform_foreach_clause(ctx, (cypher_foreach*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_LOAD_CSV:
                if (transform_load_csv_clause(ctx, (cypher_load_csv*)clause) < 0) {
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

    /* Prepend CTE prefix if we have variable-length relationships */
    prepend_cte_to_sql(ctx);

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

/* Transform a UNION query to SQL */
static int transform_union_sql(cypher_transform_context *ctx, cypher_union *union_node)
{
    CYPHER_DEBUG("Transforming UNION query (all=%s)", union_node->all ? "true" : "false");

    /* Transform left side */
    if (union_node->left->type == AST_NODE_UNION) {
        if (transform_union_sql(ctx, (cypher_union*)union_node->left) < 0) {
            return -1;
        }
    } else if (union_node->left->type == AST_NODE_QUERY) {
        if (transform_single_query_sql(ctx, (cypher_query*)union_node->left) < 0) {
            return -1;
        }
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid left side of UNION");
        return -1;
    }

    /* Add UNION or UNION ALL */
    if (union_node->all) {
        append_sql(ctx, " UNION ALL ");
    } else {
        append_sql(ctx, " UNION ");
    }

    /* Transform right side - must be a single query */
    if (union_node->right->type == AST_NODE_QUERY) {
        if (transform_single_query_sql(ctx, (cypher_query*)union_node->right) < 0) {
            return -1;
        }
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid right side of UNION");
        return -1;
    }

    return 0;
}

/* Transform a single query (non-UNION) to SQL */
static int transform_single_query_sql(cypher_transform_context *ctx, cypher_query *query)
{
    CYPHER_DEBUG("Transforming single query to SQL");

    /* Check if any MATCH clause is optional */
    bool has_optional_match = false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH) {
            cypher_match *match = (cypher_match*)clause;
            if (match->optional) {
                has_optional_match = true;
                break;
            }
        }
    }

    if (has_optional_match) {
        if (init_sql_builder(ctx) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to initialize SQL builder for OPTIONAL MATCH");
            return -1;
        }
        /* Note: We don't reset sql_buffer here as we may be appending to existing UNION */
    }

    /* Process each clause in order */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];

        if (i > 0) {
            mark_entities_as_inherited(ctx);
        }

        switch (clause->type) {
            case AST_NODE_MATCH:
                if (transform_match_clause(ctx, (cypher_match*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_CREATE:
                if (transform_create_clause(ctx, (cypher_create*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_SET:
                if (transform_set_clause(ctx, (cypher_set*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_DELETE:
                if (transform_delete_clause(ctx, (cypher_delete*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_REMOVE:
                if (transform_remove_clause(ctx, (cypher_remove*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_RETURN:
                if (transform_return_clause(ctx, (cypher_return*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_WITH:
                if (transform_with_clause(ctx, (cypher_with*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_UNWIND:
                if (transform_unwind_clause(ctx, (cypher_unwind*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_FOREACH:
                if (transform_foreach_clause(ctx, (cypher_foreach*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_LOAD_CSV:
                if (transform_load_csv_clause(ctx, (cypher_load_csv*)clause) < 0) {
                    return -1;
                }
                break;

            default:
                ctx->has_error = true;
                ctx->error_message = strdup("Unsupported clause type");
                return -1;
        }
    }

    return 0;
}

/* Generate SQL only (for EXPLAIN) - does not prepare statement */
int cypher_transform_generate_sql(cypher_transform_context *ctx, cypher_query *query)
{
    CYPHER_DEBUG("Starting SQL-only query transformation (EXPLAIN)");

    if (!ctx || !query) {
        return -1;
    }

    /* Reset context for new query */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';
    ctx->has_error = false;
    free(ctx->error_message);
    ctx->error_message = NULL;
    ctx->global_alias_counter = 0;

    /* Reset entity tracking for new query */
    for (int i = 0; i < ctx->entity_count; i++) {
        free(ctx->entities[i].name);
        free(ctx->entities[i].table_alias);
    }
    ctx->entity_count = 0;

    /* Check if this is a UNION query */
    ast_node *root = (ast_node*)query;
    if (root->type == AST_NODE_UNION) {
        int result = transform_union_sql(ctx, (cypher_union*)root);
        if (result == 0) {
            prepend_cte_to_sql(ctx);
            CYPHER_DEBUG("Generated SQL (EXPLAIN UNION): %s", ctx->sql_buffer);
        }
        return result;
    }

    /* Standard single query transformation */
    if (transform_single_query_sql(ctx, query) < 0) {
        return -1;
    }

    /* Prepend CTE prefix if we have variable-length relationships */
    prepend_cte_to_sql(ctx);

    CYPHER_DEBUG("Generated SQL (EXPLAIN): %s", ctx->sql_buffer);
    return 0;
}

/* Variable-length relationship CTE generation */

/**
 * Generate a recursive CTE for variable-length relationship traversal.
 *
 * For a query like MATCH (a)-[*1..5]->(b), generates:
 *
 * WITH RECURSIVE varlen_cte_N(start_id, end_id, depth, path_ids, visited) AS (
 *     -- Base case: direct edges
 *     SELECT e.source_id, e.target_id, 1,
 *            CAST(e.source_id || ',' || e.target_id AS TEXT),
 *            ',' || e.source_id || ',' || e.target_id || ','
 *     FROM edges e
 *     WHERE e.type = 'TYPE'  -- if type specified
 *
 *     UNION ALL
 *
 *     -- Recursive case: extend paths
 *     SELECT cte.start_id, e.target_id, cte.depth + 1,
 *            cte.path_ids || ',' || e.target_id,
 *            cte.visited || e.target_id || ','
 *     FROM varlen_cte_N cte
 *     JOIN edges e ON e.source_id = cte.end_id
 *     WHERE cte.depth < max_hops
 *       AND cte.visited NOT LIKE '%,' || e.target_id || ',%'  -- cycle prevention
 *       AND e.type = 'TYPE'  -- if type specified
 * )
 */
int generate_varlen_cte(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                       const char *source_alias, const char *target_alias,
                       const char *cte_name)
{
    (void)source_alias; /* Mark as intentionally unused for now */
    (void)target_alias; /* Mark as intentionally unused for now */

    if (!ctx || !rel || !rel->varlen || !cte_name) {
        return -1;
    }

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    int min_hops = range->min_hops > 0 ? range->min_hops : 1;
    int max_hops = range->max_hops > 0 ? range->max_hops : 100; /* Default max for unbounded */

    CYPHER_DEBUG("Generating varlen CTE %s: min=%d, max=%d, type=%s",
                 cte_name, min_hops, max_hops, rel->type ? rel->type : "<any>");

    /* Build CTE header - use cte_prefix which works with both builder and non-builder modes */
    if (ctx->cte_count == 0) {
        append_cte_prefix(ctx, "WITH RECURSIVE ");
    } else {
        append_cte_prefix(ctx, ", ");
    }

    /* CTE column definitions */
    append_cte_prefix(ctx, "%s(start_id, end_id, depth, path_ids, visited) AS (", cte_name);

    /* Base case: direct edges (depth = 1) */
    /* Handle relationship direction */
    const char *src_col = "source_id";
    const char *tgt_col = "target_id";
    if (rel->left_arrow && !rel->right_arrow) {
        /* <-[*]- reversed direction */
        src_col = "target_id";
        tgt_col = "source_id";
    }

    append_cte_prefix(ctx,
        "SELECT e.%s, e.%s, 1, "
        "CAST(e.%s || ',' || e.%s AS TEXT), "
        "','|| e.%s || ',' || e.%s || ','  "
        "FROM edges e",
        src_col, tgt_col,
        src_col, tgt_col,
        src_col, tgt_col);

    /* Add type constraint if specified */
    if (rel->type) {
        append_cte_prefix(ctx, " WHERE e.type = '%s'", rel->type);
    } else if (rel->types && rel->types->count > 0) {
        append_cte_prefix(ctx, " WHERE (");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) {
                append_cte_prefix(ctx, " OR ");
            }
            cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
            append_cte_prefix(ctx, "e.type = '%s'", type_lit->value.string);
        }
        append_cte_prefix(ctx, ")");
    }

    /* Recursive case */
    append_cte_prefix(ctx, " UNION ALL ");
    append_cte_prefix(ctx,
        "SELECT cte.start_id, e.%s, cte.depth + 1, "
        "cte.path_ids || ',' || e.%s, "
        "cte.visited || e.%s || ',' "
        "FROM %s cte "
        "JOIN edges e ON e.%s = cte.end_id "
        "WHERE cte.depth < %d",
        tgt_col, tgt_col, tgt_col,
        cte_name,
        src_col,
        max_hops);

    /* Add cycle detection */
    append_cte_prefix(ctx,
        " AND cte.visited NOT LIKE '%%,' || CAST(e.%s AS TEXT) || ',%%'",
        tgt_col);

    /* Add type constraint to recursive case */
    if (rel->type) {
        append_cte_prefix(ctx, " AND e.type = '%s'", rel->type);
    } else if (rel->types && rel->types->count > 0) {
        append_cte_prefix(ctx, " AND (");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) {
                append_cte_prefix(ctx, " OR ");
            }
            cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
            append_cte_prefix(ctx, "e.type = '%s'", type_lit->value.string);
        }
        append_cte_prefix(ctx, ")");
    }

    /* Close CTE */
    append_cte_prefix(ctx, ")");

    ctx->cte_count++;

    CYPHER_DEBUG("Generated CTE prefix: %s", ctx->cte_prefix);

    return 0;
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