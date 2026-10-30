-- ========================================================================
-- Test 93: non-reserved keywords as names (GQLITE-T-0440)
-- ========================================================================
-- PURPOSE: openCypher does not reserve these keywords, and Neo4j takes them
--          as names. `yield` became a keyword in 0.12.0, so
--          `RETURN 1 AS yield` and `MATCH (yield)` failed; the others failed
--          as variables before. Each is now a variable, an alias, a
--          property key and a label.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 93: non-reserved keywords as names ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:yield {yield: 0, t93: 1})') AS setup_0;
INSERT INTO _assert SELECT 'yield: variable, label, property key and alias',
    json(cypher('MATCH (yield:yield) WHERE yield.t93 = 1 RETURN yield.yield AS yield'))
    = json('[{"yield":0}]');
INSERT INTO _assert SELECT 'yield: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS yield RETURN [yield IN [yield] | yield * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'yield: label predicate and SET',
    json_extract(cypher('MATCH (yield) WHERE yield:yield SET yield.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:from {from: 1, t93: 1})') AS setup_1;
INSERT INTO _assert SELECT 'from: variable, label, property key and alias',
    json(cypher('MATCH (from:from) WHERE from.t93 = 1 RETURN from.from AS from'))
    = json('[{"from":1}]');
INSERT INTO _assert SELECT 'from: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS from RETURN [from IN [from] | from * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'from: label predicate and SET',
    json_extract(cypher('MATCH (from) WHERE from:from SET from.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:csv {csv: 2, t93: 1})') AS setup_2;
INSERT INTO _assert SELECT 'csv: variable, label, property key and alias',
    json(cypher('MATCH (csv:csv) WHERE csv.t93 = 1 RETURN csv.csv AS csv'))
    = json('[{"csv":2}]');
INSERT INTO _assert SELECT 'csv: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS csv RETURN [csv IN [csv] | csv * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'csv: label predicate and SET',
    json_extract(cypher('MATCH (csv) WHERE csv:csv SET csv.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:load {load: 3, t93: 1})') AS setup_3;
INSERT INTO _assert SELECT 'load: variable, label, property key and alias',
    json(cypher('MATCH (load:load) WHERE load.t93 = 1 RETURN load.load AS load'))
    = json('[{"load":3}]');
INSERT INTO _assert SELECT 'load: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS load RETURN [load IN [load] | load * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'load: label predicate and SET',
    json_extract(cypher('MATCH (load) WHERE load:load SET load.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:headers {headers: 4, t93: 1})') AS setup_4;
INSERT INTO _assert SELECT 'headers: variable, label, property key and alias',
    json(cypher('MATCH (headers:headers) WHERE headers.t93 = 1 RETURN headers.headers AS headers'))
    = json('[{"headers":4}]');
INSERT INTO _assert SELECT 'headers: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS headers RETURN [headers IN [headers] | headers * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'headers: label predicate and SET',
    json_extract(cypher('MATCH (headers) WHERE headers:headers SET headers.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:fieldterminator {fieldterminator: 5, t93: 1})') AS setup_5;
INSERT INTO _assert SELECT 'fieldterminator: variable, label, property key and alias',
    json(cypher('MATCH (fieldterminator:fieldterminator) WHERE fieldterminator.t93 = 1 RETURN fieldterminator.fieldterminator AS fieldterminator'))
    = json('[{"fieldterminator":5}]');
INSERT INTO _assert SELECT 'fieldterminator: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS fieldterminator RETURN [fieldterminator IN [fieldterminator] | fieldterminator * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'fieldterminator: label predicate and SET',
    json_extract(cypher('MATCH (fieldterminator) WHERE fieldterminator:fieldterminator SET fieldterminator.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:pattern {pattern: 6, t93: 1})') AS setup_6;
INSERT INTO _assert SELECT 'pattern: variable, label, property key and alias',
    json(cypher('MATCH (pattern:pattern) WHERE pattern.t93 = 1 RETURN pattern.pattern AS pattern'))
    = json('[{"pattern":6}]');
INSERT INTO _assert SELECT 'pattern: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS pattern RETURN [pattern IN [pattern] | pattern * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'pattern: label predicate and SET',
    json_extract(cypher('MATCH (pattern) WHERE pattern:pattern SET pattern.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:explain {explain: 7, t93: 1})') AS setup_7;
INSERT INTO _assert SELECT 'explain: variable, label, property key and alias',
    json(cypher('MATCH (explain:explain) WHERE explain.t93 = 1 RETURN explain.explain AS explain'))
    = json('[{"explain":7}]');
INSERT INTO _assert SELECT 'explain: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS explain RETURN [explain IN [explain] | explain * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'explain: label predicate and SET',
    json_extract(cypher('MATCH (explain) WHERE explain:explain SET explain.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:reduce {reduce: 8, t93: 1})') AS setup_8;
INSERT INTO _assert SELECT 'reduce: variable, label, property key and alias',
    json(cypher('MATCH (reduce:reduce) WHERE reduce.t93 = 1 RETURN reduce.reduce AS reduce'))
    = json('[{"reduce":8}]');
INSERT INTO _assert SELECT 'reduce: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS reduce RETURN [reduce IN [reduce] | reduce * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'reduce: label predicate and SET',
    json_extract(cypher('MATCH (reduce) WHERE reduce:reduce SET reduce.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:any {any: 9, t93: 1})') AS setup_9;
INSERT INTO _assert SELECT 'any: variable, label, property key and alias',
    json(cypher('MATCH (any:any) WHERE any.t93 = 1 RETURN any.any AS any'))
    = json('[{"any":9}]');
INSERT INTO _assert SELECT 'any: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS any RETURN [any IN [any] | any * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'any: label predicate and SET',
    json_extract(cypher('MATCH (any) WHERE any:any SET any.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:none {none: 10, t93: 1})') AS setup_10;
INSERT INTO _assert SELECT 'none: variable, label, property key and alias',
    json(cypher('MATCH (none:none) WHERE none.t93 = 1 RETURN none.none AS none'))
    = json('[{"none":10}]');
INSERT INTO _assert SELECT 'none: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS none RETURN [none IN [none] | none * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'none: label predicate and SET',
    json_extract(cypher('MATCH (none) WHERE none:none SET none.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:single {single: 11, t93: 1})') AS setup_11;
INSERT INTO _assert SELECT 'single: variable, label, property key and alias',
    json(cypher('MATCH (single:single) WHERE single.t93 = 1 RETURN single.single AS single'))
    = json('[{"single":11}]');
INSERT INTO _assert SELECT 'single: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS single RETURN [single IN [single] | single * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'single: label predicate and SET',
    json_extract(cypher('MATCH (single) WHERE single:single SET single.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:call {call: 12, t93: 1})') AS setup_12;
INSERT INTO _assert SELECT 'call: variable, label, property key and alias',
    json(cypher('MATCH (call:call) WHERE call.t93 = 1 RETURN call.call AS call'))
    = json('[{"call":12}]');
INSERT INTO _assert SELECT 'call: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS call RETURN [call IN [call] | call * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'call: label predicate and SET',
    json_extract(cypher('MATCH (call) WHERE call:call SET call.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:foreach {foreach: 13, t93: 1})') AS setup_13;
INSERT INTO _assert SELECT 'foreach: variable, label, property key and alias',
    json(cypher('MATCH (foreach:foreach) WHERE foreach.t93 = 1 RETURN foreach.foreach AS foreach'))
    = json('[{"foreach":13}]');
INSERT INTO _assert SELECT 'foreach: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS foreach RETURN [foreach IN [foreach] | foreach * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'foreach: label predicate and SET',
    json_extract(cypher('MATCH (foreach) WHERE foreach:foreach SET foreach.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:shortestPath {shortestPath: 14, t93: 1})') AS setup_14;
INSERT INTO _assert SELECT 'shortestPath: variable, label, property key and alias',
    json(cypher('MATCH (shortestPath:shortestPath) WHERE shortestPath.t93 = 1 RETURN shortestPath.shortestPath AS shortestPath'))
    = json('[{"shortestPath":14}]');
INSERT INTO _assert SELECT 'shortestPath: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS shortestPath RETURN [shortestPath IN [shortestPath] | shortestPath * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'shortestPath: label predicate and SET',
    json_extract(cypher('MATCH (shortestPath) WHERE shortestPath:shortestPath SET shortestPath.s93 = 1'), '$.properties_set') = 1;
SELECT cypher('CREATE (:allShortestPaths {allShortestPaths: 15, t93: 1})') AS setup_15;
INSERT INTO _assert SELECT 'allShortestPaths: variable, label, property key and alias',
    json(cypher('MATCH (allShortestPaths:allShortestPaths) WHERE allShortestPaths.t93 = 1 RETURN allShortestPaths.allShortestPaths AS allShortestPaths'))
    = json('[{"allShortestPaths":15}]');
INSERT INTO _assert SELECT 'allShortestPaths: UNWIND and comprehension variable',
    json(cypher('UNWIND [1, 2] AS allShortestPaths RETURN [allShortestPaths IN [allShortestPaths] | allShortestPaths * 10] AS l ORDER BY l'))
    = json('[{"l":[10]},{"l":[20]}]');
INSERT INTO _assert SELECT 'allShortestPaths: label predicate and SET',
    json_extract(cypher('MATCH (allShortestPaths) WHERE allShortestPaths:allShortestPaths SET allShortestPaths.s93 = 1'), '$.properties_set') = 1;

SELECT 'Test 93 passed: ' || count(*) || ' assertions' AS result FROM _assert;
