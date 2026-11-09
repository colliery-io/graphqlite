-- ========================================================================
-- Test 147: one printed form for times and datetimes (GQLITE-T-0550)
-- ========================================================================
-- PURPOSE: A string constructor kept zero seconds and trailing zeros of a
--          fraction as written; a map constructor did not; arithmetic and
--          truncate had their own forms. Equality compares the text, so
--          equal values were not equal. Now every time and datetime prints
--          as Neo4j does: no zero seconds, no trailing zeros, Z for a zero
--          offset. Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 147: canonical temporal form ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'string and map time are equal',
    json(cypher('RETURN time(''13:53:00+01:00'') AS a, time({hour: 13, minute: 53, timezone: ''+01:00''}) AS b, time(''13:53:00+01:00'') = time({hour: 13, minute: 53, timezone: ''+01:00''}) AS eq'))
        = json('[{"a":"13:53+01:00","b":"13:53+01:00","eq":true}]');
INSERT INTO _assert SELECT 'fractions without trailing zeros',
    json(cypher('RETURN localtime(''10:30:00.500'') AS a, localtime({hour: 10, minute: 30, millisecond: 500}) AS b, localtime({hour: 10, minute: 30, nanosecond: 500000000}) AS c, localtime(''10:30:00.000'') AS z'))
        = json('[{"a":"10:30:00.5","b":"10:30:00.5","c":"10:30:00.5","z":"10:30"}]');
INSERT INTO _assert SELECT 'datetime forms',
    json(cypher('RETURN datetime(''2021-06-01T10:30:00+00:00'') AS a, datetime({year: 2021, month: 6, day: 1, hour: 10, minute: 30, timezone: ''Z''}) AS b, localdatetime(''2021-06-01T10:30:00'') AS c, datetime(''2021-06-01T10:30:05Z'') AS d'))
        = json('[{"a":"2021-06-01T10:30Z","b":"2021-06-01T10:30Z","c":"2021-06-01T10:30","d":"2021-06-01T10:30:05Z"}]');
INSERT INTO _assert SELECT 'datetime string = map',
    json(cypher('RETURN datetime(''2021-06-01T10:30:00Z'') = datetime({year: 2021, month: 6, day: 1, hour: 10, minute: 30, timezone: ''Z''}) AS a, localdatetime(''2021-06-01T10:30:00'') = localdatetime({year: 2021, month: 6, day: 1, hour: 10, minute: 30}) AS b'))
        = json('[{"a":true,"b":true}]');
INSERT INTO _assert SELECT 'arithmetic, truncate, epoch',
    json(cypher('RETURN time(''13:53:00+01:00'') + duration({hours: 1}) AS a, datetime.truncate(''second'', datetime(''2021-06-01T10:30:15.123Z'')) AS b, datetime.fromepoch(0, 0) AS c, localdatetime(''2021-06-01T10:30:00.120'') + duration({hours: 1}) AS d'))
        = json('[{"a":"14:53+01:00","b":"2021-06-01T10:30:15Z","c":"1970-01-01T00:00Z","d":"2021-06-01T11:30:00.12"}]');
SELECT cypher('CREATE (:T {t: time(''10:30:00+01:00''), lt: localtime(''10:30:00'')})');
INSERT INTO _assert SELECT 'stored and matched by an equal value',
    json(cypher('MATCH (n:T) WHERE n.t = time({hour: 10, minute: 30, timezone: ''+01:00''}) AND n.lt = localtime({hour: 10, minute: 30}) RETURN n.t AS t, n.lt AS lt'))
        = json('[{"t":"10:30+01:00","lt":"10:30"}]');

SELECT 'Test 147 passed: ' || count(*) || ' assertions' AS result FROM _assert;
