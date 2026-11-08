-- ========================================================================
-- Test 136: DELETE and REMOVE see the graph as it was before the clause;
--           REMOVE (expr).prop (GQLITE-T-0524)
-- ========================================================================
-- PURPOSE: A DELETE evaluated each row after the deletes of the rows
--          before it. REMOVE (expr).prop did not parse. WITH x AS i
--          ORDER BY i RETURN collect(i) lost the order. Neo4j acceptance
--          scenarios (DeleteAcceptance, RemoveAcceptance); the DELETE
--          item-by-item one in openCypher, without COLLECT { }.
--          Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 136: DELETE / REMOVE visibility ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- WITH ... ORDER BY an alias, then an aggregate
SELECT cypher('CREATE (:C {id: 1}), (:C {id: 0}), (:C {id: 2})');
INSERT INTO _assert SELECT 'WITH ORDER BY alias before collect',
    json(cypher('MATCH (o:C) WITH o.id AS i ORDER BY i DESC RETURN collect(i) AS l'))
        = json('[{"l":[2,1,0]}]');
SELECT cypher('MATCH (n:C) DELETE n');

-- DELETE, row-by-row
SELECT cypher('CREATE (:D)-[:REL]->(:D)<-[:REL]-(:D)');
INSERT INTO _assert SELECT 'DELETE row-by-row',
    json_extract(cypher('MATCH (n:D)-->() DETACH DELETE [(n)--()--() | n][0]'), '$.nodes_deleted') = 2;
INSERT INTO _assert SELECT 'DELETE row-by-row left one node',
    json(cypher('MATCH (n:D) RETURN count(n) AS c')) = json('[{"c":1}]');
SELECT cypher('MATCH (n:D) DETACH DELETE n');

-- DELETE, item-by-item: the second item is n (id 0) while n is there and
-- m once it is gone. Seen before the clause, only n is deleted.
SELECT cypher('CREATE (:E {id: 0}), (:E {id: 1})');
INSERT INTO _assert SELECT 'DELETE item-by-item',
    json_extract(cypher('MATCH (n:E {id: 0}), (m:E {id: 1}) DELETE n, head([x IN [n, m] WHERE x.id = CASE WHEN EXISTS { MATCH (q:E {id: 0}) } THEN 0 ELSE 1 END])'), '$.nodes_deleted') = 1;
INSERT INTO _assert SELECT 'DELETE item-by-item kept m',
    json(cypher('MATCH (n:E) RETURN n.id AS i')) = json('[{"i":1}]');
SELECT cypher('MATCH (n:E) DETACH DELETE n');

-- REMOVE, row-by-row
SELECT cypher('CREATE (:F {prop: true})-[:REL]->(:F)<-[:REL]-(:F {prop: true})');
SELECT cypher('MATCH (n:F)-->() REMOVE ([(n {prop: true})--()--({prop: true}) | n][0]).prop');
INSERT INTO _assert SELECT 'REMOVE row-by-row',
    json(cypher('MATCH (n:F) WHERE n.prop IS NOT NULL RETURN count(n) AS c')) = json('[{"c":0}]');
SELECT cypher('MATCH (n:F) DETACH DELETE n');

-- REMOVE, item-by-item
SELECT cypher('CREATE (:N {prop: true, k: 1}), ({prop: true, k: 2})');
SELECT cypher('MATCH (n:N), (m {k: 2}) REMOVE n:N, (CASE WHEN n:N THEN n ELSE m END).prop');
INSERT INTO _assert SELECT 'REMOVE item-by-item',
    json(cypher('MATCH (n) WHERE n.k IS NOT NULL RETURN n.k AS k, labels(n) AS l, n.prop AS p ORDER BY k'))
        = json('[{"k":1,"l":[],"p":null},{"k":2,"l":[],"p":true}]');

SELECT 'Test 136 passed: ' || count(*) || ' assertions' AS result FROM _assert;
