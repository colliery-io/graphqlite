-- ========================================================================
-- Test 134: RETURN * after a write; the UNWIND value after CREATE
--           (GQLITE-T-0522)
-- ========================================================================
-- PURPOSE: RETURN * after CREATE, SET, MERGE or DELETE returned only the
--          statistics or [] (the write handlers read the items, which
--          RETURN * left empty); after UNWIND ... CREATE it failed
--          ("missing clause"). And RETURN of the UNWIND variable after
--          UNWIND ... CREATE gave null. Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 134: RETURN * after a write ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'UNWIND CREATE RETURN *',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:Q {c: i}) RETURN *')) IS NOT NULL
    AND json_array_length(cypher('MATCH (n:Q) RETURN n')) = 2;
INSERT INTO _assert SELECT 'UNWIND value in RETURN after CREATE',
    json(cypher('UNWIND [3, 4] AS i CREATE (n:R {c: i}) RETURN i, n.c AS c'))
        = json('[{"i":3,"c":3},{"i":4,"c":4}]');
INSERT INTO _assert SELECT 'RETURN * columns after UNWIND CREATE',
    (SELECT group_concat(key) FROM json_each(json_extract(cypher('UNWIND [5] AS i CREATE (n:S {c: i}) RETURN *'), '$[0]')))
        = 'i,n';
INSERT INTO _assert SELECT 'CREATE RETURN *',
    json_extract(cypher('CREATE (a:Y {v: 1})-[r:T]->(b:Y {v: 2}) RETURN *'), '$[0].r.type') = 'T';
INSERT INTO _assert SELECT 'SET RETURN *',
    json(cypher('MATCH (n:R) SET n.d = n.c * 2 RETURN * ORDER BY n.c')) IS NOT NULL
    AND json_array_length(cypher('MATCH (n:R) SET n.e = 1 RETURN *')) = 2;
INSERT INTO _assert SELECT 'MERGE RETURN *',
    json_extract(cypher('MATCH (n:S) MERGE (n)-[r:M]->(m:S2) RETURN *'), '$[0].m.labels[0]') = 'S2';

SELECT 'Test 134 passed: ' || count(*) || ' assertions' AS result FROM _assert;
