-- ========================================================================
-- Test 51: id() of variables carried through WITH / UNWIND (GQLITE-T-0375)
-- ========================================================================
-- PURPOSE: After a WITH (or an UNWIND of a collected list) a node or edge
--          variable's alias IS its id column; id() must emit that alias and
--          not `<alias>.id` (which produced "no such column: _with_0.n.id").
-- COVERS:  GQLITE-T-0375.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 51: id() after WITH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A51 {k: 1})-[:T51]->(:B51 {k: 2})') as setup;

INSERT INTO _assert SELECT 'T-0375 1.1 id(node) and id(rel) after WITH',
    json(cypher('MATCH (a:A51)-[r:T51]->(b:B51) WITH r, a, b LIMIT 1 RETURN id(a) = id(a) AS same, id(r) IS NOT NULL AS has_r, id(b) <> id(a) AS distinct_nodes'))
    = json('[{"same":true,"has_r":true,"distinct_nodes":true}]');
INSERT INTO _assert SELECT 'T-0375 1.2 id() after UNWIND of collect()',
    json_array_length(cypher('MATCH (n) WHERE n:A51 OR n:B51 WITH collect(n) AS ns UNWIND ns AS m RETURN id(m) AS i')) = 2;
INSERT INTO _assert SELECT 'T-0375 1.3 id() inside WITH ... WHERE',
    json(cypher('MATCH (n:A51) WITH n WHERE id(n) = id(n) RETURN n.k AS k')) = json('[{"k":1}]');
INSERT INTO _assert SELECT 'T-0375 1.4 EXISTS body on a post-WITH variable',
    json(cypher('MATCH (n) WHERE n:A51 OR n:B51 WITH n WHERE exists { MATCH (n)-->() RETURN true } RETURN n.k AS k')) = json('[{"k":1}]');
-- Plain MATCH id() unchanged.
INSERT INTO _assert SELECT 'T-0375 2.1 id() without WITH',
    json_type(cypher('MATCH (n:A51) RETURN id(n) AS i'), '$[0].i') = 'integer';

SELECT name, ok FROM _assert;
SELECT '=== Test 51 Complete ===' as test_section;
