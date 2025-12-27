/*
 * test_sql_builder.c
 *    Unit tests for the dynamic_buffer utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "transform/sql_builder.h"

/* Test init and free */
static void test_dbuf_init_free(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    CU_ASSERT_PTR_NULL(buf.data);
    CU_ASSERT_EQUAL(buf.len, 0);
    CU_ASSERT_EQUAL(buf.capacity, 0);

    dbuf_free(&buf);
    CU_ASSERT_PTR_NULL(buf.data);
    CU_ASSERT_EQUAL(buf.len, 0);
    CU_ASSERT_EQUAL(buf.capacity, 0);
}

/* Test free on NULL is safe */
static void test_dbuf_free_null(void)
{
    dbuf_free(NULL);  /* Should not crash */
}

/* Test simple append */
static void test_dbuf_append_simple(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append(&buf, "hello");
    CU_ASSERT_EQUAL(buf.len, 5);
    CU_ASSERT_STRING_EQUAL(buf.data, "hello");

    dbuf_append(&buf, " world");
    CU_ASSERT_EQUAL(buf.len, 11);
    CU_ASSERT_STRING_EQUAL(buf.data, "hello world");

    dbuf_free(&buf);
}

/* Test append NULL is safe */
static void test_dbuf_append_null(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append(&buf, NULL);  /* Should not crash */
    CU_ASSERT_TRUE(dbuf_is_empty(&buf));

    dbuf_free(&buf);
}

/* Test append empty string */
static void test_dbuf_append_empty(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append(&buf, "");
    CU_ASSERT_TRUE(dbuf_is_empty(&buf));

    dbuf_append(&buf, "test");
    CU_ASSERT_EQUAL(buf.len, 4);

    dbuf_append(&buf, "");
    CU_ASSERT_EQUAL(buf.len, 4);

    dbuf_free(&buf);
}

/* Test append_char */
static void test_dbuf_append_char(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append_char(&buf, 'a');
    dbuf_append_char(&buf, 'b');
    dbuf_append_char(&buf, 'c');

    CU_ASSERT_EQUAL(buf.len, 3);
    CU_ASSERT_STRING_EQUAL(buf.data, "abc");

    dbuf_free(&buf);
}

/* Test formatted append */
static void test_dbuf_appendf_format(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_appendf(&buf, "SELECT * FROM %s WHERE id = %d", "nodes", 42);
    CU_ASSERT_STRING_EQUAL(buf.data, "SELECT * FROM nodes WHERE id = 42");

    dbuf_free(&buf);
}

/* Test formatted append NULL is safe */
static void test_dbuf_appendf_null(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_appendf(&buf, NULL);  /* Should not crash */
    CU_ASSERT_TRUE(dbuf_is_empty(&buf));

    dbuf_free(&buf);
}

/* Test growing beyond initial capacity */
static void test_dbuf_grow_large_string(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    /* Build a string larger than initial capacity (256) */
    for (int i = 0; i < 50; i++) {
        dbuf_append(&buf, "hello world ");
    }

    CU_ASSERT_EQUAL(buf.len, 50 * 12);  /* 12 chars per iteration */
    CU_ASSERT_TRUE(buf.capacity > 256);

    dbuf_free(&buf);
}

/* Test clear and reuse */
static void test_dbuf_clear_reuse(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append(&buf, "first content");
    CU_ASSERT_EQUAL(buf.len, 13);
    size_t old_capacity = buf.capacity;

    dbuf_clear(&buf);
    CU_ASSERT_EQUAL(buf.len, 0);
    CU_ASSERT_EQUAL(buf.capacity, old_capacity);  /* Capacity preserved */

    dbuf_append(&buf, "second");
    CU_ASSERT_EQUAL(buf.len, 6);
    CU_ASSERT_STRING_EQUAL(buf.data, "second");

    dbuf_free(&buf);
}

/* Test finish returns string and resets */
static void test_dbuf_finish(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append(&buf, "result");
    char *result = dbuf_finish(&buf);

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_STRING_EQUAL(result, "result");

    /* Buffer should be reset */
    CU_ASSERT_PTR_NULL(buf.data);
    CU_ASSERT_EQUAL(buf.len, 0);
    CU_ASSERT_EQUAL(buf.capacity, 0);

    free(result);
}

/* Test finish on empty buffer */
static void test_dbuf_finish_empty(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    char *result = dbuf_finish(&buf);
    CU_ASSERT_PTR_NULL(result);  /* Should return NULL for empty */
}

/* Test dbuf_get */
static void test_dbuf_get(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    CU_ASSERT_PTR_NULL(dbuf_get(&buf));  /* Empty buffer */

    dbuf_append(&buf, "content");
    const char *str = dbuf_get(&buf);
    CU_ASSERT_PTR_NOT_NULL(str);
    CU_ASSERT_STRING_EQUAL(str, "content");

    /* Buffer still contains data after get */
    CU_ASSERT_EQUAL(buf.len, 7);

    dbuf_free(&buf);
}

/* Test dbuf_len */
static void test_dbuf_len(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    CU_ASSERT_EQUAL(dbuf_len(&buf), 0);
    CU_ASSERT_EQUAL(dbuf_len(NULL), 0);

    dbuf_append(&buf, "test");
    CU_ASSERT_EQUAL(dbuf_len(&buf), 4);

    dbuf_free(&buf);
}

/* Test dbuf_is_empty */
static void test_dbuf_is_empty(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    CU_ASSERT_TRUE(dbuf_is_empty(&buf));
    CU_ASSERT_TRUE(dbuf_is_empty(NULL));

    dbuf_append(&buf, "x");
    CU_ASSERT_FALSE(dbuf_is_empty(&buf));

    dbuf_clear(&buf);
    CU_ASSERT_TRUE(dbuf_is_empty(&buf));

    dbuf_free(&buf);
}

/* Test multiple format specifiers */
static void test_dbuf_appendf_multiple_specs(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_appendf(&buf, "%s.%s AS %s", "n", "name", "person_name");
    CU_ASSERT_STRING_EQUAL(buf.data, "n.name AS person_name");

    dbuf_free(&buf);
}

/* Test building SQL incrementally */
static void test_dbuf_sql_building(void)
{
    dynamic_buffer buf;
    dbuf_init(&buf);

    dbuf_append(&buf, "SELECT ");
    dbuf_appendf(&buf, "%s.id", "n");
    dbuf_append(&buf, " FROM nodes AS ");
    dbuf_appendf(&buf, "%s", "n");
    dbuf_append(&buf, " WHERE ");
    dbuf_appendf(&buf, "%s.label = '%s'", "n", "Person");

    CU_ASSERT_STRING_EQUAL(buf.data,
        "SELECT n.id FROM nodes AS n WHERE n.label = 'Person'");

    dbuf_free(&buf);
}

/*
 * =============================================================================
 * SQL Builder Tests
 * =============================================================================
 */

/* Test sql_builder create and free */
static void test_sql_builder_create_free(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        CU_ASSERT_EQUAL(b->limit, -1);
        CU_ASSERT_EQUAL(b->offset, -1);
        CU_ASSERT_EQUAL(b->select_count, 0);
        CU_ASSERT_FALSE(b->finalized);
        sql_builder_free(b);
    }

    /* Free NULL is safe */
    sql_builder_free(NULL);
}

/* Test simple SELECT FROM */
static void test_sql_builder_simple_select(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.id", NULL);
        sql_from(b, "nodes", "n");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql, "SELECT n.id FROM nodes AS n");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test SELECT with alias */
static void test_sql_builder_select_alias(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.id", "node_id");
        sql_select(b, "n.name", "node_name");
        sql_from(b, "nodes", "n");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql,
                "SELECT n.id AS node_id, n.name AS node_name FROM nodes AS n");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test WHERE clause */
static void test_sql_builder_where(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "*", NULL);
        sql_from(b, "nodes", "n");
        sql_where(b, "n.label = 'Person'");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql,
                "SELECT * FROM nodes AS n WHERE n.label = 'Person'");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test multiple WHERE conditions */
static void test_sql_builder_where_multiple(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "*", NULL);
        sql_from(b, "nodes", "n");
        sql_where(b, "n.label = 'Person'");
        sql_where(b, "n.age > 18");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql,
                "SELECT * FROM nodes AS n WHERE n.label = 'Person' AND n.age > 18");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test JOIN clause */
static void test_sql_builder_join(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.id", NULL);
        sql_select(b, "e.type", NULL);
        sql_from(b, "nodes", "n");
        sql_join(b, SQL_JOIN_INNER, "edges", "e", "e.source_id = n.id");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql,
                "SELECT n.id, e.type FROM nodes AS n JOIN edges AS e ON e.source_id = n.id");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test LEFT JOIN */
static void test_sql_builder_left_join(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.id", NULL);
        sql_from(b, "nodes", "n");
        sql_join(b, SQL_JOIN_LEFT, "edges", "e", "e.source_id = n.id");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT(strstr(sql, "LEFT JOIN edges") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test ORDER BY */
static void test_sql_builder_order_by(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.name", NULL);
        sql_from(b, "nodes", "n");
        sql_order_by(b, "n.name", false);

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql,
                "SELECT n.name FROM nodes AS n ORDER BY n.name");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test ORDER BY DESC */
static void test_sql_builder_order_by_desc(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.age", NULL);
        sql_from(b, "nodes", "n");
        sql_order_by(b, "n.age", true);

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT(strstr(sql, "ORDER BY n.age DESC") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test LIMIT and OFFSET */
static void test_sql_builder_limit_offset(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "*", NULL);
        sql_from(b, "nodes", "n");
        sql_limit(b, 10, 5);

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT(strstr(sql, "LIMIT 10") != NULL);
            CU_ASSERT(strstr(sql, "OFFSET 5") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test GROUP BY */
static void test_sql_builder_group_by(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.label", NULL);
        sql_select(b, "COUNT(*)", "cnt");
        sql_from(b, "nodes", "n");
        sql_group_by(b, "n.label");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT(strstr(sql, "GROUP BY n.label") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test CTE */
static void test_sql_builder_cte(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_cte(b, "friends", "SELECT target_id FROM edges WHERE type = 'KNOWS'", false);
        sql_select(b, "n.name", NULL);
        sql_from(b, "nodes", "n");
        sql_where(b, "n.id IN (SELECT target_id FROM friends)");

        /* CTEs are stored in the cte buffer, not included in sql_builder_to_string() output.
         * They are prepended later by prepend_cte_to_sql() in the transform layer. */
        CU_ASSERT(!dbuf_is_empty(&b->cte));
        const char *cte_content = dbuf_get(&b->cte);
        CU_ASSERT(cte_content != NULL);
        if (cte_content) {
            CU_ASSERT(strstr(cte_content, "WITH friends AS") != NULL);
        }

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            /* Verify the main query is correct */
            CU_ASSERT(strstr(sql, "SELECT n.name") != NULL);
            CU_ASSERT(strstr(sql, "FROM nodes") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test recursive CTE */
static void test_sql_builder_cte_recursive(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_cte(b, "paths", "SELECT 1 UNION ALL SELECT n+1 FROM paths WHERE n < 10", true);
        sql_select(b, "*", NULL);
        sql_from(b, "paths", NULL);

        /* CTEs are stored in the cte buffer, not included in sql_builder_to_string() output. */
        CU_ASSERT(!dbuf_is_empty(&b->cte));
        const char *cte_content = dbuf_get(&b->cte);
        CU_ASSERT(cte_content != NULL);
        if (cte_content) {
            CU_ASSERT(strstr(cte_content, "WITH RECURSIVE paths AS") != NULL);
        }

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            /* Verify the main query is correct */
            CU_ASSERT(strstr(sql, "SELECT *") != NULL);
            CU_ASSERT(strstr(sql, "FROM paths") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test reset */
static void test_sql_builder_reset(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.id", NULL);
        sql_from(b, "nodes", "n");
        CU_ASSERT_EQUAL(b->select_count, 1);

        sql_builder_reset(b);
        CU_ASSERT_EQUAL(b->select_count, 0);
        CU_ASSERT_EQUAL(b->limit, -1);

        /* After reset, should be able to build new query */
        sql_select(b, "e.type", NULL);
        sql_from(b, "edges", "e");

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT_STRING_EQUAL(sql, "SELECT e.type FROM edges AS e");
            free(sql);
        }
        sql_builder_free(b);
    }
}

/* Test empty builder returns NULL */
static void test_sql_builder_empty_returns_null(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NULL(sql);
        sql_builder_free(b);
    }
}

/* Test complex query */
static void test_sql_builder_complex(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        sql_select(b, "n.id", "node_id");
        sql_select(b, "m.name", "friend_name");
        sql_from(b, "nodes", "n");
        sql_join(b, SQL_JOIN_INNER, "edges", "e", "e.source_id = n.id");
        sql_join(b, SQL_JOIN_INNER, "nodes", "m", "m.id = e.target_id");
        sql_where(b, "n.label = 'Person'");
        sql_where(b, "e.type = 'KNOWS'");
        sql_order_by(b, "m.name", false);
        sql_limit(b, 10, -1);

        char *sql = sql_builder_to_string(b);
        CU_ASSERT_PTR_NOT_NULL(sql);
        if (sql) {
            CU_ASSERT(strstr(sql, "SELECT n.id AS node_id") != NULL);
            CU_ASSERT(strstr(sql, "JOIN edges") != NULL);
            CU_ASSERT(strstr(sql, "WHERE") != NULL);
            CU_ASSERT(strstr(sql, "ORDER BY m.name") != NULL);
            CU_ASSERT(strstr(sql, "LIMIT 10") != NULL);
            free(sql);
        }
        sql_builder_free(b);
    }
}

/*
 * =============================================================================
 * Builder State Extraction Tests
 * =============================================================================
 */

/* Test sql_builder_get_from */
static void test_sql_builder_get_from(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder returns NULL */
        CU_ASSERT_PTR_NULL(sql_builder_get_from(b));

        sql_from(b, "nodes", "n");
        const char *from = sql_builder_get_from(b);
        CU_ASSERT_PTR_NOT_NULL(from);
        if (from) {
            CU_ASSERT_STRING_EQUAL(from, "nodes AS n");
        }

        sql_builder_free(b);
    }

    /* NULL builder returns NULL */
    CU_ASSERT_PTR_NULL(sql_builder_get_from(NULL));
}

/* Test sql_builder_get_joins */
static void test_sql_builder_get_joins(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder returns NULL */
        CU_ASSERT_PTR_NULL(sql_builder_get_joins(b));

        sql_join(b, SQL_JOIN_INNER, "edges", "e", "e.source_id = n.id");
        const char *joins = sql_builder_get_joins(b);
        CU_ASSERT_PTR_NOT_NULL(joins);
        if (joins) {
            CU_ASSERT(strstr(joins, "JOIN edges") != NULL);
            CU_ASSERT(strstr(joins, "e.source_id = n.id") != NULL);
        }

        /* Add another join */
        sql_join(b, SQL_JOIN_LEFT, "nodes", "m", "m.id = e.target_id");
        joins = sql_builder_get_joins(b);
        CU_ASSERT_PTR_NOT_NULL(joins);
        if (joins) {
            CU_ASSERT(strstr(joins, "LEFT JOIN nodes") != NULL);
        }

        sql_builder_free(b);
    }

    /* NULL builder returns NULL */
    CU_ASSERT_PTR_NULL(sql_builder_get_joins(NULL));
}

/* Test sql_builder_get_where */
static void test_sql_builder_get_where(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder returns NULL */
        CU_ASSERT_PTR_NULL(sql_builder_get_where(b));

        sql_where(b, "n.label = 'Person'");
        const char *where = sql_builder_get_where(b);
        CU_ASSERT_PTR_NOT_NULL(where);
        if (where) {
            CU_ASSERT_STRING_EQUAL(where, "n.label = 'Person'");
        }

        /* Add another condition */
        sql_where(b, "n.age > 18");
        where = sql_builder_get_where(b);
        CU_ASSERT_PTR_NOT_NULL(where);
        if (where) {
            CU_ASSERT(strstr(where, "n.label = 'Person'") != NULL);
            CU_ASSERT(strstr(where, "AND") != NULL);
            CU_ASSERT(strstr(where, "n.age > 18") != NULL);
        }

        sql_builder_free(b);
    }

    /* NULL builder returns NULL */
    CU_ASSERT_PTR_NULL(sql_builder_get_where(NULL));
}

/* Test sql_builder_get_group_by */
static void test_sql_builder_get_group_by(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder returns NULL */
        CU_ASSERT_PTR_NULL(sql_builder_get_group_by(b));

        sql_group_by(b, "n.label");
        const char *group = sql_builder_get_group_by(b);
        CU_ASSERT_PTR_NOT_NULL(group);
        if (group) {
            CU_ASSERT_STRING_EQUAL(group, "n.label");
        }

        /* Add another group by */
        sql_group_by(b, "n.name");
        group = sql_builder_get_group_by(b);
        CU_ASSERT_PTR_NOT_NULL(group);
        if (group) {
            CU_ASSERT(strstr(group, "n.label") != NULL);
            CU_ASSERT(strstr(group, "n.name") != NULL);
        }

        sql_builder_free(b);
    }

    /* NULL builder returns NULL */
    CU_ASSERT_PTR_NULL(sql_builder_get_group_by(NULL));
}

/* Test sql_builder_get_select */
static void test_sql_builder_get_select(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder returns NULL */
        CU_ASSERT_PTR_NULL(sql_builder_get_select(b));

        sql_select(b, "n.id", "node_id");
        const char *sel = sql_builder_get_select(b);
        CU_ASSERT_PTR_NOT_NULL(sel);
        if (sel) {
            CU_ASSERT_STRING_EQUAL(sel, "n.id AS node_id");
        }

        /* Add another select */
        sql_select(b, "n.name", NULL);
        sel = sql_builder_get_select(b);
        CU_ASSERT_PTR_NOT_NULL(sel);
        if (sel) {
            CU_ASSERT(strstr(sel, "n.id AS node_id") != NULL);
            CU_ASSERT(strstr(sel, "n.name") != NULL);
        }

        sql_builder_free(b);
    }

    /* NULL builder returns NULL */
    CU_ASSERT_PTR_NULL(sql_builder_get_select(NULL));
}

/* Test sql_builder_has_from */
static void test_sql_builder_has_from(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder */
        CU_ASSERT_FALSE(sql_builder_has_from(b));

        sql_from(b, "nodes", "n");
        CU_ASSERT_TRUE(sql_builder_has_from(b));

        sql_builder_reset(b);
        CU_ASSERT_FALSE(sql_builder_has_from(b));

        sql_builder_free(b);
    }

    /* NULL builder returns false */
    CU_ASSERT_FALSE(sql_builder_has_from(NULL));
}

/* Test sql_builder_has_select */
static void test_sql_builder_has_select(void)
{
    sql_builder *b = sql_builder_create();
    CU_ASSERT_PTR_NOT_NULL(b);

    if (b) {
        /* Empty builder */
        CU_ASSERT_FALSE(sql_builder_has_select(b));

        sql_select(b, "n.id", NULL);
        CU_ASSERT_TRUE(sql_builder_has_select(b));

        sql_builder_reset(b);
        CU_ASSERT_FALSE(sql_builder_has_select(b));

        sql_builder_free(b);
    }

    /* NULL builder returns false */
    CU_ASSERT_FALSE(sql_builder_has_select(NULL));
}

/* Register test suite */
int init_sql_builder_suite(void)
{
    CU_pSuite suite = CU_add_suite("SQL Builder", NULL, NULL);
    if (!suite) return -1;

    /* dynamic_buffer tests */
    if (!CU_add_test(suite, "dbuf: Init and free", test_dbuf_init_free)) return -1;
    if (!CU_add_test(suite, "dbuf: Free NULL safe", test_dbuf_free_null)) return -1;
    if (!CU_add_test(suite, "dbuf: Append simple", test_dbuf_append_simple)) return -1;
    if (!CU_add_test(suite, "dbuf: Append NULL safe", test_dbuf_append_null)) return -1;
    if (!CU_add_test(suite, "dbuf: Append empty", test_dbuf_append_empty)) return -1;
    if (!CU_add_test(suite, "dbuf: Append char", test_dbuf_append_char)) return -1;
    if (!CU_add_test(suite, "dbuf: Appendf format", test_dbuf_appendf_format)) return -1;
    if (!CU_add_test(suite, "dbuf: Appendf NULL safe", test_dbuf_appendf_null)) return -1;
    if (!CU_add_test(suite, "dbuf: Grow large string", test_dbuf_grow_large_string)) return -1;
    if (!CU_add_test(suite, "dbuf: Clear and reuse", test_dbuf_clear_reuse)) return -1;
    if (!CU_add_test(suite, "dbuf: Finish", test_dbuf_finish)) return -1;
    if (!CU_add_test(suite, "dbuf: Finish empty", test_dbuf_finish_empty)) return -1;
    if (!CU_add_test(suite, "dbuf: Get", test_dbuf_get)) return -1;
    if (!CU_add_test(suite, "dbuf: Len", test_dbuf_len)) return -1;
    if (!CU_add_test(suite, "dbuf: Is empty", test_dbuf_is_empty)) return -1;
    if (!CU_add_test(suite, "dbuf: Appendf multiple specs", test_dbuf_appendf_multiple_specs)) return -1;
    if (!CU_add_test(suite, "dbuf: SQL building", test_dbuf_sql_building)) return -1;

    /* sql_builder tests */
    if (!CU_add_test(suite, "sql: Create and free", test_sql_builder_create_free)) return -1;
    if (!CU_add_test(suite, "sql: Simple SELECT", test_sql_builder_simple_select)) return -1;
    if (!CU_add_test(suite, "sql: SELECT with alias", test_sql_builder_select_alias)) return -1;
    if (!CU_add_test(suite, "sql: WHERE clause", test_sql_builder_where)) return -1;
    if (!CU_add_test(suite, "sql: WHERE multiple", test_sql_builder_where_multiple)) return -1;
    if (!CU_add_test(suite, "sql: JOIN clause", test_sql_builder_join)) return -1;
    if (!CU_add_test(suite, "sql: LEFT JOIN", test_sql_builder_left_join)) return -1;
    if (!CU_add_test(suite, "sql: ORDER BY", test_sql_builder_order_by)) return -1;
    if (!CU_add_test(suite, "sql: ORDER BY DESC", test_sql_builder_order_by_desc)) return -1;
    if (!CU_add_test(suite, "sql: LIMIT OFFSET", test_sql_builder_limit_offset)) return -1;
    if (!CU_add_test(suite, "sql: GROUP BY", test_sql_builder_group_by)) return -1;
    if (!CU_add_test(suite, "sql: CTE", test_sql_builder_cte)) return -1;
    if (!CU_add_test(suite, "sql: CTE recursive", test_sql_builder_cte_recursive)) return -1;
    if (!CU_add_test(suite, "sql: Reset", test_sql_builder_reset)) return -1;
    if (!CU_add_test(suite, "sql: Empty returns NULL", test_sql_builder_empty_returns_null)) return -1;
    if (!CU_add_test(suite, "sql: Complex query", test_sql_builder_complex)) return -1;

    /* Builder state extraction tests */
    if (!CU_add_test(suite, "sql: Get FROM", test_sql_builder_get_from)) return -1;
    if (!CU_add_test(suite, "sql: Get JOINs", test_sql_builder_get_joins)) return -1;
    if (!CU_add_test(suite, "sql: Get WHERE", test_sql_builder_get_where)) return -1;
    if (!CU_add_test(suite, "sql: Get GROUP BY", test_sql_builder_get_group_by)) return -1;
    if (!CU_add_test(suite, "sql: Get SELECT", test_sql_builder_get_select)) return -1;
    if (!CU_add_test(suite, "sql: Has FROM", test_sql_builder_has_from)) return -1;
    if (!CU_add_test(suite, "sql: Has SELECT", test_sql_builder_has_select)) return -1;

    return 0;
}
