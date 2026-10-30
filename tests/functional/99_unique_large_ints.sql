-- ========================================================================
-- Test 99: a unique key tells large integers apart (GQLITE-T-0441)
-- ========================================================================
-- PURPOSE: the key of a unique check read numbers as REAL, so integers
--          above 2^53 that differ (9007199254740992, 9007199254740993)
--          were the same key: valid data was refused. An integer is now
--          keyed by its exact value, and a whole-number float by the same
--          integer, so 1 and 1.0 still collide. (Storing or dropping a
--          schema object on a read-only database is now an error; it was
--          reported as done.)
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 99: unique keys of large integers ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE CONSTRAINT u99 FOR (n:P99) REQUIRE n.id IS UNIQUE') as setup;
SELECT cypher('CREATE (:P99 {id: 9007199254740992})') as s1;
SELECT cypher('CREATE (:P99 {id: 9007199254740993})') as s2;
INSERT INTO _assert SELECT 'two integers above 2^53 are two values',
    json(cypher('MATCH (n:P99) RETURN count(*) AS c')) = json('[{"c":2}]');


SELECT cypher('CREATE CONSTRAINT u99b FOR (n:Q99) REQUIRE n.id IS UNIQUE') as setup2;
SELECT cypher('CREATE (:Q99 {id: 2.5}), (:Q99 {id: 3})') as s4;
INSERT INTO _assert SELECT 'the data meets the constraint',
    json(cypher('MATCH (n:Q99) RETURN count(*) AS c')) = json('[{"c":2}]');

SELECT 'Test 99 passed: ' || count(*) || ' assertions' AS result FROM _assert;
