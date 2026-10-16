-- ========================================================================
-- Test 56: TCK gap fixes (GQLITE-T-0426)
-- ========================================================================
-- PURPOSE: named time zones (IANA tzdata), operator precedence, comparison
--          chains and typed ordering, WITH ordering / limits / DISTINCT,
--          path functions, keyword names, aggregates as a quantifier list.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 56: TCK gap fixes ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- Named time zones: daylight saving and local mean time.
INSERT INTO _assert SELECT 'tz summer offset',
    json_extract(cypher('RETURN datetime({year: 1984, month: 3, day: 28, hour: 12, timezone: ''Europe/Stockholm''}) AS d'), '$[0].d')
    = '1984-03-28T12:00+02:00[Europe/Stockholm]';
INSERT INTO _assert SELECT 'tz offset at the hour on a DST day',
    json_extract(cypher('RETURN datetime({year: 2017, month: 10, day: 29, hour: 0, timezone: ''Europe/Stockholm''}) AS d'), '$[0].d')
    = '2017-10-29T00:00+02:00[Europe/Stockholm]';
INSERT INTO _assert SELECT 'tz local mean time',
    json_extract(cypher('RETURN datetime(''1818-07-21T21:40:32.142[Europe/Stockholm]'') AS d'), '$[0].d')
    = '1818-07-21T21:40:32.142+00:53:28[Europe/Stockholm]';
INSERT INTO _assert SELECT 'duration across DST',
    json_extract(cypher('RETURN duration.inSeconds(datetime({year: 2017, month: 10, day: 29, hour: 0, timezone: ''Europe/Stockholm''}), localdatetime({year: 2017, month: 10, day: 29, hour: 4})) AS d'), '$[0].d._iso8601')
    = 'PT5H';

-- Precedence, chains, typed comparisons.
INSERT INTO _assert SELECT 'IN below +',
    json(cypher('RETURN [1]+2 IN [3]+4 AS a, [1]+(2 IN [3])+4 AS c')) = json('[{"a":false,"c":[1,false,4]}]');
INSERT INTO _assert SELECT 'comparison chain',
    json(cypher('RETURN [1 < 2 = 2, (1 < 2) = true, 2 = 1 + 1 <> 3] AS r')) = json('[{"r":[true,true,true]}]');
INSERT INTO _assert SELECT 'boolean orders only against a boolean',
    json(cypher('RETURN [false < true, true < 1, 1 < 2.5, '''' < true] AS r')) = json('[{"r":[true,null,true,null]}]');
INSERT INTO _assert SELECT 'simple CASE equality',
    json_extract(cypher('RETURN CASE true WHEN 1 THEN ''one'' ELSE ''other'' END AS r'), '$[0].r') = 'other';
INSERT INTO _assert SELECT 'smallest integer',
    json_extract(cypher('RETURN -9223372036854775808 AS i'), '$[0].i') = -9223372036854775808;

-- WITH: DISTINCT, ORDER BY + SKIP, grouping by a call, ORDER BY a projection.
SELECT cypher('CREATE (:W56 {v: 1}), (:W56 {v: 2}), (:W56 {v: 2})') as setup1;
INSERT INTO _assert SELECT 'WITH DISTINCT then count',
    json_extract(cypher('MATCH (n:W56) WITH DISTINCT n.v AS v RETURN count(*) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'WITH ORDER BY SKIP then count',
    json_extract(cypher('MATCH (n:W56) WITH n.v AS v ORDER BY v SKIP 1 RETURN count(*) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'WITH ORDER BY a projected aggregate',
    json(cypher('MATCH (n:W56) WITH n.v AS v, sum(n.v) AS s ORDER BY sum(n.v) DESC LIMIT 1 RETURN v, s')) = json('[{"v":2,"s":4}]');
INSERT INTO _assert SELECT 'aggregate as a quantifier list',
    json_extract(cypher('MATCH (n:W56) RETURN all(x IN collect(n.v) WHERE x > 0) AS a'), '$[0].a') = 1;

-- Paths and keyword names.
SELECT cypher('CREATE (:P56 {name: ''A''})-[:IN]->(:P56 {name: ''B''})-[:IN]->(:P56 {name: ''C''})') as setup2;
INSERT INTO _assert SELECT 'keyword relationship type and var-length nodes',
    json(cypher('MATCH p = (:P56 {name: ''A''})-[:IN*]->(:P56 {name: ''C''}) RETURN [n IN nodes(p) | n.name] AS ns, length(p) AS l'))
    = json('[{"ns":["A","B","C"],"l":2}]');
INSERT INTO _assert SELECT 'path forwarded through WITH',
    json(cypher('MATCH p = (:P56 {name: ''A''})-[:IN*]->(:P56 {name: ''C''}) WITH p RETURN length(p) AS l')) = json('[{"l":2}]');
INSERT INTO _assert SELECT 'NULL map key keeps its spelling',
    json(cypher('RETURN keys({null: 1, NULL: 2}) AS k')) = json('[{"k":["null","NULL"]}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 56 Complete ===' as test_section;
