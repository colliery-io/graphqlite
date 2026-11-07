-- ========================================================================
-- Test 126: CREATE dependencies and evaluation order (GQLITE-T-0516)
-- ========================================================================
-- PURPOSE: Fixes found by the Neo4j acceptance features (Create.feature):
--          * CREATE ... RETURN of a whole node gave its id as a string,
--            and of a relationship null.
--          * type(m) or id(a) in a CREATE property, of an entity created
--            earlier in the same query, was null.
--          * MATCH ()-[r]->(), (c): the second pattern moved the base of
--            the first pattern's anonymous node aliases (no such column).
--          * EXISTS { }, COUNT { } and pattern comprehensions in CREATE
--            properties are evaluated for every row before the CREATE
--            creates anything; a CREATE before it has run for every row.
--          * MERGE with a pattern comprehension in a property wrote null.
--          * toStringOrNull of a node, list or map is null.
--          Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 126: CREATE dependencies ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'CREATE RETURN of a node and a relationship',
    json(cypher('CREATE (a:A {p: 1})-[m:R {q: 2}]->(b) RETURN a, m'))
        = json('[{"a":{"id":1,"labels":["A"],"properties":{"p":1}},"m":{"id":1,"type":"R","startNode":1,"endNode":2,"properties":{"q":2}}}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'type() and a property of a relationship created earlier',
    json_extract(cypher('CREATE (:A)-[m:R {p: ''hello''}]->(:B), (c:C {t: type(m), p: m.p}) RETURN c'),
                 '$[0].c.properties') = json('{"p":"hello","t":"R"}');
INSERT INTO _assert SELECT 'the same over two CREATE clauses',
    json_extract(cypher('CREATE (:A)-[m:R {p: ''hi''}]->(:B) CREATE (c:C {t: type(m), p: m.p}) RETURN c'),
                 '$[0].c.properties') = json('{"p":"hi","t":"R"}');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'endNode(y).x of a relationship created earlier',
    json(cypher('CREATE ({x: 0})<-[y:A]-() CREATE ({x: endNode(y).x}) RETURN COUNT { ({x: 0}) } AS n'))
        = json('[{"n":2}]');

INSERT INTO _assert SELECT 'two comma patterns with anonymous nodes',
    json(cypher('MATCH ()-[m]->(), (c {x: 0}) RETURN count(*) AS n')) = json('[{"n":2}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'COUNT { } is evaluated for every row before the CREATE',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:N {id: i, c: COUNT { MATCH () }}) RETURN n.c AS c'))
        = json('[{"c":0},{"c":0}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'a CREATE before it has run for every row',
    json(cypher('UNWIND [1, 2] AS i CREATE (:A) CREATE (n:B {c: COUNT { MATCH (:A) }}) RETURN n.c AS c'))
        = json('[{"c":2},{"c":2}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'EXISTS { } in a CASE stays a boolean',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:N {p: CASE WHEN true THEN EXISTS { MATCH (:N) } END}) RETURN n.p AS p'))
        = json('[{"p":false},{"p":false}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'pattern comprehension in a relationship property',
    json(cypher('UNWIND [1, 2] AS i CREATE ()-[r:REL {p: size([()-->() | 1])}]->() RETURN r.p AS p'))
        = json('[{"p":0},{"p":0}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'MERGE with a pattern comprehension in a property',
    json(cypher('MERGE (n {k: ([()<-[r0]-() | 1] <= [1])}) RETURN n.k AS k')) = json('[{"k":true}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

INSERT INTO _assert SELECT 'toStringOrNull of a node, list and map is null',
    json(cypher('CREATE (n) RETURN toStringOrNull(n) AS a, toStringOrNull([1]) AS b, toStringOrNull({k: 1}) AS c, toStringOrNull(1.5) AS d, toStringOrNull(true) AS e'))
        = json('[{"a":null,"b":null,"c":null,"d":"1.5","e":"true"}]');

SELECT 'Test 126 passed: ' || count(*) || ' assertions' AS result FROM _assert;
