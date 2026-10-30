-- ========================================================================
-- Test 96: MERGE with a computed property value (GQLITE-T-0461)
-- ========================================================================
-- PURPOSE: the MERGE fast paths (a lone MERGE, UNWIND + MERGE, with or
--          without RETURN) wrote no property for a computed value:
--          `UNWIND [1, 2] AS i MERGE (:N {k: i * 2})` merged every row into
--          one empty node. Such a MERGE now runs in the row pipeline, as
--          CREATE does (GQLITE-T-0438). The row pipeline also stored a
--          boolean inside coalesce() as 1; it now reads the value's type
--          tag, and evaluates a list comprehension, list predicate, reduce
--          or subscript over a matched node. Found by the metamorphic
--          harness (relation merge_direct).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 96: MERGE with computed properties ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('MERGE (:M96a {k: 1 + 1})') as s1;
INSERT INTO _assert SELECT 'a lone MERGE',
    json(cypher('MATCH (n:M96a) RETURN n.k AS k')) = json('[{"k":2}]');

SELECT cypher('UNWIND [1, 2, 2] AS i MERGE (:M96b {k: i * 2})') as s2;
INSERT INTO _assert SELECT 'UNWIND + MERGE: one node per value',
    json(cypher('MATCH (n:M96b) RETURN n.k AS k ORDER BY k')) = json('[{"k":2},{"k":4}]');

INSERT INTO _assert SELECT 'UNWIND + MERGE + RETURN',
    json(cypher('UNWIND [1, 2] AS i MERGE (m:M96c {k: CASE WHEN i = 1 THEN ''a'' ELSE ''b'' END}) RETURN m.k AS k ORDER BY k'))
    = json('[{"k":"a"},{"k":"b"}]');

SELECT cypher('UNWIND [1, 2] AS i MERGE (:M96d {k: i})-[:R96]->(:M96e {k: i + 10})') as s3;
INSERT INTO _assert SELECT 'a computed value on a path',
    json(cypher('MATCH (a:M96d)-[:R96]->(b:M96e) RETURN a.k AS a, b.k AS b ORDER BY a'))
    = json('[{"a":1,"b":11},{"a":2,"b":12}]');

SELECT cypher('UNWIND [1, 2] AS i MERGE (:M96f {k: i > 1})') as s4;
INSERT INTO _assert SELECT 'booleans',
    json(cypher('MATCH (n:M96f) RETURN n.k AS k ORDER BY k')) = json('[{"k":false},{"k":true}]');

SELECT cypher('CREATE (:S96 {v: 2})') as s5;
SELECT cypher('MATCH (a:S96) CREATE (:M96g {k: coalesce(a.v > 0, ''n'')})') as s6;
INSERT INTO _assert SELECT 'a boolean inside coalesce() stays a boolean',
    json(cypher('MATCH (n:M96g) RETURN n.k AS k')) = json('[{"k":true}]');

SELECT cypher('MATCH (a:S96) MERGE (:M96h {k: [x IN [1, 2, 3] WHERE x > a.v | x * 2], q: any(y IN [1, 5] WHERE y > a.v), r: reduce(t = 0, z IN [1, 2] | t + z * a.v), s: [a.v, 7][1]})') as s7;
INSERT INTO _assert SELECT 'comprehension, list predicate, reduce and subscript over a node',
    json(cypher('MATCH (n:M96h) RETURN n.k AS k, n.q AS q, n.r AS r, n.s AS s')) = json('[{"k":[6],"q":true,"r":6,"s":7}]');

SELECT 'Test 96 passed: ' || count(*) || ' assertions' AS result FROM _assert;
