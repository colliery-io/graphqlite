-- ========================================================================
-- Test 110: a comprehension whose value reads only outer variables
--           (GQLITE-T-0480)
-- ========================================================================
-- PURPOSE: in SQLite an aggregate whose argument reads only columns of the
--          outer query belongs to the outer query. `[y IN [1] | n.v]` and
--          `[(n)-->(m) | n.v]` built json_group_array over outer columns
--          only, and folded every row into one. The value now reads a
--          column of the comprehension's own FROM.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 110: comprehension values over outer variables ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A110 {id: 1, v: 1})-[:R]->(:B110 {id: 2, v: 3}), (:C110 {id: 3, v: 7})') as setup;

INSERT INTO _assert SELECT 'list comprehension: one list per row',
    json(cypher('MATCH (n) WHERE n.id IS NOT NULL RETURN n.id AS k, [y IN [1] | n.v] AS x ORDER BY k'))
    = json('[{"k":1,"x":[1]},{"k":2,"x":[3]},{"k":3,"x":[7]}]');

INSERT INTO _assert SELECT 'list comprehension: map value',
    json(cypher('MATCH (n:A110) RETURN [y IN [1, 2] | {a: n.v}] AS x'))
    = json('[{"x":[{"a":1},{"a":1}]}]');

INSERT INTO _assert SELECT 'pattern comprehension: one list per row',
    json(cypher('MATCH (n) WHERE n.id IS NOT NULL RETURN n.id AS k, [(n)-->(m) | n.v] AS x ORDER BY k'))
    = json('[{"k":1,"x":[1]},{"k":2,"x":[]},{"k":3,"x":[]}]');

SELECT 'Test 110 passed: ' || count(*) || ' assertions' AS result FROM _assert;
