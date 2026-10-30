-- ========================================================================
-- Test 94: max() and min() keep the type of their input (GQLITE-T-0475)
-- ========================================================================
-- PURPOSE: with a grouping key the RETURN goes through the sorter, which
--          drops subtypes, and the hidden tag of max()/min() was
--          _gql_typetag() alone: no tag for text, so the string "2" was
--          shown as the number 2, and max() of booleans as 1. A string is
--          now tagged 'text' (unless it is JSON) and a boolean 'bool'.
--          Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 94: max() and min() types ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A94 {id: 2, g: 1}), (:A94 {id: 10, g: 1}), (:A94 {id: 3, g: 2})') as setup;

INSERT INTO _assert SELECT 'strings with a grouping key',
    json(cypher('MATCH (d:A94) UNWIND [1] AS i RETURN i, max(toString(d.id)) AS c1, min(toString(d.id)) AS c2'))
    = json('[{"i":1,"c1":"3","c2":"10"}]');
INSERT INTO _assert SELECT 'strings without one',
    json(cypher('MATCH (d:A94) RETURN max(toString(d.id)) AS c1')) = json('[{"c1":"3"}]');
INSERT INTO _assert SELECT 'numbers stay numbers',
    json(cypher('MATCH (d:A94) RETURN d.g AS g, max(d.id) AS m, min(d.id * 1.5) AS r ORDER BY g'))
    = json('[{"g":1,"m":10,"r":3.0},{"g":2,"m":3,"r":4.5}]');
INSERT INTO _assert SELECT 'lists stay lists',
    json(cypher('MATCH (d:A94) RETURN d.g AS g, max([d.id]) AS l ORDER BY g'))
    = json('[{"g":1,"l":[10]},{"g":2,"l":[3]}]');
INSERT INTO _assert SELECT 'booleans stay booleans',
    json(cypher('MATCH (d:A94) RETURN d.g AS g, max(d.id > 2) AS b, min(d.id > 2) AS c ORDER BY g'))
    = json('[{"g":1,"b":true,"c":false},{"g":2,"b":true,"c":true}]');

SELECT 'Test 94 passed: ' || count(*) || ' assertions' AS result FROM _assert;
