-- ========================================================================
-- Test 57: fixes found by the strict TCK harness (GQLITE-T-0429)
-- ========================================================================
-- PURPOSE: errors name their openCypher class and detail; unaliased
--          columns are named as written; writes the per-pattern handlers
--          lost (SET on null, MATCH () CREATE (), DELETE of expressions and
--          paths, MERGE after WITH / MATCH, writes before WITH); collect(p).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 57: strict TCK fixes ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- Errors: class and detail at the start of the message.
INSERT INTO _assert SELECT 'parse error is UnexpectedSyntax',
    json_extract(cypher_validate('MATCH (n RETURN n'), '$.error') LIKE 'SyntaxError: UnexpectedSyntax:%';
INSERT INTO _assert SELECT 'unknown procedure',
    json_extract(cypher_validate('CALL db.labels() YIELD label RETURN label'), '$.error') LIKE 'ProcedureError: ProcedureNotFound:%';
INSERT INTO _assert SELECT 'number run into letters',
    json_extract(cypher_validate('RETURN 12abc'), '$.error') LIKE 'SyntaxError: InvalidNumberLiteral:%';
INSERT INTO _assert SELECT 'float overflow',
    json_extract(cypher_validate('RETURN 1.34E999'), '$.error') LIKE 'SyntaxError: FloatingPointOverflow:%';
INSERT INTO _assert SELECT 'aggregate in WHERE',
    json_extract(cypher_validate('MATCH (a) WHERE count(a) > 1 RETURN a'), '$.error') LIKE 'SyntaxError: InvalidAggregation:%';
INSERT INTO _assert SELECT 'nested aggregate',
    json_extract(cypher_validate('RETURN count(count(*))'), '$.error') LIKE 'SyntaxError: NestedAggregation:%';
INSERT INTO _assert SELECT 'DELETE of a label',
    json_extract(cypher_validate('MATCH (n) DELETE n:Person'), '$.error') LIKE 'SyntaxError: InvalidDelete:%';
INSERT INTO _assert SELECT 'DELETE of an undefined variable',
    json_extract(cypher_validate('MATCH (a) DELETE x'), '$.error') LIKE 'SyntaxError: UndefinedVariable:%';
INSERT INTO _assert SELECT 'UNWIND rebinds a variable',
    json_extract(cypher_validate('WITH 1 AS x UNWIND [2] AS x RETURN x'), '$.error') LIKE 'SyntaxError: VariableAlreadyBound:%';
INSERT INTO _assert SELECT 'true as a map key',
    json(cypher('RETURN {true: 1} AS m')) = json('[{"m":{"true":1}}]');

-- Column names as written.
INSERT INTO _assert SELECT 'unaliased columns',
    json(cypher('RETURN 1 + 2, count( * ), abs(-1)')) = json('[{"1 + 2":3,"count( * )":1,"abs(-1)":1}]');

-- Writes.
SELECT cypher('CREATE (:S57), (:S57)') as setup1;
INSERT INTO _assert SELECT 'MATCH () CREATE ()',
    json_extract(cypher('MATCH (:S57) CREATE (:T57)'), '$.nodes_created') = 2;
SELECT cypher('OPTIONAL MATCH (a:Missing57) SET a.num = 42 RETURN a') as set_on_null;
INSERT INTO _assert SELECT 'SET on null stores nothing',
    (SELECT count(*) FROM node_props_int WHERE node_id NOT IN (SELECT id FROM nodes)) = 0;
SELECT cypher('WITH 42 AS v MERGE (:M57 {v: v})') as merge_after_with;
INSERT INTO _assert SELECT 'MERGE after WITH keeps the property',
    json_extract(cypher('MATCH (m:M57) RETURN m.v AS v'), '$[0].v') = 42;
SELECT cypher('CREATE (:P57 {b: ''x''}), (:P57 {b: ''y''}), (:P57 {b: ''x''})') as setup2;
SELECT cypher('MATCH (p:P57) MERGE (:C57 {name: p.b})') as merge_per_row;
INSERT INTO _assert SELECT 'MERGE per matched row',
    json_extract(cypher('MATCH (c:C57) RETURN count(c) AS c'), '$[0].c') = 2;
SELECT cypher('CREATE (:D57)-[:R57]->(:D57)-[:R57]->(:D57)') as setup3;
SELECT cypher('MATCH p = (:D57)-[:R57]->(:D57)-[:R57]->(:D57) DETACH DELETE p') as delete_path;
INSERT INTO _assert SELECT 'DELETE of a path',
    json_extract(cypher('MATCH (d:D57) RETURN count(d) AS c'), '$[0].c') = 0;
SELECT cypher('CREATE (:L57)-[:F57]->(:K57), (:L57)-[:F57]->(:K57)') as setup4;
SELECT cypher('MATCH (:L57)-[:F57]->(k) WITH collect(k) AS ks DETACH DELETE ks[0]') as delete_list_item;
INSERT INTO _assert SELECT 'DELETE of a list element',
    json_extract(cypher('MATCH (k:K57) RETURN count(k) AS c'), '$[0].c') = 1;
SELECT cypher('CREATE (:N57 {name: ''a'', num: 1}), (:N57 {name: ''a'', num: 2})') as setup5;
INSERT INTO _assert SELECT 'REMOVE before WITH runs',
    json(cypher('MATCH (n:N57) REMOVE n.name WITH n WHERE n.num % 2 = 0 RETURN n.num AS num')) = json('[{"num":2}]');
INSERT INTO _assert SELECT 'REMOVE removed the property',
    json_extract(cypher('MATCH (n:N57) WHERE n.name IS NOT NULL RETURN count(n) AS c'), '$[0].c') = 0;

-- collect() of paths.
INSERT INTO _assert SELECT 'collect(p) holds paths',
    json_type(cypher('MATCH p = (:L57)-[:F57]->() RETURN collect(p) AS ps'), '$[0].ps[0].nodes') = 'array';

SELECT name, ok FROM _assert;
SELECT '=== Test 57 Complete ===' as test_section;
