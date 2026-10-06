-- ========================================================================
-- Test 44: large durations and extended-year temporals (GQLITE-T-0369)
-- ========================================================================
-- PURPOSE: openCypher allows years outside 0000..9999 when written with an
--          explicit sign and up to nine digits ('-999999999-01-01'). The
--          durations between such dates exceed int32 months and an int64 of
--          nanoseconds, so months/days/seconds/nanos must be carried as
--          separate fields (TCK Temporal10 [9] and [10]).
-- ASSERT:  json_extract('FAIL', '$') raises "malformed JSON" on mismatch so
--          the -bail runner exits non-zero.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 44: large durations ===' as test_section;

SELECT 'Test 44.1 - duration.between over a two-billion-year span (Temporal10 [9]):' as test_name;
WITH r AS (SELECT cypher('RETURN duration.between(date(''-999999999-01-01''), date(''+999999999-12-31'')) AS d') AS out)
SELECT CASE WHEN json_extract(out, '$[0].d._iso8601') = 'P1999999998Y11M30D'
             AND json_extract(out, '$[0].d.months') = 23999999987
             AND json_extract(out, '$[0].d.days') = 30
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.2 - duration.inSeconds over the same span (Temporal10 [10]):' as test_name;
WITH r AS (SELECT cypher('RETURN duration.inSeconds(localdatetime(''-999999999-01-01''), localdatetime(''+999999999-12-31T23:59:59'')) AS d') AS out)
SELECT CASE WHEN json_extract(out, '$[0].d._iso8601') = 'PT17531639991215H59M59S'
             AND json_extract(out, '$[0].d.seconds') = 63113903968377599
             AND json_extract(out, '$[0].d.nanosecondsOfSecond') = 0
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.3 - extended-year date and datetime literals round-trip:' as test_name;
WITH r AS (SELECT cypher('RETURN date(''-999999999-01-01'') AS a, date(''+10000-01-01'') AS b, date(''-0001-02-03'') AS c, localdatetime(''+999999999-12-31T23:59:59'') AS d, date(''12024-01-01'') AS e') AS out)
SELECT CASE WHEN json_extract(out, '$[0].a') = '-999999999-01-01'
             AND json_extract(out, '$[0].b') = '+10000-01-01'
             AND json_extract(out, '$[0].c') = '-0001-02-03'
             AND json_extract(out, '$[0].d') = '+999999999-12-31T23:59:59'
             AND json_extract(out, '$[0].e') IS NULL
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.4 - huge seconds plus nanos stay exact through duration addition:' as test_name;
WITH r AS (SELECT cypher('RETURN duration({seconds: 100000000000000000, nanoseconds: 999999999}) + duration({nanoseconds: 1}) AS d') AS out)
SELECT CASE WHEN json_extract(out, '$[0].d.seconds') = 100000000000000001
             AND json_extract(out, '$[0].d.nanosecondsOfSecond') = 0
             AND json_extract(out, '$[0].d._iso8601') = 'PT27777777777777H46M41S'
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;

SELECT 'Test 44.5 - small mixed-sign durations unchanged (Temporal10 [11]):' as test_name;
WITH r AS (SELECT cypher('RETURN duration.inSeconds(localtime(''12:44:54.7''), localtime(''12:34:55.3'')) AS d') AS out)
SELECT CASE WHEN json_extract(out, '$[0].d._iso8601') = 'PT-9M-59.4S'
             AND json_extract(out, '$[0].d.seconds') = -600
             AND json_extract(out, '$[0].d.nanosecondsOfSecond') = 600000000
            THEN 'PASS: ' || out ELSE json_extract('FAIL: ' || out, '$') END AS result FROM r;
