/*
 * sql_builder.c
 *    Dynamic buffer and SQL builder utilities for Cypher transformation
 *
 * This module provides a reusable growing string buffer (dynamic_buffer) that
 * serves as the foundation for unified SQL generation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "transform/sql_builder.h"

/*
 * Ensure buffer has at least 'needed' bytes of additional capacity.
 * Returns true on success, false on allocation failure.
 */
static bool dbuf_ensure_capacity(dynamic_buffer *buf, size_t needed)
{
    if (!buf) return false;

    size_t required = buf->len + needed + 1; /* +1 for null terminator */

    if (required <= buf->capacity) {
        return true;
    }

    /* Calculate new capacity - double until sufficient */
    size_t new_capacity = buf->capacity;
    if (new_capacity == 0) {
        new_capacity = DBUF_INITIAL_CAPACITY;
    }
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    char *new_data = realloc(buf->data, new_capacity);
    if (!new_data) {
        return false;
    }

    buf->data = new_data;
    buf->capacity = new_capacity;
    return true;
}

/*
 * Initialize a dynamic buffer.
 */
void dbuf_init(dynamic_buffer *buf)
{
    if (!buf) return;

    buf->data = NULL;
    buf->len = 0;
    buf->capacity = 0;
}

/*
 * Free all memory associated with a dynamic buffer.
 */
void dbuf_free(dynamic_buffer *buf)
{
    if (!buf) return;

    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->capacity = 0;
}

/*
 * Clear buffer contents without freeing memory.
 */
void dbuf_clear(dynamic_buffer *buf)
{
    if (!buf) return;

    buf->len = 0;
    if (buf->data && buf->capacity > 0) {
        buf->data[0] = '\0';
    }
}

/*
 * Append a string to the buffer.
 */
void dbuf_append(dynamic_buffer *buf, const char *str)
{
    if (!buf || !str) return;

    size_t str_len = strlen(str);
    if (str_len == 0) return;

    if (!dbuf_ensure_capacity(buf, str_len)) {
        return; /* Allocation failure - silently fail */
    }

    memcpy(buf->data + buf->len, str, str_len);
    buf->len += str_len;
    buf->data[buf->len] = '\0';
}

/*
 * Append a single character to the buffer.
 */
void dbuf_append_char(dynamic_buffer *buf, char c)
{
    if (!buf) return;

    if (!dbuf_ensure_capacity(buf, 1)) {
        return; /* Allocation failure - silently fail */
    }

    buf->data[buf->len] = c;
    buf->len++;
    buf->data[buf->len] = '\0';
}

/*
 * Escape a string for SQL by doubling single quotes.
 * Returns a newly allocated string that must be freed by the caller.
 * Returns NULL on allocation failure or if input is NULL.
 */
char *escape_sql_string(const char *str)
{
    if (!str) return NULL;

    /* Count single quotes to determine output size */
    size_t quote_count = 0;
    size_t len = 0;
    for (const char *p = str; *p; p++) {
        if (*p == '\'') quote_count++;
        len++;
    }

    /* Allocate output buffer: original length + extra quotes + null terminator */
    char *result = malloc(len + quote_count + 1);
    if (!result) return NULL;

    /* Copy with escaping */
    char *out = result;
    for (const char *p = str; *p; p++) {
        if (*p == '\'') {
            *out++ = '\''; /* Double the quote */
        }
        *out++ = *p;
    }
    *out = '\0';

    return result;
}

/*
 * Append formatted string using va_list.
 */
void dbuf_vappendf(dynamic_buffer *buf, const char *fmt, va_list args)
{
    if (!buf || !fmt) return;

    /* First, determine the required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) return; /* Format error */
    if (needed == 0) return; /* Nothing to append */

    if (!dbuf_ensure_capacity(buf, (size_t)needed)) {
        return; /* Allocation failure - silently fail */
    }

    /* Now format into the buffer */
    vsnprintf(buf->data + buf->len, (size_t)needed + 1, fmt, args);
    buf->len += (size_t)needed;
}

/*
 * Append formatted string to the buffer (printf-style).
 */
void dbuf_appendf(dynamic_buffer *buf, const char *fmt, ...)
{
    if (!buf || !fmt) return;

    va_list args;
    va_start(args, fmt);
    dbuf_vappendf(buf, fmt, args);
    va_end(args);
}

/*
 * Finish building and return the owned string.
 */
char *dbuf_finish(dynamic_buffer *buf)
{
    if (!buf) return NULL;

    if (buf->len == 0 || !buf->data) {
        return NULL;
    }

    /* Take ownership of the buffer data */
    char *result = buf->data;

    /* Reset buffer without freeing the data */
    buf->data = NULL;
    buf->len = 0;
    buf->capacity = 0;

    return result;
}

/*
 * Get current buffer contents without consuming.
 */
const char *dbuf_get(const dynamic_buffer *buf)
{
    if (!buf || buf->len == 0) return NULL;
    return buf->data;
}

/*
 * Get current buffer length.
 */
size_t dbuf_len(const dynamic_buffer *buf)
{
    if (!buf) return 0;
    return buf->len;
}

/*
 * Check if buffer is empty.
 */
bool dbuf_is_empty(const dynamic_buffer *buf)
{
    if (!buf) return true;
    return buf->len == 0;
}

/*
 * =============================================================================
 * SQL Builder Implementation
 * =============================================================================
 */

/*
 * Create a new SQL builder.
 */
sql_builder *sql_builder_create(void)
{
    sql_builder *b = calloc(1, sizeof(sql_builder));
    if (!b) return NULL;

    dbuf_init(&b->cte);
    dbuf_init(&b->select);
    dbuf_init(&b->from);
    dbuf_init(&b->joins);
    dbuf_init(&b->where);
    dbuf_init(&b->group_by);
    dbuf_init(&b->order_by);

    b->limit = -1;
    b->offset = -1;
    b->select_count = 0;
    b->cte_count = 0;
    b->where_count = 0;
    b->group_count = 0;
    b->order_count = 0;
    b->finalized = false;

    return b;
}

/*
 * Free an SQL builder.
 */
void sql_builder_free(sql_builder *b)
{
    if (!b) return;

    dbuf_free(&b->cte);
    dbuf_free(&b->select);
    dbuf_free(&b->from);
    dbuf_free(&b->joins);
    dbuf_free(&b->where);
    dbuf_free(&b->group_by);
    dbuf_free(&b->order_by);

    free(b);
}

/*
 * Reset an SQL builder for reuse.
 */
void sql_builder_reset(sql_builder *b)
{
    if (!b) return;

    dbuf_clear(&b->cte);
    dbuf_clear(&b->select);
    dbuf_clear(&b->from);
    dbuf_clear(&b->joins);
    dbuf_clear(&b->where);
    dbuf_clear(&b->group_by);
    dbuf_clear(&b->order_by);

    b->limit = -1;
    b->offset = -1;
    b->select_count = 0;
    b->cte_count = 0;
    b->where_count = 0;
    b->group_count = 0;
    b->order_count = 0;
    b->finalized = false;
    b->distinct = false;
}

/*
 * Add a SELECT expression.
 */
void sql_select(sql_builder *b, const char *expr, const char *alias)
{
    if (!b || !expr) return;

    if (b->select_count > 0) {
        dbuf_append(&b->select, ", ");
    }

    dbuf_append(&b->select, expr);

    if (alias && alias[0] != '\0') {
        dbuf_appendf(&b->select, " AS %s", alias);
    }

    b->select_count++;
}

/*
 * Set SELECT DISTINCT mode.
 */
void sql_distinct(sql_builder *b)
{
    if (!b) return;
    b->distinct = true;
}

/*
 * Set the FROM clause.
 */
void sql_from(sql_builder *b, const char *table, const char *alias)
{
    if (!b || !table) return;

    dbuf_clear(&b->from);
    dbuf_append(&b->from, table);

    if (alias && alias[0] != '\0') {
        dbuf_appendf(&b->from, " AS %s", alias);
    }
}

/*
 * Add raw JOIN SQL (for pending property JOINs from aggregate functions).
 */
void sql_join_raw(sql_builder *b, const char *raw_join_sql)
{
    if (!b || !raw_join_sql) return;
    dbuf_append(&b->joins, raw_join_sql);
}

/*
 * Add a JOIN clause.
 */
void sql_join(sql_builder *b, sql_join_type type, const char *table,
              const char *alias, const char *on_condition)
{
    if (!b || !table) return;

    /* Add join keyword */
    switch (type) {
        case SQL_JOIN_INNER:
            dbuf_append(&b->joins, " JOIN ");
            break;
        case SQL_JOIN_LEFT:
            dbuf_append(&b->joins, " LEFT JOIN ");
            break;
        case SQL_JOIN_CROSS:
            dbuf_append(&b->joins, " CROSS JOIN ");
            break;
    }

    /* Add table */
    dbuf_append(&b->joins, table);

    /* Add alias */
    if (alias && alias[0] != '\0') {
        dbuf_appendf(&b->joins, " AS %s", alias);
    }

    /* Add ON condition (not for CROSS JOIN) */
    if (type != SQL_JOIN_CROSS && on_condition && on_condition[0] != '\0') {
        dbuf_appendf(&b->joins, " ON %s", on_condition);
    }
}

/*
 * Add a WHERE condition.
 */
void sql_where(sql_builder *b, const char *condition)
{
    if (!b || !condition) return;

    if (b->where_count > 0) {
        dbuf_append(&b->where, " AND ");
    }

    dbuf_append(&b->where, condition);
    b->where_count++;
}

/*
 * Add a GROUP BY expression.
 */
void sql_group_by(sql_builder *b, const char *expr)
{
    if (!b || !expr) return;

    if (b->group_count > 0) {
        dbuf_append(&b->group_by, ", ");
    }

    dbuf_append(&b->group_by, expr);
    b->group_count++;
}

/*
 * Add an ORDER BY expression.
 */
void sql_order_by(sql_builder *b, const char *expr, bool desc)
{
    if (!b || !expr) return;

    if (b->order_count > 0) {
        dbuf_append(&b->order_by, ", ");
    }

    dbuf_append(&b->order_by, expr);
    if (desc) {
        dbuf_append(&b->order_by, " DESC");
    }

    b->order_count++;
}

/*
 * Set LIMIT and OFFSET.
 */
void sql_limit(sql_builder *b, int limit, int offset)
{
    if (!b) return;
    b->limit = limit;
    b->offset = offset;
}

/*
 * Add a CTE (Common Table Expression).
 */
void sql_cte(sql_builder *b, const char *name, const char *query, bool recursive)
{
    if (!b || !name || !query) return;

    if (b->cte_count == 0) {
        /* First CTE - add WITH keyword */
        if (recursive) {
            dbuf_append(&b->cte, "WITH RECURSIVE ");
        } else {
            dbuf_append(&b->cte, "WITH ");
        }
    } else {
        /* Additional CTE */
        dbuf_append(&b->cte, ", ");
    }

    dbuf_appendf(&b->cte, "%s AS (%s)", name, query);
    b->cte_count++;
}

/*
 * Build the final SQL string.
 *
 * NOTE: CTEs are NOT included here. They are handled separately by
 * prepend_cte_to_sql() which runs at the end of transformation.
 * This ensures CTEs are only added once and are preserved across
 * multiple calls to sql_builder_to_string() during clause processing.
 */
char *sql_builder_to_string(sql_builder *b)
{
    if (!b) return NULL;

    /* Need at least SELECT items or FROM clause */
    if (b->select_count == 0 && dbuf_is_empty(&b->from)) {
        return NULL;
    }

    dynamic_buffer result;
    dbuf_init(&result);

    /* NOTE: CTEs are intentionally NOT included here.
     * They are handled by prepend_cte_to_sql() at the end. */

    /* SELECT */
    if (b->distinct) {
        dbuf_append(&result, "SELECT DISTINCT ");
    } else {
        dbuf_append(&result, "SELECT ");
    }
    if (b->select_count > 0) {
        dbuf_append(&result, dbuf_get(&b->select));
    } else {
        dbuf_append(&result, "*");
    }

    /* FROM (optional for standalone SELECT like "SELECT 1 + 2") */
    if (!dbuf_is_empty(&b->from)) {
        dbuf_append(&result, " FROM ");
        dbuf_append(&result, dbuf_get(&b->from));

        /* JOINs (only valid with FROM) */
        if (!dbuf_is_empty(&b->joins)) {
            dbuf_append(&result, dbuf_get(&b->joins));
        }
    }

    /* WHERE */
    if (!dbuf_is_empty(&b->where)) {
        dbuf_append(&result, " WHERE ");
        dbuf_append(&result, dbuf_get(&b->where));
    }

    /* GROUP BY */
    if (!dbuf_is_empty(&b->group_by)) {
        dbuf_append(&result, " GROUP BY ");
        dbuf_append(&result, dbuf_get(&b->group_by));
    }

    /* ORDER BY */
    if (!dbuf_is_empty(&b->order_by)) {
        dbuf_append(&result, " ORDER BY ");
        dbuf_append(&result, dbuf_get(&b->order_by));
    }

    /* LIMIT */
    if (b->limit >= 0) {
        dbuf_appendf(&result, " LIMIT %d", b->limit);
    } else if (b->offset >= 0) {
        /* SQLite requires LIMIT before OFFSET - use -1 for unlimited */
        dbuf_append(&result, " LIMIT -1");
    }

    /* OFFSET */
    if (b->offset >= 0) {
        dbuf_appendf(&result, " OFFSET %d", b->offset);
    }

    b->finalized = true;
    return dbuf_finish(&result);
}

/*
 * Build a subquery (SELECT/FROM/JOIN/WHERE) WITHOUT CTEs.
 * Use this when the result will become the body of a new CTE.
 * CTEs are NOT included - they should be preserved in the builder
 * and merged with the parent query's CTEs later.
 */
char *sql_builder_to_subquery(sql_builder *b)
{
    if (!b) return NULL;

    /* Need at least a FROM clause */
    if (dbuf_is_empty(&b->from)) {
        return NULL;
    }

    dynamic_buffer result;
    dbuf_init(&result);

    /* NOTE: CTEs are intentionally NOT included here.
     * They will be handled separately by the caller. */

    /* SELECT */
    if (b->distinct) {
        dbuf_append(&result, "SELECT DISTINCT ");
    } else {
        dbuf_append(&result, "SELECT ");
    }
    if (b->select_count > 0) {
        dbuf_append(&result, dbuf_get(&b->select));
    } else {
        dbuf_append(&result, "*");
    }

    /* FROM */
    dbuf_append(&result, " FROM ");
    dbuf_append(&result, dbuf_get(&b->from));

    /* JOINs */
    if (!dbuf_is_empty(&b->joins)) {
        dbuf_append(&result, dbuf_get(&b->joins));
    }

    /* WHERE */
    if (!dbuf_is_empty(&b->where)) {
        dbuf_append(&result, " WHERE ");
        dbuf_append(&result, dbuf_get(&b->where));
    }

    /* GROUP BY */
    if (!dbuf_is_empty(&b->group_by)) {
        dbuf_append(&result, " GROUP BY ");
        dbuf_append(&result, dbuf_get(&b->group_by));
    }

    /* ORDER BY */
    if (!dbuf_is_empty(&b->order_by)) {
        dbuf_append(&result, " ORDER BY ");
        dbuf_append(&result, dbuf_get(&b->order_by));
    }

    /* LIMIT */
    if (b->limit >= 0) {
        dbuf_appendf(&result, " LIMIT %d", b->limit);
    } else if (b->offset >= 0) {
        dbuf_append(&result, " LIMIT -1");
    }

    /* OFFSET */
    if (b->offset >= 0) {
        dbuf_appendf(&result, " OFFSET %d", b->offset);
    }

    return dbuf_finish(&result);
}

/*
 * =============================================================================
 * Builder State Extraction Functions
 * =============================================================================
 * These functions allow reading builder state without generating full SQL.
 * Used by WITH/UNWIND to extract FROM/JOIN/WHERE for CTE construction.
 */

/*
 * Get the FROM clause content (table and alias).
 * Returns NULL if no FROM clause set.
 */
const char *sql_builder_get_from(sql_builder *b)
{
    if (!b || dbuf_is_empty(&b->from)) {
        return NULL;
    }
    return dbuf_get(&b->from);
}

/*
 * Get the JOIN clauses content.
 * Returns NULL if no JOINs added.
 */
const char *sql_builder_get_joins(sql_builder *b)
{
    if (!b || dbuf_is_empty(&b->joins)) {
        return NULL;
    }
    return dbuf_get(&b->joins);
}

/*
 * Get the WHERE clause content (conditions only, no "WHERE" keyword).
 * Returns NULL if no WHERE conditions.
 */
const char *sql_builder_get_where(sql_builder *b)
{
    if (!b || dbuf_is_empty(&b->where)) {
        return NULL;
    }
    return dbuf_get(&b->where);
}

/*
 * Get the GROUP BY clause content.
 * Returns NULL if no GROUP BY.
 */
const char *sql_builder_get_group_by(sql_builder *b)
{
    if (!b || dbuf_is_empty(&b->group_by)) {
        return NULL;
    }
    return dbuf_get(&b->group_by);
}

/*
 * Check if the builder has any FROM clause content.
 */
bool sql_builder_has_from(sql_builder *b)
{
    return b && !dbuf_is_empty(&b->from);
}
