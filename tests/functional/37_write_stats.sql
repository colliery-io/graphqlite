-- ========================================================================
-- Test 37: Structured write statistics (GitHub #116)
-- ========================================================================
-- PURPOSE: A modification query without RETURN yields a JSON object of
--          counts, not a status string. The first byte ('{') distinguishes
--          it from a result set ('[').
-- ASSERT:  Each check below evaluates json_extract('FAIL', '$') -- which
--          raises "malformed JSON" -- when the expectation is not met, so
--          the -bail runner exits non-zero on any mismatch.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 37: Write statistics ===' as test_section;

SELECT 'Test 37.1 - CREATE nodes + relationship + properties:' as test_name;
WITH r AS (SELECT cypher('CREATE (a:X {name: ''a''})-[:R {w: 1}]->(b:X)') AS out)
SELECT CASE WHEN out = '{"nodes_created":2,"relationships_created":1,"nodes_deleted":0,"relationships_deleted":0,"properties_set":2}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 37.2 - MERGE creates once:' as test_name;
WITH r AS (SELECT cypher('MERGE (n:X {id: ''m''})') AS out)
SELECT CASE WHEN out = '{"nodes_created":1,"relationships_created":0,"nodes_deleted":0,"relationships_deleted":0,"properties_set":1}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 37.3 - MERGE matches second time:' as test_name;
WITH r AS (SELECT cypher('MERGE (n:X {id: ''m''})') AS out)
SELECT CASE WHEN out = '{"nodes_created":0,"relationships_created":0,"nodes_deleted":0,"relationships_deleted":0,"properties_set":0}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 37.4 - SET-only counts properties:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:X {id: ''m''}) SET n.k = 1, n.j = 2') AS out)
SELECT CASE WHEN out = '{"nodes_created":0,"relationships_created":0,"nodes_deleted":0,"relationships_deleted":0,"properties_set":2}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 37.5 - DELETE counts nodes:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:X {id: ''m''}) DELETE n') AS out)
SELECT CASE WHEN out = '{"nodes_created":0,"relationships_created":0,"nodes_deleted":1,"relationships_deleted":0,"properties_set":0}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 37.6 - DETACH DELETE counts nodes and cascaded relationships:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:X) DETACH DELETE n') AS out)
SELECT CASE WHEN out = '{"nodes_created":0,"relationships_created":0,"nodes_deleted":2,"relationships_deleted":1,"properties_set":0}'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 37.7 - RETURN with zero rows is still a result set:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:Nope) RETURN n') AS out)
SELECT CASE WHEN out = '[]' THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT '=== Test 37 Complete ===' as test_section;
