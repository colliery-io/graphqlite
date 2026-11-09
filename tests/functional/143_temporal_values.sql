-- ========================================================================
-- Test 143: temporal components, arithmetic, truncate, ordering
--           (GQLITE-T-0544)
-- ========================================================================
-- PURPOSE: A component was null outside a WITH-bound constructor (UNWIND,
--          a call, a stored property). d + dur - dur printed +00:00 for Z
--          (and dropped written seconds and the zone name), so it was not
--          equal to d. datetime({..., timezone: 'Z'}) printed +00:00[Z]
--          and did not truncate. localtime.truncate gave 00:00. ORDER BY a
--          time wrapped at midnight UTC. Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 143: temporal values ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'components after UNWIND',
    json(cypher('UNWIND [date(''1999-05-05''), date({year: 2002, month: 5, day: 28})] AS x RETURN x.year AS y, x.month AS m'))
        = json('[{"y":1999,"m":5},{"y":2002,"m":5}]');
INSERT INTO _assert SELECT 'components of a call',
    json(cypher('RETURN (date(''2024-03-05'')).year AS y, (duration(''P32DT12H'')).days AS d, (duration(''P32DT12H'')).hours AS h, (localtime(''03:07:49'')).minute AS mi'))
        = json('[{"y":2024,"d":32,"h":12,"mi":7}]');
SELECT cypher('CREATE (:T {k: date(''2024-03-05''), t: datetime(''2024-03-05T10:20:30Z'')})');
INSERT INTO _assert SELECT 'components of a stored value',
    json(cypher('MATCH (n:T) RETURN n.k.year AS y, n.t.hour AS h, n.t.epochMillis AS e'))
        = json('[{"y":2024,"h":10,"e":1709634030000}]');
INSERT INTO _assert SELECT 'ORDER BY a component after UNWIND',
    json(cypher('UNWIND [date(''1999-05-05''), date(''2002-05-28''), date(''2001-04-01'')] AS x RETURN x AS t ORDER BY x.year'))
        = json('[{"t":"1999-05-05"},{"t":"2001-04-01"},{"t":"2002-05-28"}]');

INSERT INTO _assert SELECT 'arithmetic keeps the form',
    json(cypher('RETURN datetime(''2003-04-09T23:25:58Z'') + duration({days: 1}) AS a, time(''13:53:00+01:00'') + duration({hours: 1}) AS b, datetime(''2003-04-09T23:25:58[Europe/Stockholm]'') + duration({days: 1}) AS c'))
        = json('[{"a":"2003-04-10T23:25:58Z","b":"14:53:00+01:00","c":"2003-04-10T23:25:58+02:00[Europe/Stockholm]"}]');
INSERT INTO _assert SELECT 'd + dur - dur = d',
    json(cypher('WITH datetime(''2003-04-09T23:25:58Z'') AS x, time(''13:53:00+01:00'') AS t RETURN (x + duration({days: 323, hours: 15})) - duration({days: 323, hours: 15}) = x AS a, (t + duration({hours: 1, minutes: 36})) - duration({hours: 1, minutes: 36}) = t AS b'))
        = json('[{"a":true,"b":true}]');

INSERT INTO _assert SELECT 'timezone Z in a map',
    json(cypher('WITH datetime({year: 2021, month: 2, day: 2, hour: 9, minute: 7, second: 20, timezone: ''Z''}) AS x RETURN x AS x, datetime.truncate(''day'', x) AS t, time({hour: 10, timezone: ''Z''}) AS u'))
        = json('[{"x":"2021-02-02T09:07:20Z","t":"2021-02-02T00:00Z","u":"10:00Z"}]');
INSERT INTO _assert SELECT 'localtime.truncate',
    json(cypher('RETURN localtime.truncate(''hour'', localtime(''21:23:49'')) AS a, localtime.truncate(''minute'', localtime(''21:23:49'')) AS c'))
        = json('[{"a":"21:00","c":"21:23"}]');

INSERT INTO _assert SELECT 'ORDER BY time, one offset',
    json(cypher('UNWIND [time(''09:34:49+01:00''), time(''08:27:30+01:00''), time(''00:06:01+01:00'')] AS x RETURN x AS t ORDER BY x'))
        = json('[{"t":"00:06:01+01:00"},{"t":"08:27:30+01:00"},{"t":"09:34:49+01:00"}]');
INSERT INTO _assert SELECT 'ORDER BY time, offsets',
    json(cypher('UNWIND [time(''10:00+01:00''), time(''09:35+00:00'')] AS x RETURN x AS t ORDER BY x'))
        = json('[{"t":"10:00+01:00"},{"t":"09:35Z"}]');

SELECT 'Test 143 passed: ' || count(*) || ' assertions' AS result FROM _assert;
