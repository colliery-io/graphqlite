-- ========================================================================
-- Test 45: DELETE semantics after GQLITE-T-0254 / GQLITE-T-0253
-- ========================================================================
-- PURPOSE: 1. A DELETE statement is verified as a whole before anything is
--             applied: `DELETE x, r` succeeds when the node's relationships
--             are deleted in the same clause, whatever the item order
--             (node-first used to fail with "still has relationships").
--          2. DETACH DELETE still cascades and reports the cascaded count.
--          3. Identity-only projections after DELETE (`RETURN r`,
--             `RETURN count(*)`) stay legal.
-- NOTE:    The error paths (ConstraintVerificationFailed: DeleteConnectedNode
--          for a connected node without DETACH, EntityNotFound:
--          DeletedEntityAccess for `DELETE n RETURN n.prop`) raise an SQL
--          error from cypher(), which makes `sqlite3 -bail` exit non-zero
--          even under `.bail off`, so they cannot be asserted in this
--          runner. They are covered by tests/test_executor_delete.c
--          (T-0253 / T-0254 cases, including the graph-unchanged checks),
--          the Python/Rust binding tests, and TCK Delete1 [7],
--          Return2 [15]-[17].
-- ASSERT:  _assert has CHECK (ok = 1), so a failed assertion aborts the
--          -bail runner with a constraint violation.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 45: DELETE statement semantics ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (x:Conn45 {k: 3})-[:R45]->(:Other45), (x)-[:R45]->(:Other45)') AS setup;

SELECT 'Test 45.1 - DELETE x, r (node listed first) deletes the node and both relationships:' as test_name;
WITH r AS (SELECT cypher('MATCH (x:Conn45)-[r:R45]->() DELETE x, r') AS out)
SELECT CASE WHEN out = '{"nodes_created":0,"relationships_created":0,"nodes_deleted":1,"relationships_deleted":2,"properties_set":0}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;
INSERT INTO _assert SELECT 'T-0254 45.1 node gone',
    json_extract(cypher('MATCH (n:Conn45) RETURN count(n) AS c'), '$[0].c') = 0;
INSERT INTO _assert SELECT 'T-0254 45.1 relationships gone',
    json_extract(cypher('MATCH ()-[r:R45]->() RETURN count(r) AS c'), '$[0].c') = 0;
INSERT INTO _assert SELECT 'T-0254 45.1 other endpoints kept',
    json_extract(cypher('MATCH (n:Other45) RETURN count(n) AS c'), '$[0].c') = 2;

SELECT 'Test 45.2 - undirected match deleting both endpoints and the relationship (deduplicated counts):' as test_name;
SELECT cypher('CREATE (:U45)-[:RU45]->(:U45)') AS setup;
WITH r AS (SELECT cypher('MATCH (a:U45)-[r:RU45]-(b:U45) DELETE r, a, b RETURN count(*) AS c') AS out)
SELECT CASE WHEN json_extract(out, '$[0].c') = 2
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;
INSERT INTO _assert SELECT 'T-0254 45.2 nodes gone',
    json_extract(cypher('MATCH (n:U45) RETURN count(n) AS c'), '$[0].c') = 0;

SELECT 'Test 45.3 - DETACH DELETE still cascades:' as test_name;
SELECT cypher('CREATE (y:Conn45b)-[:R45b]->(:Other45), (y)-[:R45b]->(:Other45)') AS setup;
INSERT INTO _assert SELECT 'T-0254 45.3 detach counts',
    cypher('MATCH (y:Conn45b) DETACH DELETE y') =
    '{"nodes_created":0,"relationships_created":0,"nodes_deleted":1,"relationships_deleted":2,"properties_set":0}';
INSERT INTO _assert SELECT 'T-0254 45.3 relationships gone',
    json_extract(cypher('MATCH ()-[r:R45b]->() RETURN count(r) AS c'), '$[0].c') = 0;

SELECT 'Test 45.4 - identity-only projections after DELETE stay legal:' as test_name;
SELECT cypher('CREATE (:Del45 {num: 0})-[:T45 {num: 7}]->(:Del45 {num: 1})') AS setup;
INSERT INTO _assert SELECT 'T-0253 45.4 RETURN r after DELETE r',
    json_extract(cypher('MATCH ()-[r:T45]->() DELETE r RETURN r'), '$[0].r.type') = 'T45';
INSERT INTO _assert SELECT 'T-0253 45.4 relationship deleted',
    json_extract(cypher('MATCH ()-[r:T45]->() RETURN count(r) AS c'), '$[0].c') = 0;
INSERT INTO _assert SELECT 'T-0253 45.4 count(*) after DELETE n',
    json_extract(cypher('MATCH (n:Del45) DELETE n RETURN count(*) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'T-0253 45.4 nodes deleted',
    json_extract(cypher('MATCH (n:Del45) RETURN count(n) AS c'), '$[0].c') = 0;

SELECT '=== Assertions run (all must show ok=1) ===' as section;
SELECT name, ok FROM _assert;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

SELECT '=== Test 45 Complete ===' as test_section;
