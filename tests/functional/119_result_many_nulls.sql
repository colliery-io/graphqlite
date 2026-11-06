-- ========================================================================
-- Test 119: results with many null cells (GQLITE-T-0496)
-- ========================================================================
-- PURPOSE: the result writer sized its buffer from the non-null cells, so
--          a result with many null cells wrote past the buffer (a heap
--          overflow found by the fuzzer). The writer now grows as it
--          writes. Under `angreal test sanitize` an overflow is a report.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 119: many null cells ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('UNWIND range(1, 50) AS i CREATE (:N119 {i: i})') as setup;

INSERT INTO _assert SELECT 'one null column, many rows',
    json_array_length(cypher('UNWIND range(1, 3000) AS i RETURN null AS x')) = 3000;
INSERT INTO _assert SELECT 'one null column after MATCH',
    json_array_length(cypher('MATCH (n:N119), (m:N119) RETURN null AS x')) = 2500;
INSERT INTO _assert SELECT 'the last row is whole',
    json_extract(cypher('UNWIND range(1, 3000) AS i RETURN i AS a, null AS b, null AS c'), '$[2999]')
    = json('{"a":3000,"b":null,"c":null}');
INSERT INTO _assert SELECT 'node and null columns',
    json_array_length(cypher('MATCH (n:N119), (m:N119) RETURN n, null AS x')) = 2500;

SELECT 'Test 119 passed: ' || count(*) || ' assertions' AS result FROM _assert;
