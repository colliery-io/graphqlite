-- ========================================================================
-- Test 38: nodeSimilarity(threshold, top_k) argument handling (GitHub #107)
-- ========================================================================
-- PURPOSE: The two-argument numeric form must apply BOTH the threshold and
--          the top_k cap. Uses the same json_extract('FAIL', '$') trick as
--          test 37 to fail the -bail runner on a mismatch.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 38: nodeSimilarity arguments ===' as test_section;

-- a-b share {c,d} (1.0); a-e and b-e share only c (0.5); pairs with c/d are 0.0
SELECT cypher('CREATE (a:Node {id: ''a''}), (b:Node {id: ''b''}), (c:Node {id: ''c''}), (d:Node {id: ''d''}), (e:Node {id: ''e''})') AS setup;
SELECT cypher('MATCH (a {id: ''a''}), (c {id: ''c''}) CREATE (a)-[:L]->(c)') AS setup;
SELECT cypher('MATCH (a {id: ''a''}), (d {id: ''d''}) CREATE (a)-[:L]->(d)') AS setup;
SELECT cypher('MATCH (b {id: ''b''}), (c {id: ''c''}) CREATE (b)-[:L]->(c)') AS setup;
SELECT cypher('MATCH (b {id: ''b''}), (d {id: ''d''}) CREATE (b)-[:L]->(d)') AS setup;
SELECT cypher('MATCH (e {id: ''e''}), (c {id: ''c''}) CREATE (e)-[:L]->(c)') AS setup;

SELECT 'Test 38.1 - threshold alone filters (baseline):' as test_name;
WITH r AS (SELECT cypher('RETURN nodeSimilarity(0.9)') AS out)
SELECT CASE WHEN instr(out, '"similarity":1.0') > 0 AND instr(out, '"similarity":0.5') = 0 AND instr(out, '"similarity":0.0') = 0
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 38.2 - threshold + top_k honours the threshold:' as test_name;
WITH r AS (SELECT cypher('RETURN nodeSimilarity(0.9, 10)') AS out)
SELECT CASE WHEN instr(out, '"similarity":1.0') > 0 AND instr(out, '"similarity":0.5') = 0 AND instr(out, '"similarity":0.0') = 0
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 38.3 - threshold 0.4 + top_k keeps the 0.5 pairs but drops 0.0:' as test_name;
WITH r AS (SELECT cypher('RETURN nodeSimilarity(0.4, 10)') AS out)
SELECT CASE WHEN instr(out, '"similarity":0.5') > 0 AND instr(out, '"similarity":0.0') = 0
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 38.4 - top_k alone (threshold 0) returns exactly one pair:' as test_name;
WITH r AS (SELECT cypher('RETURN nodeSimilarity(0, 1)') AS out)
SELECT CASE WHEN (length(out) - length(replace(out, '"node1"', ''))) / length('"node1"') = 1
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT cypher('MATCH (n) DETACH DELETE n') AS cleanup;
SELECT '=== Test 38 Complete ===' as test_section;
