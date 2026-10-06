-- ========================================================================
-- Test 44: startNode(r) / endNode(r) project the endpoint node (GQLITE-T-0181)
-- ========================================================================
-- PURPOSE: Bare startNode(r) / endNode(r) used to return the raw source_id /
--          target_id integer. They must project the node in the same shape
--          as `RETURN n` ({"id","labels","properties"}), while property
--          access, id(), labels() and equality on the endpoint keep working.
-- ASSERT:  _assert has CHECK (ok = 1), so a failed assertion aborts the
--          -bail runner with a constraint violation.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 44: startNode()/endNode() node projection ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:P44 {name: "Alice"})-[:K44 {since: 2020}]->(b:P44 {name: "Bob"})') AS setup;

SELECT 'Test 44.1 - bare startNode/endNode return node objects:' as test_name;
WITH r AS (SELECT cypher('MATCH ()-[r:K44]->() RETURN startNode(r) AS sn, endNode(r) AS en') AS out)
SELECT out FROM r;
INSERT INTO _assert SELECT 'T-0181 44.1 sn is object',
    json_type(cypher('MATCH ()-[r:K44]->() RETURN startNode(r) AS sn, endNode(r) AS en'), '$[0].sn') = 'object';
INSERT INTO _assert SELECT 'T-0181 44.1 sn.properties.name',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN startNode(r) AS sn, endNode(r) AS en'), '$[0].sn.properties.name') = 'Alice';
INSERT INTO _assert SELECT 'T-0181 44.1 en.properties.name',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN startNode(r) AS sn, endNode(r) AS en'), '$[0].en.properties.name') = 'Bob';
INSERT INTO _assert SELECT 'T-0181 44.1 sn.labels',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN startNode(r) AS sn, endNode(r) AS en'), '$[0].sn.labels[0]') = 'P44';

SELECT 'Test 44.2 - same shape as RETURN n:' as test_name;
INSERT INTO _assert SELECT 'T-0181 44.2 identical to RETURN a',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN startNode(r) AS n'), '$[0].n') =
    json_extract(cypher('MATCH (a:P44 {name: "Alice"}) RETURN a AS n'), '$[0].n');

SELECT 'Test 44.3 - property access on the endpoint still works:' as test_name;
INSERT INTO _assert SELECT 'T-0181 44.3 startNode(r).name',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN startNode(r).name AS s, endNode(r).name AS e'), '$[0].s') = 'Alice';
INSERT INTO _assert SELECT 'T-0181 44.3 endNode(r).name',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN startNode(r).name AS s, endNode(r).name AS e'), '$[0].e') = 'Bob';

SELECT 'Test 44.4 - id() and labels() of the endpoint:' as test_name;
INSERT INTO _assert SELECT 'T-0181 44.4 id(startNode(r)) = id(a)',
    json_extract(cypher('MATCH (a)-[r:K44]->(b) RETURN id(startNode(r)) = id(a) AS s, id(endNode(r)) = id(b) AS e'), '$[0].s') = 1
    AND json_extract(cypher('MATCH (a)-[r:K44]->(b) RETURN id(startNode(r)) = id(a) AS s, id(endNode(r)) = id(b) AS e'), '$[0].e') = 1;
INSERT INTO _assert SELECT 'T-0181 44.4 labels(startNode(r))',
    json_extract(cypher('MATCH ()-[r:K44]->() RETURN labels(startNode(r)) AS l'), '$[0].l[0]') = 'P44';

SELECT 'Test 44.5 - endpoint equality and WITH projection:' as test_name;
INSERT INTO _assert SELECT 'T-0181 44.5 startNode(r) = a',
    json_extract(cypher('MATCH (a)-[r:K44]->() RETURN startNode(r) = a AS same'), '$[0].same') = 1;
INSERT INTO _assert SELECT 'T-0181 44.5 WITH startNode(r) AS s RETURN s.name',
    json_extract(cypher('MATCH ()-[r:K44]->() WITH startNode(r) AS s RETURN s.name AS n'), '$[0].n') = 'Alice';

SELECT 'Test 44.6 - null propagation (OPTIONAL MATCH miss, literal null):' as test_name;
INSERT INTO _assert SELECT 'T-0181 44.6 optional miss',
    json_type(cypher('MATCH (a:P44 {name: "Bob"}) OPTIONAL MATCH (a)-[r:K44]->() RETURN startNode(r) AS s'), '$[0].s') = 'null';
INSERT INTO _assert SELECT 'T-0181 44.6 startNode(null)',
    json_type(cypher('RETURN startNode(null) AS s'), '$[0].s') = 'null';

SELECT '=== Assertions run (all must show ok=1) ===' as section;
SELECT name, ok FROM _assert;

SELECT cypher('MATCH (n:P44) DETACH DELETE n') as cleanup;

SELECT '=== Test 44 Complete ===' as test_section;
