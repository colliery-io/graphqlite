-- ========================================================================
-- Test 59: CALL { } subqueries return their rows (GQLITE-T-0432)
-- ========================================================================
-- PURPOSE: `CALL { RETURN 1 AS x } RETURN x` returned write statistics;
--          a CALL after UNWIND or WITH did not see the imported variables.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 59: CALL subquery rows ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'leading CALL then RETURN',
    json(cypher('CALL { RETURN 1 AS x } RETURN x')) = json('[{"x":1}]');
INSERT INTO _assert SELECT 'CALL with UNION then ORDER BY',
    json(cypher('CALL { RETURN 1 AS x UNION RETURN 2 AS x } RETURN x ORDER BY x DESC')) = json('[{"x":2},{"x":1}]');
INSERT INTO _assert SELECT 'UNWIND then a correlated CALL',
    json(cypher('UNWIND [1, 2] AS y CALL { WITH y RETURN y * 10 AS z } RETURN y, z ORDER BY y')) = json('[{"y":1,"z":10},{"y":2,"z":20}]');
INSERT INTO _assert SELECT 'aggregate per outer row',
    json(cypher('UNWIND [1, 2, 2] AS y CALL { WITH y UNWIND range(1, y) AS w RETURN count(*) AS c } RETURN sum(c) AS s')) = json('[{"s":5}]');
INSERT INTO _assert SELECT 'count over no rows is 0',
    json(cypher('UNWIND [1] AS y CALL { MATCH (n:None59) RETURN count(n) AS c } RETURN c')) = json('[{"c":0}]');
SELECT cypher('UNWIND [1, 2, 3] AS y CALL { WITH y CREATE (:K59 {v: y}) } RETURN count(*) AS c') as unit_call;
INSERT INTO _assert SELECT 'unit subquery writes per row',
    json_extract(cypher('MATCH (k:K59) RETURN count(k) AS c'), '$[0].c') = 3;
INSERT INTO _assert SELECT 'inner entities continue the row',
    json(cypher('WITH 3 AS y CALL { WITH y MATCH (k:K59) WHERE k.v < y RETURN k } RETURN k.v AS v ORDER BY v')) = json('[{"v":1},{"v":2}]');
INSERT INTO _assert SELECT 'no inner row drops the outer row',
    json(cypher('UNWIND [1, 5] AS y CALL { WITH y MATCH (k:K59 {v: y}) RETURN k } RETURN y')) = json('[{"y":1}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 59 Complete ===' as test_section;
