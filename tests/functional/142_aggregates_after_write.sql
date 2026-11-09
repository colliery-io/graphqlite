-- ========================================================================
-- Test 142: aggregates in RETURN after a write (GQLITE-T-0545)
-- ========================================================================
-- PURPOSE: After a write, an aggregate inside an expression was wrong:
--          size(collect(x)) gave null per row, count(*) + 1 gave 2 for
--          two rows. With no input, collect() gave null or no row came
--          back. stDev and the percentiles gave null. Assertions are
--          hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 142: aggregates after a write ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'size(collect()) after CREATE',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:A {k: i}) RETURN size(collect(n.k)) AS a')) = json('[{"a":2}]');
INSERT INTO _assert SELECT 'count(*) + 1 after CREATE',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:B {k: i}) RETURN collect(n.k) AS a, count(*) + 1 AS c'))
        = json('[{"a":[1,2],"c":3}]');
INSERT INTO _assert SELECT 'grouped, nested',
    json(cypher('UNWIND [1, 2, 3] AS i CREATE (n:C {k: i % 2}) RETURN n.k AS k, count(*) * 10 AS c ORDER BY k'))
        = json('[{"k":0,"c":10},{"k":1,"c":20}]');
INSERT INTO _assert SELECT 'no input: collect is []',
    json(cypher('MATCH (a:Nope) CREATE (n:D {k: a.b}) RETURN collect(n.k) AS a')) = json('[{"a":[]}]');
INSERT INTO _assert SELECT 'no input: size(collect()) is 0',
    json(cypher('MATCH (a:Nope) CREATE (n:D {k: a.b}) RETURN size(collect(n.k)) AS a')) = json('[{"a":0}]');
INSERT INTO _assert SELECT 'UNWIND []: collect is []',
    json(cypher('UNWIND [] AS i CREATE (n:D {k: i}) RETURN collect(n.k) AS a')) = json('[{"a":[]}]');
INSERT INTO _assert SELECT 'stDev and percentiles after CREATE',
    json(cypher('UNWIND [1, 2, 3, 4] AS i CREATE (n:E {k: i}) RETURN round(stDev(n.k) * 1000) AS s, round(stDevP(n.k) * 1000) AS sp, percentileCont(n.k, 0.25) AS pc, percentileDisc(n.k, 0.6) AS pd'))
        = json('[{"s":1291.0,"sp":1118.0,"pc":1.75,"pd":3}]');

SELECT 'Test 142 passed: ' || count(*) || ' assertions' AS result FROM _assert;
