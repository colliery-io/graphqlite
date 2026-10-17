-- ========================================================================
-- Test 61: map projection selectors, \u escapes, octal (GQLITE-T-0136)
-- ========================================================================
-- PURPOSE: `n {.*}` with other items gave {"*": null, ...}; a map value's
--          `m {.*}` gave {}; the variable selector `m {x}` did not parse;
--          a boolean value came out as 1 / 0; a null base gave a map.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 61: map projection ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:M61 {a: 1, b: "x"})-[:T61 {w: 2}]->(:M61 {a: 2})') as setup;
INSERT INTO _assert SELECT 'all properties and more',
    json(cypher('MATCH (n:M61 {a: 1}) RETURN n {.*, c: 3} AS r')) = json('[{"r":{"a":1,"b":"x","c":3}}]');
INSERT INTO _assert SELECT 'an item overrides a property',
    json(cypher('MATCH (n:M61 {a: 1}) RETURN n {.*, a: 9, z: null} AS r')) = json('[{"r":{"a":9,"b":"x","z":null}}]');
INSERT INTO _assert SELECT 'relationship properties',
    json(cypher('MATCH ()-[r:T61]->() RETURN r {.*, k: true} AS r')) = json('[{"r":{"w":2,"k":true}}]');
INSERT INTO _assert SELECT 'a map value',
    json(cypher('WITH {a: 1, b: 2} AS m RETURN m {.*} AS r')) = json('[{"r":{"a":1,"b":2}}]');
INSERT INTO _assert SELECT 'variable selector',
    json(cypher('WITH 5 AS x, {a: 1} AS m RETURN m {.a, x} AS r')) = json('[{"r":{"a":1,"x":5}}]');
INSERT INTO _assert SELECT 'booleans stay booleans',
    json(cypher('MATCH (n:M61 {a: 1}) RETURN n {.a, k: false, c: 1 > 0} AS r')) = json('[{"r":{"a":1,"k":false,"c":true}}]');
INSERT INTO _assert SELECT 'a null base is null',
    json(cypher('OPTIONAL MATCH (n:None61) RETURN n {.a} AS r, n {.*} AS s')) = json('[{"r":null,"s":null}]');
INSERT INTO _assert SELECT 'after WITH',
    json(cypher('MATCH (n:M61 {a: 2}) WITH n RETURN n {.*} AS r')) = json('[{"r":{"a":2}}]');
INSERT INTO _assert SELECT 'unicode escape',
    json(cypher('RETURN "café" AS s, size("é") AS n')) = json('[{"s":"café","n":1}]');
INSERT INTO _assert SELECT 'octal integer',
    json(cypher('RETURN 0o52 AS o, -0o17 AS p')) = json('[{"o":42,"p":-15}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 61 Complete ===' as test_section;
