-- ========================================================================
-- Test 40: JSON escaping of control characters in the text result path
-- ========================================================================
-- PURPOSE: cypher() output must be valid JSON even when a string value
--          contains newlines, tabs, or other control characters, and the
--          EXPLAIN output (multi-line) must be parseable (perf review C1).
--          Also asserts the SQL shapes introduced by perf findings F1/F2.
-- ASSERT:  json_extract('FAIL', '$') raises "malformed JSON" on mismatch so
--          the -bail runner exits non-zero.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 40: JSON escaping ===' as test_section;

SELECT 'Test 40.1 - newline/tab/quote/backslash in a returned string round-trip:' as test_name;
WITH r AS (SELECT cypher('RETURN $s AS s', '{"s": "a\nb\tc\"d\\e"}') AS out)
SELECT CASE WHEN json_valid(out) = 1 AND json_extract(out, '$[0].s') = 'a' || char(10) || 'b' || char(9) || 'c"d\e'
            THEN 'PASS: ' || replace(replace(out, char(10), '\n'), char(9), '\t') ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 40.2 - other control characters are emitted as \u00XX:' as test_name;
-- Build the params JSON at run time so the control character reaches the
-- text result path without appearing literally in this file.
WITH r AS (SELECT cypher('RETURN $s AS s', '{"s": "x' || char(1) || 'y"}') AS out)
SELECT CASE WHEN json_valid(out) = 1 AND json_extract(out, '$[0].s') = 'x' || char(1) || 'y' AND instr(out, '\u0001') > 0
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 40.3 - EXPLAIN output is valid JSON:' as test_name;
SELECT cypher('CREATE (:E {id: ''e1''})') AS setup;
WITH r AS (SELECT cypher('EXPLAIN MATCH (n:E {id: $id}) RETURN n.id', '{"id": "e1"}') AS out)
SELECT CASE WHEN json_valid(out) = 1 AND instr(json_extract(out, '$[0].column_0'), 'SQL: ') > 0
            THEN 'PASS: explain is json' ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 40.4 - parameterized inline match compiles to an IN semi-join (perf F1):' as test_name;
WITH r AS (SELECT cypher('EXPLAIN MATCH (n {id: $id}) RETURN n.id', '{"id": "e1"}') AS out)
SELECT CASE WHEN instr(json_extract(out, '$[0].column_0'), '.id IN (SELECT npt.node_id FROM node_props_text npt') > 0
             AND instr(json_extract(out, '$[0].column_0'), 'EXISTS(SELECT 1 FROM node_props_text npt') = 0
            THEN 'PASS: semi-join form' ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 40.5 - varlen CTE base case is anchored at the bound start node (perf F2):' as test_name;
WITH r AS (SELECT cypher('EXPLAIN MATCH (a {id: ''e1''})-[:R*1..3]->(b) RETURN count(b)') AS out)
SELECT CASE WHEN instr(json_extract(out, '$[0].column_0'), 'FROM edges e WHERE e.type = ''R'' AND e.source_id IN (SELECT node_id FROM (SELECT t.node_id FROM node_props_text t') > 0
            THEN 'PASS: anchored' ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT cypher('MATCH (n) DETACH DELETE n') AS cleanup;
SELECT '=== Test 40 Complete ===' as test_section;
