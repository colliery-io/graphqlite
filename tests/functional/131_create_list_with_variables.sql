-- ========================================================================
-- Test 131: a list or map property built from a variable (GQLITE-T-0526)
-- ========================================================================
-- PURPOSE: CREATE stored a list or map literal straight from its AST, so a
--          variable inside it became null: FOREACH (i IN [1, 2] |
--          CREATE ({k: [i, 1]})) stored [null, 1]. Found by the
--          write-batching relation of the metamorphic harness. Assertions
--          are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 131: lists and maps from variables ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('FOREACH (i IN [1, 3] | CREATE (:F {k: [1, i], m: {a: i}, d: [date(''2020-01-01'')]}))');
INSERT INTO _assert SELECT 'FOREACH value in a list and a map',
    json(cypher('MATCH (n:F) RETURN n.k AS k, n.m AS m, n.d AS d ORDER BY k'))
        = json('[{"k":[1,1],"m":{"a":1},"d":["2020-01-01"]},{"k":[1,3],"m":{"a":3},"d":["2020-01-01"]}]');

SELECT cypher('UNWIND [1, 2] AS i CREATE (:U {k: [i, i * 2]})-[:R {l: [i]}]->(:V {m: {x: i}})');
INSERT INTO _assert SELECT 'UNWIND value on nodes and a relationship',
    json(cypher('MATCH (a:U)-[r]->(b) RETURN a.k AS k, r.l AS l, b.m AS m ORDER BY k'))
        = json('[{"k":[1,2],"l":[1],"m":{"x":1}},{"k":[2,4],"l":[2],"m":{"x":2}}]');

INSERT INTO _assert SELECT 'an entity created earlier in the CREATE',
    json(cypher('CREATE (a:W {v: 5}), (:X {l: [a.v, 1]}) RETURN 1 AS one')) = json('[{"one":1}]')
    AND json(cypher('MATCH (x:X) RETURN x.l AS l')) = json('[{"l":[5,1]}]');

SELECT 'Test 131 passed: ' || count(*) || ' assertions' AS result FROM _assert;
