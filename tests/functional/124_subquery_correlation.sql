-- ========================================================================
-- Test 124: EXISTS / COUNT { } and the outer variables (GQLITE-T-0513)
-- ========================================================================
-- PURPOSE: the subquery lost the outer variable after WITH p AS person,
--          when its body opened with WITH or UNWIND, and in SET; a leading
--          OPTIONAL MATCH with an inline map gave no row instead of a null
--          row; toString(EXISTS {...}) gave "1"; CREATE ... RETURN gave
--          booleans as strings.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 124: subquery correlation ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:P124 {name: ''Bosse''})-[:HAS_DOG]->(:D124 {name: ''Bosse''}), (:P124 {name: ''Chris''})-[:HAS_DOG]->(:D124 {name: ''Ozzy''}), (:P124 {name: ''Alice''})') as setup;

INSERT INTO _assert SELECT 'after WITH p AS person',
    json(cypher('MATCH (p:P124) WITH p AS person WHERE EXISTS { MATCH (person)-[:HAS_DOG]->(d:D124) WHERE person.name = d.name } RETURN person.name AS name'))
    = json('[{"name":"Bosse"}]');
INSERT INTO _assert SELECT 'a body opening with WITH',
    json(cypher('MATCH (person:P124) WHERE EXISTS { WITH count(*) AS c MATCH (person)-[:HAS_DOG]->(d:D124) WHERE d.name = ''Ozzy'' RETURN person } RETURN person.name AS name'))
    = json('[{"name":"Chris"}]');
INSERT INTO _assert SELECT 'a body opening with UNWIND',
    json(cypher('MATCH (person:P124) WHERE EXISTS { UNWIND [''Ozzy'', ''Fido''] AS n MATCH (person)-[:HAS_DOG]->(d:D124) WHERE d.name = n RETURN n } RETURN person.name AS name'))
    = json('[{"name":"Chris"}]');
INSERT INTO _assert SELECT 'leading OPTIONAL MATCH with an inline map',
    json(cypher('OPTIONAL MATCH (p:P124 {name: ''Charlie''}) RETURN p.name AS name')) = json('[{"name":null}]')
    AND json(cypher('OPTIONAL MATCH (p:P124 {name: ''Alice''}) WHERE EXISTS { MATCH (p)-[:HAS_DOG]->() } RETURN p.name AS name')) = json('[{"name":null}]');
INSERT INTO _assert SELECT 'toString of EXISTS',
    json(cypher('MATCH (p:P124) WHERE toString(EXISTS { MATCH (p)-[:HAS_DOG]->() }) = ''true'' RETURN p.name AS name ORDER BY name'))
    = json('[{"name":"Bosse"},{"name":"Chris"}]');
INSERT INTO _assert SELECT 'COUNT and EXISTS in SET',
    json(cypher('MATCH (p:P124) SET p.n = COUNT { (p)-[:HAS_DOG]->() }, p.e = EXISTS { (p)-[:HAS_DOG]->() } RETURN p.name AS name, p.n AS n, p.e AS e ORDER BY name'))
    = json('[{"name":"Alice","n":0,"e":false},{"name":"Bosse","n":1,"e":true},{"name":"Chris","n":1,"e":true}]');
INSERT INTO _assert SELECT 'CREATE ... RETURN keeps booleans and strings',
    json(cypher('CREATE (c:C124 {x: 1 = 1, s: ''7''}) RETURN c.x AS x, c.s AS s')) = json('[{"x":true,"s":"7"}]');

SELECT 'Test 124 passed: ' || count(*) || ' assertions' AS result FROM _assert;
