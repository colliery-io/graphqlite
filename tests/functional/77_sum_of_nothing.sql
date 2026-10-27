-- ========================================================================
-- Test 77: sum() of no rows or only nulls is 0 (GQLITE-T-0455)
-- ========================================================================
-- PURPOSE: SQLite's SUM gives NULL for no values; openCypher's sum() gives
--          0. Found by the metamorphic harness (ternary partitioning of
--          count(*) / sum()).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 77: sum() of nothing ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A77 {v: 2}), (:A77 {v: 3}), (:B77)') as setup;

INSERT INTO _assert SELECT 'no rows',
    json(cypher('MATCH (n:Nope77) RETURN sum(n.v) AS s, count(*) AS c')) = json('[{"s":0,"c":0}]');
INSERT INTO _assert SELECT 'only nulls',
    json(cypher('MATCH (n:B77) RETURN sum(n.v) AS s')) = json('[{"s":0}]');
INSERT INTO _assert SELECT 'empty UNWIND',
    json(cypher('UNWIND [] AS x RETURN sum(x) AS s')) = json('[{"s":0}]');
INSERT INTO _assert SELECT 'per group',
    json(cypher('MATCH (n) WHERE n:A77 OR n:B77 RETURN labels(n)[0] AS l, sum(n.v) AS s ORDER BY l')) = json('[{"l":"A77","s":5},{"l":"B77","s":0}]');
INSERT INTO _assert SELECT 'values still summed',
    json(cypher('MATCH (n:A77) RETURN sum(n.v) AS s, sum(DISTINCT n.v) AS d')) = json('[{"s":5,"d":5}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 77 Complete ===' as test_section;
