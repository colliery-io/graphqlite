/*
 * sql_builder.h
 *    Dynamic buffer and SQL builder utilities for Cypher transformation
 *
 * This module provides a reusable growing string buffer (dynamic_buffer) that
 * serves as the foundation for unified SQL generation.
 */

#ifndef SQL_BUILDER_H
#define SQL_BUILDER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/*
 * dynamic_buffer - A growing string buffer
 *
 * Manages a dynamically-sized character buffer that grows as needed.
 * All operations handle memory allocation internally.
 */
typedef struct {
    char *data;       /* Buffer contents (always null-terminated) */
    size_t len;       /* Current string length (excluding null terminator) */
    size_t capacity;  /* Allocated capacity */
} dynamic_buffer;

/* Initial buffer capacity */
#define DBUF_INITIAL_CAPACITY 256

/*
 * Initialize a dynamic buffer.
 * Must be called before any other dbuf_* functions.
 * Safe to call on zero-initialized struct.
 */
void dbuf_init(dynamic_buffer *buf);

/*
 * Free all memory associated with a dynamic buffer.
 * Safe to call multiple times or on uninitialized buffer.
 * Resets buffer to empty state.
 */
void dbuf_free(dynamic_buffer *buf);

/*
 * Clear buffer contents without freeing memory.
 * Useful for reusing a buffer.
 */
void dbuf_clear(dynamic_buffer *buf);

/*
 * Append a string to the buffer.
 * str may be NULL (no-op in that case).
 */
void dbuf_append(dynamic_buffer *buf, const char *str);

/*
 * Append a single character to the buffer.
 */
void dbuf_append_char(dynamic_buffer *buf, char c);

/*
 * Escape a string for SQL by doubling single quotes.
 * Returns a newly allocated string that must be freed by the caller.
 * Returns NULL on allocation failure or if input is NULL.
 */
char *escape_sql_string(const char *str);

/*
 * Append formatted string to the buffer (printf-style).
 * fmt may be NULL (no-op in that case).
 */
void dbuf_appendf(dynamic_buffer *buf, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/*
 * Append formatted string using va_list.
 */
void dbuf_vappendf(dynamic_buffer *buf, const char *fmt, va_list args);

/*
 * Finish building and return the owned string.
 * Returns the buffer contents and resets the buffer to empty.
 * Caller is responsible for freeing the returned string.
 * Returns NULL if buffer is empty.
 */
char *dbuf_finish(dynamic_buffer *buf);

/*
 * Get current buffer contents without consuming.
 * Returns NULL if buffer is empty.
 * Returned pointer is valid until next dbuf_* call.
 */
const char *dbuf_get(const dynamic_buffer *buf);

/*
 * Get current buffer length.
 */
size_t dbuf_len(const dynamic_buffer *buf);

/*
 * Check if buffer is empty.
 */
bool dbuf_is_empty(const dynamic_buffer *buf);

/*
 * =============================================================================
 * SQL Builder - Clause-based SQL construction
 * =============================================================================
 *
 * Builds SQL queries clause-by-clause, assembling them in correct order
 * regardless of the order functions are called.
 */

/* Join types for sql_join() */
typedef enum {
    SQL_JOIN_INNER,
    SQL_JOIN_LEFT,
    SQL_JOIN_CROSS
} sql_join_type;

/*
 * sql_builder - Structured SQL query builder
 *
 * Collects SQL clauses separately, then assembles them in correct order
 * when sql_builder_to_string() is called.
 */
typedef struct {
    dynamic_buffer cte;       /* WITH RECURSIVE ... */
    dynamic_buffer select;    /* SELECT columns */
    dynamic_buffer from;      /* FROM table */
    dynamic_buffer joins;     /* JOIN clauses */
    dynamic_buffer where;     /* WHERE conditions */
    dynamic_buffer group_by;  /* GROUP BY */
    dynamic_buffer order_by;  /* ORDER BY */
    int limit;                /* LIMIT value, -1 if not set */
    int offset;               /* OFFSET value, -1 if not set */
    int select_count;         /* Number of SELECT expressions */
    int cte_count;            /* Number of CTEs */
    int where_count;          /* Number of WHERE conditions */
    int group_count;          /* Number of GROUP BY expressions */
    int order_count;          /* Number of ORDER BY expressions */
    bool finalized;           /* True after to_string called */
    bool distinct;            /* True for SELECT DISTINCT */
} sql_builder;

/*
 * Create a new SQL builder.
 * Returns NULL on allocation failure.
 */
sql_builder *sql_builder_create(void);

/*
 * Free an SQL builder and all associated memory.
 * Safe to call with NULL.
 */
void sql_builder_free(sql_builder *b);

/*
 * Reset an SQL builder for reuse.
 * Clears all clauses but keeps allocated memory.
 */
void sql_builder_reset(sql_builder *b);

/*
 * Add a SELECT expression.
 * expr: The expression (e.g., "n.id", "COUNT(*)")
 * alias: Optional alias (e.g., "node_id"), may be NULL
 */
void sql_select(sql_builder *b, const char *expr, const char *alias);

/*
 * Set SELECT DISTINCT mode.
 */
void sql_distinct(sql_builder *b);

/*
 * Set the FROM clause.
 * table: Table name or subquery
 * alias: Table alias
 */
void sql_from(sql_builder *b, const char *table, const char *alias);

/*
 * Add a JOIN clause.
 * type: JOIN type (INNER, LEFT, CROSS)
 * table: Table name or subquery
 * alias: Table alias
 * on_condition: JOIN condition (NULL for CROSS JOIN)
 */
void sql_join(sql_builder *b, sql_join_type type, const char *table,
              const char *alias, const char *on_condition);

/*
 * Add raw JOIN SQL (for pending property JOINs from aggregate functions).
 */
void sql_join_raw(sql_builder *b, const char *raw_join_sql);

/*
 * Add a WHERE condition.
 * Multiple conditions are joined with AND.
 */
void sql_where(sql_builder *b, const char *condition);

/*
 * Add a GROUP BY expression.
 */
void sql_group_by(sql_builder *b, const char *expr);

/*
 * Add an ORDER BY expression.
 * desc: true for DESC, false for ASC
 */
void sql_order_by(sql_builder *b, const char *expr, bool desc);

/*
 * Set LIMIT and OFFSET.
 * limit: Maximum rows to return (-1 to not set)
 * offset: Rows to skip (-1 to not set)
 */
void sql_limit(sql_builder *b, int limit, int offset);

/*
 * Add a CTE (Common Table Expression).
 * name: CTE name
 * query: The CTE query body
 * recursive: true if this CTE is recursive
 */
void sql_cte(sql_builder *b, const char *name, const char *query, bool recursive);

/*
 * Build the final SQL string.
 * Assembly order: CTE -> SELECT -> FROM -> JOIN -> WHERE -> GROUP BY -> ORDER BY -> LIMIT
 * Returns owned string that caller must free.
 * Returns NULL if builder is empty or on error.
 */
char *sql_builder_to_string(sql_builder *b);

/*
 * Build a subquery (SELECT/FROM/JOIN/WHERE) WITHOUT CTEs.
 * Use this when the result will become the body of a new CTE.
 * CTEs in the builder are preserved and can be retrieved separately.
 * Returns owned string that caller must free.
 * Returns NULL if builder is empty or on error.
 */
char *sql_builder_to_subquery(sql_builder *b);

/*
 * =============================================================================
 * Builder State Extraction
 * =============================================================================
 * Functions to read builder state without generating full SQL.
 * Used by WITH/UNWIND to extract FROM/JOIN/WHERE for CTE construction.
 */

/*
 * Get the FROM clause content (table and alias).
 * Returns NULL if no FROM clause set.
 * Returned pointer is valid until builder is modified.
 */
const char *sql_builder_get_from(sql_builder *b);

/*
 * Get the JOIN clauses content.
 * Returns NULL if no JOINs added.
 */
const char *sql_builder_get_joins(sql_builder *b);

/*
 * Get the WHERE clause content (conditions only, no "WHERE" keyword).
 * Returns NULL if no WHERE conditions.
 */
const char *sql_builder_get_where(sql_builder *b);

/*
 * Get the GROUP BY clause content.
 * Returns NULL if no GROUP BY.
 */
const char *sql_builder_get_group_by(sql_builder *b);

/*
 * Check if the builder has any FROM clause content.
 */
bool sql_builder_has_from(sql_builder *b);

/*
 * =============================================================================
 * Write Builder - INSERT/UPDATE/DELETE statement construction
 * =============================================================================
 * Functions to build write statements that can integrate with sql_builder
 * for SELECT subqueries.
 */

/* Conflict resolution for INSERT statements */
typedef enum {
    SQL_INSERT_NORMAL,       /* INSERT INTO - fail on conflict */
    SQL_INSERT_OR_REPLACE,   /* INSERT OR REPLACE - update on conflict */
    SQL_INSERT_OR_IGNORE     /* INSERT OR IGNORE - skip on conflict */
} sql_insert_mode;

/*
 * write_builder - Structured SQL write statement builder
 *
 * Builds INSERT, UPDATE, DELETE statements that can include
 * SELECT subqueries from sql_builder.
 */
typedef struct {
    dynamic_buffer statement;   /* The complete statement */
    int statement_count;        /* Number of statements (for multi-statement) */
} write_builder;

/*
 * Create a new write builder.
 * Returns NULL on allocation failure.
 */
write_builder *write_builder_create(void);

/*
 * Free a write builder and all associated memory.
 * Safe to call with NULL.
 */
void write_builder_free(write_builder *wb);

/*
 * Reset a write builder for reuse.
 */
void write_builder_reset(write_builder *wb);

/*
 * Add an INSERT statement with literal VALUES.
 * table: Target table name
 * columns: Comma-separated column names (e.g., "node_id, key_id, value")
 * values: Comma-separated values (e.g., "1, 2, 'hello'")
 * mode: Conflict resolution mode
 */
void write_insert_values(write_builder *wb, sql_insert_mode mode,
                         const char *table, const char *columns, const char *values);

/*
 * Add an INSERT ... SELECT statement.
 * table: Target table name
 * columns: Comma-separated column names
 * select_sql: Complete SELECT statement to insert from
 * mode: Conflict resolution mode
 */
void write_insert_select(write_builder *wb, sql_insert_mode mode,
                         const char *table, const char *columns, const char *select_sql);

/*
 * Add a DELETE statement.
 * table: Target table name
 * where_condition: WHERE condition (without "WHERE" keyword), NULL for delete all
 */
void write_delete(write_builder *wb, const char *table, const char *where_condition);

/*
 * Add a DELETE with subquery.
 * table: Target table name
 * id_column: Column to match (e.g., "node_id")
 * subquery: SELECT statement that returns IDs to delete
 */
void write_delete_where_in(write_builder *wb, const char *table,
                           const char *id_column, const char *subquery);

/*
 * Add a raw SQL statement.
 * Useful for complex statements not covered by other functions.
 */
void write_raw(write_builder *wb, const char *sql);

/*
 * Build the final SQL string (may contain multiple semicolon-separated statements).
 * Returns owned string that caller must free.
 * Returns NULL if builder is empty.
 */
char *write_builder_to_string(write_builder *wb);

#endif /* SQL_BUILDER_H */
