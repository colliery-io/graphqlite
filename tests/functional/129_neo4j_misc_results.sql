-- ========================================================================
-- Test 129: wrong results found by the Neo4j acceptance features
--           (GQLITE-T-0519)
-- ========================================================================
-- PURPOSE: * =~ matches the whole string ('1_.*' matched '21_val');
--          * a CASE holding an aggregate aggregates (it was a grouping
--            key, so all rows fell in one group);
--          * a node held in a map, used in a MATCH pattern;
--          * a boolean expression as an inline property value in MATCH;
--          * EXISTS { () } (an empty WHERE was invalid SQL);
--          * a pattern comprehension inside another one;
--          * SKIP / LIMIT with an expression;
--          * trim() removes Unicode whitespace too.
--          Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 129: Neo4j misc results ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('UNWIND range(1, 30) AS x CREATE (:U {p: x + ''_v''})');
INSERT INTO _assert SELECT 'regex matches the whole string',
    json(cypher('MATCH (c:U) WHERE c.p =~ ''1_.*'' OR c.p =~ ''11_.*'' RETURN c.p AS p ORDER BY p'))
        = json('[{"p":"11_v"},{"p":"1_v"}]')
    AND json(cypher('RETURN ''abc'' =~ ''b'' AS a, ''abc'' =~ ''.*b.*'' AS b, ''ABC'' =~ ''(?i)abc'' AS c'))
        = json('[{"a":false,"b":true,"c":true}]');
INSERT INTO _assert SELECT 'SKIP and LIMIT expressions',
    json(cypher('MATCH (c:U) RETURN c.p AS p ORDER BY p SKIP reduce(s = 0, x IN [1, 27] | s + x) LIMIT 1 + 1'))
        = json('[{"p":"8_v"},{"p":"9_v"}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'CASE with aggregates groups by the key',
    json(cypher('UNWIND [[''a'', 1], [''a'', 2], [''b'', 5]] AS l WITH l[0] AS k, l[1] AS v RETURN k, CASE k WHEN ''a'' THEN sum(v) ELSE max(v) END AS r ORDER BY k'))
        = json('[{"k":"a","r":3},{"k":"b","r":5}]');

SELECT cypher('CREATE (:Book)-[:HOLDS]->(:Page)-[:HOLDS]->(:Para {words: ''blah''})');
INSERT INTO _assert SELECT 'a node held in a map, in MATCH',
    json(cypher('MATCH (a:Page) WITH {k: a} AS map WITH map.k AS nd MATCH (p)<-[:HOLDS]-(nd) RETURN p.words AS w'))
        = json('[{"w":"blah"}]')
    AND json(cypher('MATCH (a:Page) WITH [{node: a}] AS ns UNWIND ns AS m WITH m.node AS nd MATCH (nd)-->(x) RETURN x.words AS w'))
        = json('[{"w":"blah"}]');
INSERT INTO _assert SELECT 'EXISTS { () }',
    json(cypher('RETURN EXISTS { () } AS a, EXISTS { (:Nope) } AS b')) = json('[{"a":true,"b":false}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

SELECT cypher('CREATE (:C {id: 0, x: true}), (:C {id: 1, x: false}), (:C {id: 2, x: 1})');
INSERT INTO _assert SELECT 'boolean expression as an inline property',
    json(cypher('MATCH (x:C {x: (NOT false)}) RETURN x.id AS id')) = json('[{"id":0}]')
    AND json(cypher('MATCH (x:C {x: (1 > 2)}) RETURN x.id AS id')) = json('[{"id":1}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

SELECT cypher('CREATE (:Chicken)-[:rel]->(:Carrot)-[:rel]->(:Ham)');
INSERT INTO _assert SELECT 'nested pattern comprehensions',
    json(cypher('MATCH (c:Chicken) RETURN [(c)--(i1) | [(i1)--(i2) | labels(i2)[0]]] AS p'))
        = json('[{"p":[["Ham","Chicken"]]}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'trim removes Unicode whitespace',
    json(cypher('WITH ''   hi   '' AS s RETURN trim(s) AS t, ltrim(s) AS l, size(rtrim(s)) AS r'))
        = json('[{"t":"hi","l":"hi   ","r":5}]');

SELECT 'Test 129 passed: ' || count(*) || ' assertions' AS result FROM _assert;
