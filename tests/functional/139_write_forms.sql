-- ========================================================================
-- Test 139: SET += a $map with a null value; UNWIND of any expression
--           (GQLITE-T-0508)
-- ========================================================================
-- PURPOSE: SET n += $m kept a property that m sets to null (the literal
--          map form removed it). UNWIND of a list comprehension (or a map,
--          or another expression the transform had no branch for) was
--          refused. Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 139: write forms ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:B {id: 4, v: 0})');
SELECT cypher('MATCH (a:B) SET a += $props', '{"props": {"v": null, "t": "r"}}');
INSERT INTO _assert SELECT 'SET += $map removes null keys',
    json(cypher('MATCH (a:B) RETURN properties(a) AS p')) = json('[{"p":{"id":4,"t":"r"}}]');
SELECT cypher('MATCH (a:B) SET a.v = 5');
SELECT cypher('MATCH (a:B) SET a += $props RETURN 1 AS one', '{"props": {"v": null}}');
INSERT INTO _assert SELECT 'SET += $map with RETURN removes null keys',
    json(cypher('MATCH (a:B) RETURN properties(a) AS p')) = json('[{"p":{"id":4,"t":"r"}}]');

INSERT INTO _assert SELECT 'UNWIND a list comprehension',
    json(cypher('UNWIND [x IN [1, 2, 3] WHERE x > 1 | x * 2] AS i RETURN i')) = json('[{"i":4},{"i":6}]');
INSERT INTO _assert SELECT 'UNWIND a comprehension over a node',
    json(cypher('MATCH (a:B) UNWIND [x IN [1, 2, 3] WHERE x <= a.id - 2] AS i RETURN a.id * 10 + i AS k'))
        = json('[{"k":41},{"k":42}]');
INSERT INTO _assert SELECT 'UNWIND a map',
    json(cypher('UNWIND {a: 1} AS m RETURN m')) = json('[{"m":{"a":1}}]');
SELECT cypher('MATCH (a:B) UNWIND [x IN [1, 2] WHERE x > 1] AS i CREATE (:New {k: a.id * 10 + i})');
INSERT INTO _assert SELECT 'UNWIND a comprehension before a write',
    json(cypher('MATCH (n:New) RETURN n.k AS k')) = json('[{"k":42}]');

SELECT 'Test 139 passed: ' || count(*) || ' assertions' AS result FROM _assert;
