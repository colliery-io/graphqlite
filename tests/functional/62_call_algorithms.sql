-- ========================================================================
-- Test 62: CALL algo(...) YIELD ... (GQLITE-T-0389)
-- ========================================================================
-- PURPOSE: the graph algorithms as procedures: one row per result object,
--          YIELD selects and renames, YIELD ... WHERE filters, clauses can
--          follow; RETURN algo() AS x names the column.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 62: CALL algorithms ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A62 {id: "a"})-[:R62]->(:A62 {id: "b"})-[:R62]->(:A62 {id: "c"})') as setup;
INSERT INTO _assert SELECT 'one row per result',
    json_array_length(cypher('CALL pageRank() YIELD user_id, score RETURN user_id, score ORDER BY score DESC')) = 3;
INSERT INTO _assert SELECT 'ordered by score',
    json_extract(cypher('CALL pageRank() YIELD user_id, score RETURN user_id ORDER BY score DESC LIMIT 1'), '$[0].user_id') = 'c';
INSERT INTO _assert SELECT 'YIELD AS and WHERE',
    json(cypher('CALL degreeCentrality() YIELD user_id AS u, degree WHERE degree = 2 RETURN u')) = json('[{"u":"b"}]');
INSERT INTO _assert SELECT 'no YIELD gives every key',
    json_extract(cypher('CALL wcc()'), '$[0].component') = 0;
INSERT INTO _assert SELECT 'dijkstra gives one row',
    json(cypher('CALL dijkstra("a", "c") YIELD path, distance')) = json('[{"path":["a","b","c"],"distance":2}]');
SELECT cypher('CALL pageRank() YIELD node_id, score MATCH (n) WHERE id(n) = node_id SET n.rank = score') as write_after_call;
INSERT INTO _assert SELECT 'a write after CALL',
    json_extract(cypher('MATCH (n:A62) WHERE n.rank > 0 RETURN count(n) AS c'), '$[0].c') = 3;
INSERT INTO _assert SELECT 'alias of the function form',
    json_type(cypher('RETURN pageRank() AS pr'), '$[0].pr') = 'array';
INSERT INTO _assert SELECT 'no alias keeps column_0',
    json_type(cypher('RETURN pageRank()'), '$[0].column_0') = 'array';

SELECT name, ok FROM _assert;
SELECT '=== Test 62 Complete ===' as test_section;
