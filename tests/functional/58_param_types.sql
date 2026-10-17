-- ========================================================================
-- Test 58: parameters match by type (GQLITE-T-0431)
-- ========================================================================
-- PURPOSE: `{k: $x}` and `n.k = $x` compared a parameter against every
--          property table, so $x = 1 matched a stored true and '1'.
--          A per-row MERGE reads values from the row.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 58: parameter types ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:Q58 {k: true}), (:Q58 {k: 1}), (:Q58 {k: ''1''}), (:Q58 {k: 1.0}), (:Q58 {k: [1]})') as setup1;
INSERT INTO _assert SELECT 'inline integer parameter',
    json_extract(cypher('MATCH (n:Q58 {k: $x}) RETURN count(n) AS c', '{"x": 1}'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'inline boolean parameter',
    json_extract(cypher('MATCH (n:Q58 {k: $x}) RETURN count(n) AS c', '{"x": true}'), '$[0].c') = 1;
INSERT INTO _assert SELECT 'inline string parameter',
    json_extract(cypher('MATCH (n:Q58 {k: $x}) RETURN count(n) AS c', '{"x": "1"}'), '$[0].c') = 1;
INSERT INTO _assert SELECT 'inline list parameter',
    json_extract(cypher('MATCH (n:Q58 {k: $x}) RETURN count(n) AS c', '{"x": [1]}'), '$[0].c') = 1;
INSERT INTO _assert SELECT 'WHERE equality with an integer parameter',
    json_extract(cypher('MATCH (n:Q58) WHERE n.k = $x RETURN count(n) AS c', '{"x": 1}'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'WHERE equality with a boolean parameter',
    json_extract(cypher('MATCH (n:Q58) WHERE n.k = $x RETURN count(n) AS c', '{"x": true}'), '$[0].c') = 1;

SELECT cypher('CREATE ()-[:R58 {w: true}]->(), ()-[:R58 {w: 1}]->()') as setup2;
INSERT INTO _assert SELECT 'relationship parameter',
    json_extract(cypher('MATCH ()-[r:R58 {w: $x}]->() RETURN count(r) AS c', '{"x": 1}'), '$[0].c') = 1;

-- A per-row MERGE keeps the type of each row's value.
SELECT cypher('UNWIND [1, 2, 3, 4] AS i CREATE (:S58 {id: i})') as setup3;
SELECT cypher('MATCH (n:S58) MERGE (:C58 {k: n.id % 2 = 0})') as merge_per_row;
INSERT INTO _assert SELECT 'per-row MERGE of booleans',
    json(cypher('MATCH (c:C58) RETURN c.k AS k ORDER BY k')) = json('[{"k":false},{"k":true}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 58 Complete ===' as test_section;
