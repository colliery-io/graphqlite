-- ========================================================================
-- Test 39: Issue Regression Tests
-- ========================================================================
-- PURPOSE: Round-trip regression tests for reported GitHub issues, one
--          section per issue. Referenced from
--          docs/testing/semantic-coverage-matrix.md.
-- COVERS:  GH-96 (rel inline property filter with $param),
--          GH-95 (MATCH+CREATE ... RETURN created rel var),
--          GH-97 (MERGE with $param inline properties / parallel edges)
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 39: Issue Regression Tests ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- =======================================================================
-- SECTION 1: GitHub #96 — inline relationship property filter with $param
-- =======================================================================
SELECT '=== Section 1: GH-96 rel inline property filter with $param ===' as section;

SELECT cypher('CREATE (:G96 {name: "alice"})') as setup;
SELECT cypher('CREATE (:G96 {name: "bob"})') as setup;
SELECT cypher('MATCH (x:G96 {name:"alice"}),(y:G96 {name:"bob"}) CREATE (x)-[:G96_KNOWS {w: 1}]->(y)') as setup;
SELECT cypher('MATCH (x:G96 {name:"alice"}),(y:G96 {name:"bob"}) CREATE (x)-[:G96_KNOWS {w: 2}]->(y)') as setup;
SELECT cypher('MATCH (x:G96 {name:"alice"}),(y:G96 {name:"bob"}) CREATE (x)-[:G96_KNOWS {w: 3, tag: "target"}]->(y)') as setup;

SELECT 'Test 1.1 - inline string $param matches only the tagged edge:' as test_name;
SELECT cypher('MATCH ()-[r:G96_KNOWS {tag: $t}]->() RETURN r.w AS w', '{"t": "target"}') as result;
INSERT INTO _assert SELECT 'GH-96 1.1 rows',
    json_array_length(cypher('MATCH ()-[r:G96_KNOWS {tag: $t}]->() RETURN r.w AS w', '{"t": "target"}')) = 1;
INSERT INTO _assert SELECT 'GH-96 1.1 value',
    json_extract(cypher('MATCH ()-[r:G96_KNOWS {tag: $t}]->() RETURN r.w AS w', '{"t": "target"}'), '$[0].w') = 3;

SELECT 'Test 1.2 - inline literal filter agrees with the $param filter:' as test_name;
INSERT INTO _assert SELECT 'GH-96 1.2 literal parity',
    json_array_length(cypher('MATCH ()-[r:G96_KNOWS {tag: "target"}]->() RETURN r.w AS w')) = 1;

SELECT 'Test 1.3 - WHERE-clause $param agrees with the inline filter:' as test_name;
INSERT INTO _assert SELECT 'GH-96 1.3 where parity',
    json_array_length(cypher('MATCH ()-[r:G96_KNOWS]->() WHERE r.tag = $t RETURN r.w AS w', '{"t": "target"}')) = 1;

SELECT 'Test 1.4 - inline integer $param:' as test_name;
INSERT INTO _assert SELECT 'GH-96 1.4 int param',
    json_extract(cypher('MATCH ()-[r:G96_KNOWS {w: $v}]->() RETURN r.w AS w', '{"v": 2}'), '$[0].w') = 2;

SELECT 'Test 1.5 - non-matching $param returns no rows:' as test_name;
INSERT INTO _assert SELECT 'GH-96 1.5 no match',
    json_array_length(cypher('MATCH ()-[r:G96_KNOWS {tag: $t}]->() RETURN r.w AS w', '{"t": "nope"}')) = 0;

SELECT 'Test 1.6 - SET scoped by inline $param filter touches one edge only:' as test_name;
SELECT cypher('MATCH ()-[r:G96_KNOWS {tag: $t}]->() SET r.marked = 1', '{"t": "target"}') as result;
INSERT INTO _assert SELECT 'GH-96 1.6 scoped set',
    json_array_length(cypher('MATCH ()-[r:G96_KNOWS]->() WHERE r.marked = 1 RETURN r.w AS w')) = 1;

-- =======================================================================
-- SECTION 2: GitHub #95 — RETURN of rel created between MATCH-bound nodes
-- =======================================================================
SELECT '=== Section 2: GH-95 MATCH+CREATE ... RETURN created rel var ===' as section;

SELECT cypher('CREATE (:G95Hub {name: "hub0"})') as setup;
SELECT cypher('CREATE (:G95Hub {name: "hub1"})') as setup;

SELECT 'Test 2.1 - comma multi-pattern MATCH, RETURN r.prop:' as test_name;
SELECT cypher('MATCH (x:G95Hub {name: "hub0"}), (y:G95Hub {name: "hub1"}) CREATE (x)-[r:G95_LINKS {seq: 1}]->(y) RETURN r.seq AS s') as result;
INSERT INTO _assert SELECT 'GH-95 2.1 value',
    json_extract(cypher('MATCH ()-[e:G95_LINKS]->() RETURN count(e) AS c'), '$[0].c') = 1;

SELECT 'Test 2.2 - two separate MATCH clauses, RETURN r.prop:' as test_name;
INSERT INTO _assert SELECT 'GH-95 2.2 returned value',
    json_extract(cypher('MATCH (x:G95Hub {name: "hub0"}) MATCH (y:G95Hub {name: "hub1"}) CREATE (x)-[r2:G95_LINKS {seq: 2}]->(y) RETURN r2.seq AS s'), '$[0].s') = 2;

SELECT 'Test 2.3 - bare RETURN r carries the relationship type:' as test_name;
INSERT INTO _assert SELECT 'GH-95 2.3 bare rel',
    json_extract(cypher('MATCH (x:G95Hub {name: "hub0"}), (y:G95Hub {name: "hub1"}) CREATE (x)-[r:G95_LINKS2 {seq: 3}]->(y) RETURN r'), '$[0].r.type') = 'G95_LINKS2';

SELECT 'Test 2.4 - exactly one edge per CREATE (no duplicate writes):' as test_name;
INSERT INTO _assert SELECT 'GH-95 2.4 edge count',
    json_extract(cypher('MATCH ()-[e:G95_LINKS]->() RETURN count(e) AS c'), '$[0].c') = 2;

SELECT 'Test 2.5 - multi-row MATCH creates and returns one row per match:' as test_name;
SELECT cypher('CREATE (:G95P {n: 1})') as setup;
SELECT cypher('CREATE (:G95P {n: 2})') as setup;
SELECT cypher('CREATE (:G95P {n: 3})') as setup;
INSERT INTO _assert SELECT 'GH-95 2.5 rows',
    json_array_length(cypher('MATCH (p:G95P) CREATE (p)-[r:G95_HAS {k: 7}]->(:G95Q) RETURN r.k AS k')) = 3;
INSERT INTO _assert SELECT 'GH-95 2.5 edges',
    json_extract(cypher('MATCH ()-[e:G95_HAS]->() RETURN count(e) AS c'), '$[0].c') = 3;

-- =======================================================================
-- SECTION 3: GitHub #97 — MERGE with $param inline props / parallel edges
-- =======================================================================
SELECT '=== Section 3: GH-97 MERGE with $param inline properties ===' as section;

SELECT 'Test 3.1 - node MERGE with $param creates then matches in place:' as test_name;
SELECT cypher('MERGE (n:G97 {pid: $p})', '{"p": "m1"}') as result;
SELECT cypher('MERGE (n:G97 {pid: $p})', '{"p": "m1"}') as result;
SELECT cypher('MERGE (n:G97 {pid: $p})', '{"p": "m2"}') as result;
INSERT INTO _assert SELECT 'GH-97 3.1 node count',
    json_extract(cypher('MATCH (n:G97) RETURN count(n) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'GH-97 3.1 prop stored',
    json_array_length(cypher('MATCH (n:G97 {pid: "m2"}) RETURN n.pid AS p')) = 1;

SELECT 'Test 3.2 - edge MERGE keyed on $param id addresses parallel edges:' as test_name;
SELECT cypher('MATCH (a:G97 {pid: $s}), (b:G97 {pid: $t}) MERGE (a)-[e:G97_REL {eid: $e}]->(b)', '{"s": "m1", "t": "m2", "e": "e1"}') as result;
SELECT cypher('MATCH (a:G97 {pid: $s}), (b:G97 {pid: $t}) MERGE (a)-[e:G97_REL {eid: $e}]->(b)', '{"s": "m1", "t": "m2", "e": "e1"}') as result;
SELECT cypher('MATCH (a:G97 {pid: $s}), (b:G97 {pid: $t}) MERGE (a)-[e:G97_REL {eid: $e}]->(b)', '{"s": "m1", "t": "m2", "e": "e2"}') as result;
INSERT INTO _assert SELECT 'GH-97 3.2 parallel edges',
    json_extract(cypher('MATCH ()-[e:G97_REL]->() RETURN count(e) AS c'), '$[0].c') = 2;

SELECT 'Test 3.3 - each parallel edge individually addressable by its id:' as test_name;
SELECT cypher('MATCH ()-[e:G97_REL {eid: $e}]->() SET e.seq = 10', '{"e": "e1"}') as result;
INSERT INTO _assert SELECT 'GH-97 3.3 targeted update',
    json_array_length(cypher('MATCH ()-[e:G97_REL]->() WHERE e.seq = 10 RETURN e.eid AS eid')) = 1;

-- =======================================================================
-- VERIFICATION SUMMARY
-- =======================================================================
SELECT '=== Assertions run (all must show ok=1) ===' as section;
SELECT name, ok FROM _assert;

-- =======================================================================
-- TEARDOWN
-- =======================================================================
SELECT '=== Teardown: Cleaning up ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

SELECT '=== Test 39 Complete ===' as test_section;
