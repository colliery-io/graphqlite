-- ========================================================================
-- Test 149: WHERE inside a node or relationship pattern (GQLITE-T-0538)
-- ========================================================================
-- PURPOSE: (n WHERE ...) and -[r WHERE ...]- did not parse ("unexpected
--          WHERE, expecting ')'"). The parser now takes the predicate of
--          an element and puts it in the WHERE of the clause, subquery or
--          pattern comprehension that holds the pattern.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 149: WHERE inside a pattern ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:P149 {name: ''A'', age: 40})-[:K149 {w: 2}]->(b:P149 {name: ''B'', age: 20}),
                      (a)-[:K149 {w: 1}]->(c:P149 {name: ''C'', age: 35}), (b)-[:K149 {w: 5}]->(c)') as setup;

INSERT INTO _assert SELECT 'node WHERE in MATCH',
    json(cypher('MATCH (a:P149 WHERE a.age > 30) RETURN a.name AS n ORDER BY n'))
    = json('[{"n":"A"},{"n":"C"}]');

INSERT INTO _assert SELECT 'node WHERE after inline properties',
    json(cypher('MATCH (a:P149 {name: ''A''} WHERE a.age = 40) RETURN a.name AS n'))
    = json('[{"n":"A"}]');

INSERT INTO _assert SELECT 'relationship WHERE in MATCH',
    json(cypher('MATCH (a:P149)-[r WHERE r.w > 1]->(b) RETURN a.name AS a, b.name AS b ORDER BY a'))
    = json('[{"a":"A","b":"B"},{"a":"B","b":"C"}]');

INSERT INTO _assert SELECT 'three elements, each with a WHERE',
    json(cypher('MATCH (a:P149 WHERE a.age > 30)-[r:K149 WHERE r.w < 2]->(b WHERE b.age > 30) RETURN a.name AS a, b.name AS b'))
    = json('[{"a":"A","b":"C"}]');

INSERT INTO _assert SELECT 'element WHERE and clause WHERE',
    json(cypher('MATCH (a:P149 WHERE a.age > 30)-[r]->(b) WHERE r.w > 1 OR b.name = ''Z'' RETURN a.name AS a, b.name AS b'))
    = json('[{"a":"A","b":"B"}]');

INSERT INTO _assert SELECT 'variable-length relationship with WHERE',
    json(cypher('MATCH (a:P149 WHERE a.name = ''A'')-[:K149*1..2 WHERE true]->(b) RETURN count(*) AS c'))
    = json('[{"c":3}]');

INSERT INTO _assert SELECT 'OPTIONAL MATCH: the WHERE is part of the optional pattern',
    json(cypher('MATCH (a:P149) OPTIONAL MATCH (a)-[r WHERE r.w > 4]->(b) RETURN a.name AS n, b.name AS m ORDER BY n'))
    = json('[{"n":"A","m":null},{"n":"B","m":"C"},{"n":"C","m":null}]');

INSERT INTO _assert SELECT 'COUNT { (p WHERE ...) }',
    json(cypher('MATCH (p:P149) RETURN p.name AS n, COUNT { (p WHERE p.age > 30) } AS c ORDER BY n'))
    = json('[{"n":"A","c":1},{"n":"B","c":0},{"n":"C","c":1}]');

INSERT INTO _assert SELECT 'a subquery inside the node WHERE',
    json(cypher('MATCH (n:P149 WHERE COUNT { MATCH (n)-[]->(:P149) } > 1) RETURN n.name AS name'))
    = json('[{"name":"A"}]');

INSERT INTO _assert SELECT 'EXISTS { pattern } with a relationship WHERE',
    json(cypher('MATCH (a:P149) WHERE EXISTS { (a)-[r WHERE r.w > 4]->() } RETURN a.name AS n'))
    = json('[{"n":"B"}]');

INSERT INTO _assert SELECT 'exists(pattern) with element WHEREs',
    json(cypher('MATCH (a:P149) WHERE exists((a)-[:K149 WHERE true]->(:P149 WHERE false)) RETURN a.name AS n'))
    = json('[]');

INSERT INTO _assert SELECT 'pattern predicate with a node WHERE',
    json(cypher('MATCH (a:P149) WHERE (a)-[:K149]->(:P149 {name: ''C''} WHERE true) RETURN a.name AS n ORDER BY n'))
    = json('[{"n":"A"},{"n":"B"}]');

INSERT INTO _assert SELECT 'pattern comprehension: relationship WHERE',
    json(cypher('MATCH (a:P149) RETURN a.name AS n, [(a)-[r WHERE r.w > 1]->(b) | b.name] AS l ORDER BY n'))
    = json('[{"n":"A","l":["B"]},{"n":"B","l":["C"]},{"n":"C","l":[]}]');

INSERT INTO _assert SELECT 'pattern comprehension: node WHERE and its own WHERE',
    json(cypher('MATCH (a:P149) RETURN a.name AS n, [(a WHERE a.age > 30)-[r]->(b) WHERE b.age > 30 | b.name] AS l ORDER BY n'))
    = json('[{"n":"A","l":["C"]},{"n":"B","l":[]},{"n":"C","l":[]}]');

INSERT INTO _assert SELECT 'a list comprehension WHERE still parses next to it',
    json(cypher('MATCH (a:P149 WHERE a.age > 39) RETURN [y IN [1, 2, 3] WHERE y > 1] AS l, a.name AS n'))
    = json('[{"l":[2,3],"n":"A"}]');

INSERT INTO _assert SELECT 'CREATE refuses a pattern WHERE',
    json_extract(cypher_validate('CREATE (x:Z149 WHERE x.a = 1)'), '$.valid') = 0;
INSERT INTO _assert SELECT 'MERGE refuses a pattern WHERE',
    json_extract(cypher_validate('MERGE (x:Z149 WHERE x.a = 1)'), '$.valid') = 0;

SELECT 'Test 149 passed: ' || count(*) || ' assertions' AS result FROM _assert;
