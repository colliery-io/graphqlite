-- ========================================================================
-- Test 85: WITH ... ORDER BY ... LIMIT in a UNION branch (GQLITE-T-0458)
-- ========================================================================
-- PURPOSE: the ORDER BY of a WITH was carried onto the branch's SELECT, and
--          SQLite refuses an ORDER BY before UNION. A RETURN in a UNION
--          branch now drops a carried order (the LIMIT is applied inside
--          the WITH). A standalone RETURN after UNION found the earlier
--          branch's `(SELECT * FROM _with_0 ...)` and failed with "SELECT *
--          pattern should not occur". Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 85: UNION branch with an ordered WITH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A85 {id: 1}), (:A85 {id: 2})') as setup;

INSERT INTO _assert SELECT 'WITH * ORDER BY LIMIT, then a standalone RETURN',
    (SELECT group_concat(v) FROM (SELECT json_extract(value, '$.x') AS v FROM json_each(cypher('MATCH (a:A85) WITH * ORDER BY a.id LIMIT 1 RETURN a.id AS x UNION RETURN 5 AS x')) ORDER BY v)) = '1,5';
INSERT INTO _assert SELECT 'WITH ORDER BY DESC LIMIT on the right',
    (SELECT group_concat(v) FROM (SELECT json_extract(value, '$.x') AS v FROM json_each(cypher('RETURN 5 AS x UNION MATCH (a:A85) WITH a ORDER BY a.id DESC LIMIT 1 RETURN a.id AS x')) ORDER BY v)) = '2,5';
INSERT INTO _assert SELECT 'ordered WITH without LIMIT in UNION ALL',
    (SELECT count(*) FROM json_each(cypher('MATCH (a:A85) WITH a ORDER BY a.id DESC RETURN a.id AS x UNION ALL RETURN 5 AS x'))) = 3;
INSERT INTO _assert SELECT 'SKIP in both branches',
    (SELECT group_concat(v) FROM (SELECT json_extract(value, '$.x') AS v FROM json_each(cypher('MATCH (a:A85) WITH a ORDER BY a.id SKIP 1 RETURN a.id AS x UNION MATCH (a:A85) WITH a ORDER BY a.id DESC SKIP 1 RETURN a.id AS x')) ORDER BY v)) = '1,2';

SELECT 'Test 85 passed: ' || count(*) || ' assertions' AS result FROM _assert;
