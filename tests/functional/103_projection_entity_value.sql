-- ========================================================================
-- Test 103: n {.*} on a node held as a value (GQLITE-T-0447)
-- ========================================================================
-- PURPOSE: `m {.*}` where m is a node or relationship held as a value (a
--          list comprehension variable) returned the whole entity object
--          ({id, labels, properties}), not its properties. A plain map value
--          still projects as itself. (Also in this change: SHOW INDEXES with
--          words after it, a procedure CALL after other clauses, unchecked
--          allocations and leaked algorithm parameters: see the Python
--          tests.)
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 103: {.*} on entity values ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:P103 {x: 1, s: ''a''})-[:R103 {w: 2}]->(:Q103 {y: 3})') as setup;

INSERT INTO _assert SELECT 'a node in a comprehension',
    json(cypher('MATCH (n:P103) RETURN [m IN [n] | m {.*}] AS p')) = json('[{"p":[{"x":1,"s":"a"}]}]');
INSERT INTO _assert SELECT 'nodes and relationships of a path',
    json(cypher('MATCH p = (:P103)-->() RETURN [x IN nodes(p) | x {.*}] AS ns, [r IN relationships(p) | r {.*}] AS rs'))
    = json('[{"ns":[{"x":1,"s":"a"},{"y":3}],"rs":[{"w":2}]}]');
INSERT INTO _assert SELECT 'with another item',
    json(cypher('MATCH (n:P103) RETURN [m IN [n] | m {.*, z: 9}] AS p')) = json('[{"p":[{"x":1,"s":"a","z":9}]}]');
INSERT INTO _assert SELECT 'a map value is itself',
    json(cypher('WITH {a: 1, properties: 2} AS m RETURN m {.*} AS p')) = json('[{"p":{"a":1,"properties":2}}]');

SELECT 'Test 103 passed: ' || count(*) || ' assertions' AS result FROM _assert;
