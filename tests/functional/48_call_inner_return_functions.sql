-- ========================================================================
-- Test 48: function calls in a CALL subquery's inner RETURN (GQLITE-T-0374)
-- ========================================================================
-- PURPOSE: size()/length()/toInteger()/toUpper() over an imported variable
--          inside CALL { WITH f RETURN ... } must evaluate. size(f.id) used
--          to abort the whole process (the inner RETURN evaluator pointed the
--          transform's growable SQL buffer at a 2 KiB stack array).
-- COVERS:  GQLITE-T-0374.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 48: CALL inner RETURN functions ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:F48 {id: "cf1", tags: [1, 2, 3]})') as setup;

-- Values compare numerically (exported aliases are rendered as text today,
-- tracked on GQLITE-T-0373).
INSERT INTO _assert SELECT 'T-0374 1.1 size(string prop) inside CALL',
    CAST(json_extract(cypher('MATCH (f:F48) CALL { WITH f RETURN size(f.id) AS n } RETURN n'), '$[0].n') AS INTEGER) = 3;
INSERT INTO _assert SELECT 'T-0374 1.2 exported size() in arithmetic',
    CAST(json_extract(cypher('MATCH (f:F48) CALL { WITH f RETURN size(f.id) AS n } RETURN n + 1 AS n1'), '$[0].n1') AS INTEGER) = 4;
INSERT INTO _assert SELECT 'T-0374 1.3 size(list prop) inside CALL',
    CAST(json_extract(cypher('MATCH (f:F48) CALL { WITH f RETURN size(f.tags) AS n } RETURN n'), '$[0].n') AS INTEGER) = 3;
INSERT INTO _assert SELECT 'T-0374 1.4 toInteger() inside CALL',
    CAST(json_extract(cypher('MATCH (f:F48) CALL { WITH f RETURN toInteger("7") AS i } RETURN i'), '$[0].i') AS INTEGER) = 7;
INSERT INTO _assert SELECT 'T-0374 1.5 toUpper(prop) inside CALL',
    json_extract(cypher('MATCH (f:F48) CALL { WITH f RETURN toUpper(f.id) AS u } RETURN u'), '$[0].u') = 'CF1';
INSERT INTO _assert SELECT 'T-0374 1.6 several functions in one inner RETURN',
    json_array_length(cypher('MATCH (f:F48) CALL { WITH f RETURN size(f.id) AS n, size(f.tags) AS m, toUpper(f.id) AS u } RETURN n, m, u')) = 1;

SELECT name, ok FROM _assert;
SELECT '=== Test 48 Complete ===' as test_section;
