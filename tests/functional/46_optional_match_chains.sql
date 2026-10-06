-- ========================================================================
-- Test 46: OPTIONAL MATCH chains with several unbound nodes (GQLITE-T-0336)
-- ========================================================================
-- PURPOSE: A multi-relationship OPTIONAL MATCH whose intermediate and end
--          nodes are all unbound is matched as one all-or-none chain, and a
--          WHERE on the OPTIONAL MATCH nulls the whole chain. A bound
--          relationship re-matched with two fresh endpoints keeps the anchor
--          row when the WHERE fails.
-- COVERS:  openCypher TCK MatchWhere6 [5] and [7]; GQLITE-T-0336 (P3).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 46: OPTIONAL MATCH chains ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:X46 {val: 1})-[:E1]->(:Y46 {val: 2})-[:E2]->(:Z46 {val: 3}), (:X46 {val: 4})-[:E1]->(:Y46 {val: 5}), (:X46 {val: 6})') as setup;

-- Two-rel chain, both y and z unbound: used to fail with
-- "ON clause references tables to its right".
INSERT INTO _assert SELECT 'T-0336 1.1 chain rows',
    json(cypher('MATCH (x:X46) OPTIONAL MATCH (x)-[:E1]->(y:Y46)-[:E2]->(z:Z46) WHERE x.val < z.val RETURN x.val AS x, y.val AS y, z.val AS z ORDER BY x'))
    = json('[{"x":1,"y":2,"z":3},{"x":4,"y":null,"z":null},{"x":6,"y":null,"z":null}]');

-- A WHERE that fails on the complete chain nulls y AND z (all-or-none).
INSERT INTO _assert SELECT 'T-0336 1.2 all-or-none WHERE',
    json(cypher('MATCH (x:X46) OPTIONAL MATCH (x)-[:E1]->(y:Y46)-[:E2]->(z:Z46) WHERE z.val > 100 RETURN x.val AS x, y.val AS y, z.val AS z ORDER BY x'))
    = json('[{"x":1,"y":null,"z":null},{"x":4,"y":null,"z":null},{"x":6,"y":null,"z":null}]');

-- Without WHERE the partial chain (x=4 has E1 but no E2) still yields nulls.
INSERT INTO _assert SELECT 'T-0336 1.3 partial chain is null',
    json(cypher('MATCH (x:X46) OPTIONAL MATCH (x)-[:E1]->(y:Y46)-[:E2]->(z:Z46) RETURN x.val AS x, y.val AS y, z.val AS z ORDER BY x'))
    = json('[{"x":1,"y":2,"z":3},{"x":4,"y":null,"z":null},{"x":6,"y":null,"z":null}]');

-- Bound relationship, both endpoints fresh, WHERE correlates to the outer
-- node: the anchor row survives with the optional vars null.
SELECT cypher('CREATE (:A46)-[:T46]->(:B46)') as setup2;
INSERT INTO _assert SELECT 'T-0336 2.1 bound rel reverse keeps anchor row',
    json_array_length(cypher('MATCH (a1:A46)-[r:T46]->() WITH r, a1 LIMIT 1 OPTIONAL MATCH (a2)<-[r]-(b2) WHERE a1 = a2 RETURN a1, r, b2, a2')) = 1;
INSERT INTO _assert SELECT 'T-0336 2.2 bound rel reverse optional vars null',
    json_extract(cypher('MATCH (a1:A46)-[r:T46]->() WITH r, a1 LIMIT 1 OPTIONAL MATCH (a2)<-[r]-(b2) WHERE a1 = a2 RETURN a1, r, b2, a2'), '$[0].b2') IS NULL
    AND json_extract(cypher('MATCH (a1:A46)-[r:T46]->() WITH r, a1 LIMIT 1 OPTIONAL MATCH (a2)<-[r]-(b2) WHERE a1 = a2 RETURN a1, r, b2, a2'), '$[0].a2') IS NULL
    AND json_extract(cypher('MATCH (a1:A46)-[r:T46]->() WITH r, a1 LIMIT 1 OPTIONAL MATCH (a2)<-[r]-(b2) WHERE a1 = a2 RETURN a1, r, b2, a2'), '$[0].a1.labels[0]') = 'A46';

SELECT name, ok FROM _assert;
SELECT '=== Test 46 Complete ===' as test_section;
