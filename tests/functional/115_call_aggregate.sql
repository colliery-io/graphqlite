-- ========================================================================
-- Test 115: aggregates after CALL { } (GQLITE-T-0490)
-- ========================================================================
-- PURPOSE: after MATCH, CALL { } ran in its own handler, which projected
--          the RETURN one row at a time: count(*), sum() and collect()
--          gave one row per outer row, and outer values lost their types.
--          MATCH ... CALL { } now runs in the row pipeline, as UNWIND ...
--          CALL { } did. A bound value is returned as it is (a string
--          output that looks like JSON stayed a list).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 115: aggregates after CALL ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A115 {b: true, v: 1}), (:B115 {b: false, v: 2})') as setup;

INSERT INTO _assert SELECT 'count and sum after CALL',
    json(cypher('MATCH (n) WHERE n:A115 OR n:B115 CALL { WITH n RETURN n.v AS v } RETURN count(*) AS c, sum(v) AS s'))
    = json('[{"c":2,"s":3}]');
INSERT INTO _assert SELECT 'grouping key after CALL keeps its type',
    json(cypher('MATCH (n) WHERE n:A115 OR n:B115 CALL { WITH n RETURN n.v AS v } RETURN n.b AS k, count(*) AS c ORDER BY k'))
    = json('[{"k":false,"c":1},{"k":true,"c":1}]');
INSERT INTO _assert SELECT 'collect of CALL outputs',
    json(cypher('MATCH (n:A115) CALL { WITH n RETURN n.b AS b } RETURN collect(b) AS bs'))
    = json('[{"bs":[true]}]');
INSERT INTO _assert SELECT 'a string output stays a string',
    json(cypher('UNWIND [1] AS i CALL { WITH i RETURN ''[1]'' AS s } RETURN s'))
    = json('[{"s":"[1]"}]');

SELECT 'Test 115 passed: ' || count(*) || ' assertions' AS result FROM _assert;
