-- ========================================================================
-- Test 44: list / pattern comprehension over collected entities (T-0370)
-- ========================================================================
-- PURPOSE: 1. [x IN collect(n) | x.prop] and [x IN collect(n) WHERE x.prop]
--             read the entity JSON that collect() stores.
--          2. [p IN collect(path) | nodes(p)] / relationships(p): a path that
--             is a list element (not a registered path variable) is decoded
--             from its stored encoding and hydrated.
--          3. A pattern comprehension whose bound node is a list-comprehension
--             element ([x IN nodes(p) | size([(x)-->() | 1])]), and an
--             undirected pattern comprehension (n)--().
--          4. Write path: a SET after WITH/UNWIND over collect(n) applies, and
--             values projected before the SET keep their pre-write state.
-- ASSERT:  json_extract('FAIL', '$') raises "malformed JSON" on mismatch so
--          the -bail runner exits non-zero.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 44: comprehensions over collected entities ===' as test_section;

SELECT cypher('CREATE (:L44 {name: "a", v: 1}), (:L44 {name: "b", v: 2}), (:L44 {name: "c", v: 3})') AS setup;
SELECT cypher('MATCH (a:L44 {name: "a"}), (b:L44 {name: "b"}), (c:L44 {name: "c"}) CREATE (a)-[:R44 {w: 1}]->(b), (a)-[:R44 {w: 2}]->(c)') AS setup;

SELECT 'Test 44.1 - [x IN collect(n) | x.prop]:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:L44) WITH collect(n) AS ns RETURN [x IN ns | x.name] AS names') AS out)
SELECT CASE WHEN json_array_length(out, '$[0].names') = 3
             AND (SELECT group_concat(value, '') FROM (SELECT value FROM json_each(out, '$[0].names') ORDER BY value)) = 'abc'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.2 - [x IN collect(n) WHERE x.prop = v | x] keeps whole entities:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:L44) WITH collect(n) AS ns RETURN [x IN ns WHERE x.v > 1 | x] AS big') AS out)
SELECT CASE WHEN json_array_length(out, '$[0].big') = 2
             AND json_extract(out, '$[0].big[0].labels[0]') = 'L44'
             AND (SELECT min(json_extract(value, '$.properties.v')) FROM json_each(out, '$[0].big')) = 2
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.3 - [p IN collect(path) | nodes(p)] hydrates the path nodes:' as test_name;
WITH r AS (SELECT cypher('MATCH path = (a:L44 {name: "a"})-[:R44]->(m) RETURN [p IN collect(path) | nodes(p)] AS ns') AS out)
SELECT CASE WHEN json_array_length(out, '$[0].ns') = 2
             AND json_array_length(out, '$[0].ns[0]') = 2
             AND json_extract(out, '$[0].ns[0][0].properties.name') = 'a'
             AND json_extract(out, '$[0].ns[1][0].properties.name') = 'a'
             AND json_extract(out, '$[0].ns[0][1].labels[0]') = 'L44'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.4 - [p IN collect(path) | relationships(p)] hydrates the rels:' as test_name;
WITH r AS (SELECT cypher('MATCH path = (a:L44 {name: "a"})-[:R44]->(m) RETURN [p IN collect(path) | relationships(p)] AS rs') AS out)
SELECT CASE WHEN json_array_length(out, '$[0].rs') = 2
             AND json_extract(out, '$[0].rs[0][0].type') = 'R44'
             AND (SELECT sum(json_extract(value, '$[0].properties.w')) FROM json_each(out, '$[0].rs')) = 3
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.5 - nodes(q) on an unwound collected path, read through a comprehension:' as test_name;
WITH r AS (SELECT cypher('MATCH path = (a:L44 {name: "a"})-->() WITH collect(path) AS ps UNWIND ps AS q RETURN [x IN nodes(q) | x.name] AS names, size(relationships(q)) AS nrel') AS out)
SELECT CASE WHEN json_array_length(out) = 2
             AND json_extract(out, '$[0].names[0]') = 'a' AND json_extract(out, '$[1].names[0]') = 'a'
             AND json_array_length(out, '$[0].names') = 2
             AND json_extract(out, '$[0].nrel') = 1 AND json_extract(out, '$[1].nrel') = 1
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.6 - pattern comprehension inside a list comprehension:' as test_name;
WITH r AS (SELECT cypher('MATCH path = (a:L44 {name: "a"})-->() RETURN [x IN nodes(path) | size([(x)-->(:L44) | 1])] AS cnt') AS out)
SELECT CASE WHEN json_array_length(out) = 2
             AND json_extract(out, '$[0].cnt') = '[2,0]' AND json_extract(out, '$[1].cnt') = '[2,0]'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.7 - undirected pattern comprehension matches both orientations:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:L44) RETURN n.name AS name, size([(n)--() | 1]) AS deg ORDER BY name') AS out)
SELECT CASE WHEN json_extract(out, '$[0].deg') = 2 AND json_extract(out, '$[1].deg') = 1 AND json_extract(out, '$[2].deg') = 1
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.8 - SET after UNWIND over collect(n): values projected before the SET are pre-write:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:L44) WITH collect(n) AS ns WITH ns, [x IN ns | x.v] AS old UNWIND ns AS m SET m.v = m.v * 10 RETURN old, m.v AS cur ORDER BY cur') AS out)
SELECT CASE WHEN json_array_length(out) = 3
             AND (SELECT sum(value) FROM json_each(out, '$[0].old')) = 6
             AND json_extract(out, '$[0].cur') = 10 AND json_extract(out, '$[2].cur') = 30
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.9 - the write persisted (and a new key can be SET after UNWIND):' as test_name;
WITH w AS (SELECT cypher('MATCH (n:L44) WITH collect(n) AS ns UNWIND ns AS m SET m.fresh = "yes" RETURN count(*) AS c') AS wrote),
     r AS (SELECT cypher('MATCH (n:L44) RETURN sum(n.v) AS total, count(n.fresh) AS fresh') AS out FROM w)
SELECT CASE WHEN json_extract(out, '$[0].total') = 60 AND json_extract(out, '$[0].fresh') = 3
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.10 - WITH n, n.prop AS old SET n.prop = ... RETURN old sees the old value:' as test_name;
WITH r AS (SELECT cypher('MATCH (n:L44 {name: "a"}) WITH n, n.v AS old SET n.v = 100 RETURN old, n.v AS cur') AS out)
SELECT CASE WHEN json_extract(out, '$[0].old') = 10 AND json_extract(out, '$[0].cur') = 100
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT '=== Test 44 complete ===' as test_section;
