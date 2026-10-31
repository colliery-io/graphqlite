-- ========================================================================
-- Test 104: unique checks on a write inside another statement
--           (GQLITE-T-0445)
-- ========================================================================
-- PURPOSE: a write run by cypher() inside another SQL statement cannot open
--          a savepoint, and its constraint check was skipped. The check now
--          runs there too (a violation is reported; it cannot be undone).
--          A check that cannot run (a missing table) now fails the write:
--          see bindings/python/tests/test_schema_objects.py.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 104: nested write under a unique key ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));
CREATE TEMP TABLE _w(r TEXT);

SELECT cypher('CREATE CONSTRAINT u104 FOR (p:P104) REQUIRE p.id IS UNIQUE') as setup;
INSERT INTO _w SELECT cypher('CREATE (:P104 {id: 1})');
INSERT INTO _w SELECT cypher('CREATE (:P104 {id: 2})');
INSERT INTO _assert SELECT 'nested writes that meet the constraint are kept',
    json(cypher('MATCH (p:P104) RETURN count(p) AS c')) = json('[{"c":2}]');

SELECT 'Test 104 passed: ' || count(*) || ' assertions' AS result FROM _assert;
