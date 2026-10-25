-- ========================================================================
-- Test 69: UNWIND keeps the variables in scope (GQLITE-T-0457)
-- ========================================================================
-- PURPOSE: UNWIND [] and UNWIND null built a CTE that did not carry the
--          prior variables ("no such column: _prev.a", "`a` is not
--          defined"). An UNWIND after an OPTIONAL MATCH with an anonymous
--          relationship spliced its source into the EXISTS subquery of the
--          join ("no such column: json_each.value"): the splice looked for
--          the first " WHERE ", not the top-level one. Found by the
--          metamorphic harness (COLLIERY-I-0658).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 69: UNWIND carries variables ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (n:I69 {v: 2, s: ''x WHERE y''})-[:R69]->(m:J69 {v: 3})') as setup;

INSERT INTO _assert SELECT 'UNWIND [] after MATCH: no rows',
    json(cypher('MATCH (a:I69) UNWIND [] AS i RETURN a.v AS v')) = json('[]');
INSERT INTO _assert SELECT 'UNWIND null after MATCH: no rows',
    json(cypher('MATCH (a:I69) UNWIND null AS i RETURN a.v AS v')) = json('[]');
INSERT INTO _assert SELECT 'UNWIND [] then aggregate over a MATCH variable',
    json(cypher('MATCH (a:I69) UNWIND [] AS i RETURN count(*) AS c, collect(a.v) AS l')) = json('[{"c":0,"l":[]}]');
INSERT INTO _assert SELECT 'UNWIND [] after WITH DISTINCT',
    json(cypher('MATCH (a)-[r]->(b) WITH DISTINCT a, b UNWIND [] AS i RETURN count(*) AS c, collect(b.v) AS l')) = json('[{"c":0,"l":[]}]');
INSERT INTO _assert SELECT 'UNWIND [] after an aggregating WITH',
    json(cypher('MATCH (a)-[:R69]-(b) WITH b, a, collect(a.v) AS w UNWIND [] AS i RETURN collect(DISTINCT b.s) AS c0')) = json('[{"c0":[]}]');
INSERT INTO _assert SELECT 'UNWIND [] after OPTIONAL MATCH',
    json(cypher('MATCH (a:I69) OPTIONAL MATCH (a)-[p]-(d) UNWIND [] AS i RETURN count(*) AS c, collect(d.v) AS l')) = json('[{"c":0,"l":[]}]');
INSERT INTO _assert SELECT 'UNWIND [] then WITH * WHERE on a label',
    json(cypher('MATCH (a:I69)-[r]->(b) UNWIND [] AS i WITH * WHERE a:I69 RETURN i')) = json('[]');
INSERT INTO _assert SELECT 'UNWIND range() after OPTIONAL MATCH with an anonymous relationship',
    json(cypher('MATCH (a)<-[r:R69]-(b) OPTIONAL MATCH (a)<-[:R69]-(d) UNWIND range(0, coalesce(d.v, 1)) AS i RETURN count(*) AS c, collect(b.v) AS l')) = json('[{"c":3,"l":[2,2,2]}]');
INSERT INTO _assert SELECT 'UNWIND list after OPTIONAL MATCH with an anonymous relationship',
    json(cypher('MATCH (a)<-[r:R69]-(b) OPTIONAL MATCH (a)<-[:R69]-(d) UNWIND [b.v, 1] AS i RETURN i ORDER BY i')) = json('[{"i":1},{"i":2}]');
INSERT INTO _assert SELECT 'WHERE text in a string literal does not move the splice',
    json(cypher('MATCH (a:I69) WHERE a.s = ''x WHERE y'' UNWIND range(1, 2) AS i RETURN a.v AS v, i ORDER BY i')) = json('[{"v":2,"i":1},{"v":2,"i":2}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 69 Complete ===' as test_section;
