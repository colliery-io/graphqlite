-- ========================================================================
-- Test 42: correctness bugs found during the performance review
-- ========================================================================
-- PURPOSE: 1. ORDER BY on a RETURN alias that shadows one of our own SQL
--             column names (id, type, value, ...) must sort by the projected
--             expression, not by the base table column it would otherwise
--             bind to inside _gql_order_rank().
--          2. Control characters in a stored string must be escaped on the
--             way out of the scalar result path, not replaced by a space.
--          3. \uXXXX in a params JSON object must be decoded, which is how
--             json.dumps() spells every non-ASCII character by default.
-- ASSERT:  json_extract('FAIL', '$') raises "malformed JSON" on mismatch so
--          the -bail runner exits non-zero.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 42: correctness regressions ===' as test_section;

-- Inserted out of order so the internal node ids disagree with the property.
SELECT cypher('CREATE (:B {id: "b3", type: "b3", value: "b3"})') AS setup;
SELECT cypher('CREATE (:B {id: "b1", type: "b1", value: "b1"})') AS setup;
SELECT cypher('CREATE (:B {id: "b2", type: "b2", value: "b2"})') AS setup;

SELECT 'Test 42.1 - ORDER BY alias shadowing the nodes.id column:' as test_name;
WITH r AS (SELECT cypher('MATCH (b:B) RETURN b.id AS id ORDER BY id') AS out)
SELECT CASE WHEN json_extract(out, '$[0].id') = 'b1'
             AND json_extract(out, '$[1].id') = 'b2'
             AND json_extract(out, '$[2].id') = 'b3'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 42.2 - the same alias, DESC:' as test_name;
WITH r AS (SELECT cypher('MATCH (b:B) RETURN b.id AS id ORDER BY id DESC') AS out)
SELECT CASE WHEN json_extract(out, '$[0].id') = 'b3' AND json_extract(out, '$[2].id') = 'b1'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 42.3 - aliases shadowing edges.type and the prop-table value column:' as test_name;
WITH t AS (SELECT cypher('MATCH (b:B) RETURN b.id AS type ORDER BY type') AS o1),
     v AS (SELECT cypher('MATCH (b:B) RETURN b.id AS value ORDER BY value') AS o2)
SELECT CASE WHEN json_extract(o1, '$[0].type') = 'b1' AND json_extract(o1, '$[2].type') = 'b3'
             AND json_extract(o2, '$[0].value') = 'b1' AND json_extract(o2, '$[2].value') = 'b3'
            THEN 'PASS' ELSE json_extract('FAIL: ' || o1 || ' / ' || o2, '$') END AS result FROM t, v;

SELECT 'Test 42.4 - two patterns in scope no longer make the alias ambiguous:' as test_name;
WITH r AS (SELECT cypher('MATCH (a:B), (b:B) WHERE a.id = "b1" RETURN b.id AS id ORDER BY id') AS out)
SELECT CASE WHEN json_valid(out) AND json_array_length(out) = 3
             AND json_extract(out, '$[0].id') = 'b1' AND json_extract(out, '$[2].id') = 'b3'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 42.5 - a non-shadowing alias is unaffected:' as test_name;
WITH r AS (SELECT cypher('MATCH (b:B) RETURN b.id AS bid ORDER BY bid') AS out)
SELECT CASE WHEN json_extract(out, '$[0].bid') = 'b1' AND json_extract(out, '$[2].bid') = 'b3'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 42.6 - control characters are escaped, not blanked, on the scalar path:' as test_name;
SELECT cypher('CREATE (:C {id: "c1"})') AS setup;
WITH w AS (SELECT cypher('MATCH (n:C) SET n.v = $v', json_object('v', 'p' || char(1) || 'q')) AS wrote),
     r AS (SELECT cypher('MATCH (n:C) RETURN n.v AS v') AS out FROM w)
SELECT CASE WHEN json_valid(out) AND out LIKE '%\u0001%' AND out NOT LIKE '%p q%'
             AND json_extract(out, '$[0].v') = 'p' || char(1) || 'q'
            THEN 'PASS' ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 42.7 - \uXXXX escapes in params are decoded to UTF-8:' as test_name;
WITH r AS (SELECT cypher('RETURN $s AS s', '{"s": "caf\u00e9 \u4e2d"}') AS out)
SELECT CASE WHEN json_valid(out) AND json_extract(out, '$[0].s') = 'café 中'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 42.8 - a surrogate pair in params becomes one code point:' as test_name;
WITH r AS (SELECT cypher('RETURN size($s) AS n', '{"s": "\ud83d\ude00"}') AS out)
SELECT CASE WHEN json_extract(out, '$[0].n') = 1
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT '=== Test 42 complete ===' as test_section;
