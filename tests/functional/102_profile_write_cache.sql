-- ========================================================================
-- Test 102: PROFILE of a write marks the cached graph stale (GQLITE-T-0444)
-- ========================================================================
-- PURPOSE: PROFILE put the run's write counts in its row but not on the
--          result, where the executor looks for writes, so after
--          gql_load_graph() a PROFILE CREATE left the cached algorithm graph
--          as it was: CALL pageRank() did not see the new nodes. The counts
--          are now on the result too.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 102: PROFILE write and the graph cache ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A102 {id: 1})-[:R102]->(:A102 {id: 2})') as setup;
SELECT gql_load_graph() as loaded;
INSERT INTO _assert SELECT 'two nodes before',
    json_array_length(cypher('CALL pageRank()')) = 2;
INSERT INTO _assert SELECT 'the PROFILE row has the writes',
    json_extract(cypher('PROFILE CREATE (:A102 {id: 3})-[:R102]->(:A102 {id: 4})'), '$[0].nodes_created') = 2;
INSERT INTO _assert SELECT 'the algorithm sees the new nodes',
    json_array_length(cypher('CALL pageRank()')) = 4;

SELECT 'Test 102 passed: ' || count(*) || ' assertions' AS result FROM _assert;
