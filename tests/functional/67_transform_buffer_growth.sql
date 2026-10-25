-- ========================================================================
-- Test 67: expressions whose SQL outgrows a temporary buffer (GQLITE-T-0454)
-- ========================================================================
-- PURPOSE: WITH ... WHERE and UNWIND <expression> translate the expression
--          into a 4 KB temporary buffer. When append_sql grew it with
--          realloc, the old pointer was freed again: a double free that
--          aborted the process (SIGABRT / SIGTRAP), even on an empty graph.
--          Found by the metamorphic harness (COLLIERY-I-0658).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 67: transform buffer growth ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:I67 {id: 1, v: 0})-[:R67]->(b:I67 {id: 2, v: 5})') as setup;

INSERT INTO _assert SELECT 'WITH * WHERE: OR of comparisons across two variables',
    json(cypher('MATCH (a:I67)-->(b) WITH * WHERE (a.v <= b.id OR b.v <= a.id) RETURN a.id AS a')) = json('[{"a":1}]');
INSERT INTO _assert SELECT 'WITH a, b WHERE NOT (...)',
    json(cypher('MATCH (a:I67)-->(b) WITH a, b WHERE NOT (a.v <= b.id OR b.v <= a.id) RETURN a.id AS a')) = json('[]');
INSERT INTO _assert SELECT 'WITH WHERE: property compared with a predicate',
    json(cypher('MATCH (a:I67) WITH a WHERE a.v = (a.v IS NOT NULL) RETURN a.id AS a')) = json('[]');
INSERT INTO _assert SELECT 'UNWIND of a long list expression',
    json(cypher('MATCH (a:I67)-->(b) UNWIND [0] + CASE WHEN (a.v <= b.id OR b.v <= a.id) AND (a.id <= b.v OR b.id <= a.v) THEN [1] ELSE [2] END AS i RETURN i')) = json('[{"i":0},{"i":1}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 67 Complete ===' as test_section;
