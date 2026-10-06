-- ========================================================================
-- Test 47: timestamp() is integer milliseconds on every platform (GQLITE-T-0205)
-- ========================================================================
-- PURPOSE: timestamp() must return a positive integer millisecond count in
--          RETURN, MATCH+SET and MERGE ON CREATE SET. The former Julian-day
--          floating-point form evaluated to 0 on the Windows CI runner.
-- COVERS:  GQLITE-T-0205.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 47: timestamp() portability ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- 2026-01-01T00:00:00Z in ms, as a lower bound; integer typed.
INSERT INTO _assert SELECT 'T-0205 1.1 RETURN timestamp() > 2026-01-01',
    json_extract(cypher('RETURN timestamp() AS ts'), '$[0].ts') > 1767225600000;
INSERT INTO _assert SELECT 'T-0205 1.2 RETURN timestamp() is integer',
    json_type(cypher('RETURN timestamp() AS ts'), '$[0].ts') = 'integer';
-- Within a second of SQLite's own clock.
INSERT INTO _assert SELECT 'T-0205 1.3 timestamp() agrees with strftime',
    abs(json_extract(cypher('RETURN timestamp() AS ts'), '$[0].ts') - CAST(strftime('%s','now') AS INTEGER) * 1000) < 2000;

SELECT cypher('CREATE (:TS47)') as setup;
INSERT INTO _assert SELECT 'T-0205 2.1 MATCH+SET timestamp()',
    json_extract(cypher('MATCH (n:TS47) SET n.updated = timestamp() RETURN n.updated AS u'), '$[0].u') > 1767225600000;
INSERT INTO _assert SELECT 'T-0205 2.2 MERGE ON CREATE SET timestamp()',
    json_extract(cypher('MERGE (m:TS47B) ON CREATE SET m.created = timestamp() RETURN m.created AS c'), '$[0].c') > 1767225600000;

SELECT name, ok FROM _assert;
SELECT '=== Test 47 Complete ===' as test_section;
