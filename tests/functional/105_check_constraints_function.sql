-- ========================================================================
-- Test 105: gql_check_constraints() (GQLITE-T-0445)
-- ========================================================================
-- PURPOSE: writers outside cypher() (the Python and Rust bulk importers)
--          wrote the tables directly, and no constraint saw their rows.
--          gql_check_constraints() returns NULL when the data meets every
--          constraint and fails with the violation otherwise; the importers
--          call it before they commit and roll back on a failure (see the
--          binding tests).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 105: gql_check_constraints() ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'no constraints', gql_check_constraints() IS NULL;
SELECT cypher('CREATE CONSTRAINT u105 FOR (p:P105) REQUIRE p.id IS UNIQUE') as setup;
SELECT cypher('CREATE (:P105 {id: 1}), (:P105 {id: 2})') as rows;
INSERT INTO _assert SELECT 'the data meets the constraint', gql_check_constraints() IS NULL;

SELECT 'Test 105 passed: ' || count(*) || ' assertions' AS result FROM _assert;
