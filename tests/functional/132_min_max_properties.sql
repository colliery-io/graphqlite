-- ========================================================================
-- Test 132: min() / max() of a property keep its type (GQLITE-T-0529)
-- ========================================================================
-- PURPOSE: min() and max() of a property took the numeric shortcut of
--          sum() / avg(): a string was cast to REAL (max of 'z' and 'a'
--          gave 0.0) and booleans were not read (null). Found by the
--          differential test against Neo4j. Assertions are hard: _assert
--          has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 132: min / max of properties ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE ({id: 0, b: false, s: ''z'', v: 3, f: 1.5}), ({id: 1, b: true, s: ''a'', v: 10, f: -2.0}), ({id: 2})');

INSERT INTO _assert SELECT 'strings, booleans, numbers',
    json(cypher('MATCH (h) RETURN max(h.s) AS ms, min(h.s) AS ns, max(h.b) AS mb, min(h.b) AS nb, max(h.v) AS mv, min(h.f) AS nf'))
        = json('[{"ms":"z","ns":"a","mb":true,"nb":false,"mv":10,"nf":-2.0}]');
INSERT INTO _assert SELECT 'sum and avg unchanged',
    json(cypher('MATCH (h) RETURN sum(h.v) AS sv, avg(h.f) AS af')) = json('[{"sv":13,"af":-0.25}]');
INSERT INTO _assert SELECT 'grouped',
    json(cypher('MATCH (h) WHERE h.s IS NOT NULL RETURN h.id % 2 AS k, max(h.s) AS m ORDER BY k'))
        = json('[{"k":0,"m":"z"},{"k":1,"m":"a"}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

SELECT cypher('CREATE ()-[:R {w: ''b''}]->(), ()-[:R {w: ''c''}]->()');
INSERT INTO _assert SELECT 'relationship property',
    json(cypher('MATCH ()-[r:R]->() RETURN max(r.w) AS m, min(r.w) AS n')) = json('[{"m":"c","n":"b"}]');

SELECT 'Test 132 passed: ' || count(*) || ' assertions' AS result FROM _assert;
