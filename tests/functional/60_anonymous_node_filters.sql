-- ========================================================================
-- Test 60: inline property filters on anonymous nodes (GQLITE-T-0430)
-- ========================================================================
-- PURPOSE: the alias of an anonymous node was read after its buffer went
--          out of scope; an -O2 (release) build gave the filter a garbage
--          table alias. CI runs this file on the release build too.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 60: anonymous node filters ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:I60 {id: ''a''}), (b:I60 {id: ''b''}), (c:I60 {id: ''c''}), (a)-[:R60 {w: 1}]->(b), (a)-[:R60 {w: 2}]->(c)') as setup;
INSERT INTO _assert SELECT 'anonymous node filter from an UNWIND row',
    json(cypher('UNWIND $rows AS row MATCH (:I60 {id: row.a})-[:R60 {w: row.w}]->(t) RETURN t.id AS id',
                '{"rows": [{"a": "a", "w": 2}]}')) = json('[{"id":"c"}]');
INSERT INTO _assert SELECT 'anonymous node filter from WITH',
    json(cypher('WITH ''a'' AS k MATCH (:I60 {id: k})-[:R60]->(t) RETURN t.id AS id ORDER BY id')) = json('[{"id":"b"},{"id":"c"}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 60 Complete ===' as test_section;
