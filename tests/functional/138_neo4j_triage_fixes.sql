-- ========================================================================
-- Test 138: fixes from the Neo4j acceptance triage (GQLITE-T-0520)
-- ========================================================================
-- PURPOSE: RETURN p of a path from the read prefix of the row pipeline
--          gave its ids. UNWIND of a value that is not a list failed
--          ("malformed JSON"); UNWIND of a CASE was refused. A literal
--          grouping key (RETURN 123, count(*)) was a column number to
--          SQLite. A subquery that matches only outer variables
--          (COUNT { MATCH (p) }, EXISTS { (p:L) }) failed. Assertions
--          are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 138: Neo4j triage fixes ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:P {age: 50})-[:R {w: 2}]->(:X {k: 3}), (:P:T {age: 2})');

-- paths from the prefix
INSERT INTO _assert SELECT 'prefix path after CALL',
    json_extract(cypher('MATCH p = (n:P)-->() CALL { WITH p RETURN length(p) AS l } RETURN p, l'), '$[0].p.rels[0].type') = 'R';
INSERT INTO _assert SELECT 'prefix path after CREATE',
    json(cypher('MATCH p = (n:P)-->() CREATE (:Y) RETURN length(p) AS l, nodes(p)[1].k AS k, relationships(p)[0].w AS w'))
        = json('[{"l":1,"k":3,"w":2}]');

-- UNWIND of a value
INSERT INTO _assert SELECT 'nested UNWIND with a string',
    json(cypher('WITH [[''a'', ''b''], ''e''] AS nested UNWIND nested AS x UNWIND x AS y RETURN y'))
        = json('[{"y":"a"},{"y":"b"},{"y":"e"}]');
INSERT INTO _assert SELECT 'UNWIND a string literal',
    json(cypher('UNWIND ''e'' AS y RETURN y')) = json('[{"y":"e"}]');
INSERT INTO _assert SELECT 'UNWIND null',
    json(cypher('UNWIND null AS y RETURN y')) = json('[]');
INSERT INTO _assert SELECT 'UNWIND a CASE',
    json(cypher('UNWIND CASE WHEN 1 = 1 THEN [1, 2] ELSE 3 END AS y RETURN y')) = json('[{"y":1},{"y":2}]');

-- literal grouping keys
INSERT INTO _assert SELECT 'RETURN literal and count',
    json(cypher('UNWIND [1, 2] AS x RETURN 123 AS k, count(*) AS c ORDER BY 123')) = json('[{"k":123,"c":2}]');
INSERT INTO _assert SELECT 'literal key on empty input',
    json(cypher('MATCH (n:None) RETURN 1 AS k, count(*) AS c')) = json('[]');
INSERT INTO _assert SELECT 'WITH literal and count',
    json(cypher('UNWIND [1, 2] AS x WITH 2 AS k, count(*) AS c RETURN k, c')) = json('[{"k":2,"c":2}]');

-- subqueries over outer variables only
INSERT INTO _assert SELECT 'COUNT { MATCH (p) WHERE }',
    json(cypher('MATCH (p:P) RETURN p.age AS a, COUNT { MATCH (p) WHERE p.age > 3 } AS c ORDER BY a'))
        = json('[{"a":2,"c":0},{"a":50,"c":1}]');
INSERT INTO _assert SELECT 'COUNT { (p {age: 50}) }',
    json(cypher('MATCH (p:P) RETURN p.age AS a, COUNT { (p {age: 50}) } AS c ORDER BY a'))
        = json('[{"a":2,"c":0},{"a":50,"c":1}]');
INSERT INTO _assert SELECT 'EXISTS { (p:T) } in RETURN',
    json(cypher('MATCH (p:P) RETURN p.age AS a, EXISTS { (p:T) } AS e ORDER BY a'))
        = json('[{"a":2,"e":true},{"a":50,"e":false}]');
INSERT INTO _assert SELECT 'EXISTS { (p:T) } in WHERE',
    json(cypher('MATCH (p:P) WHERE EXISTS { (p:T) } RETURN p.age AS a')) = json('[{"a":2}]');

SELECT 'Test 138 passed: ' || count(*) || ' assertions' AS result FROM _assert;
