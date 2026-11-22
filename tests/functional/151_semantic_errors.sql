-- ========================================================================
-- Test 151: semantic errors that were missing (GQLITE-T-0539)
-- ========================================================================
-- PURPOSE: queries that Cypher refuses and GraphQLite ran: a CALL { } body
--          that reads a variable it did not import, or has an unaliased
--          RETURN item; SKIP / LIMIT that read the graph; isEmpty() of a
--          node, relationship or path; a pattern expression that brings in
--          a variable; a property value used as a node.
--          Each is a validation error, so cypher_validate() reports it
--          (cypher() raises an SQL error, which this runner cannot assert).
--          trim() of a number is a run-time error: tests/test_executor_functions.c.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 151: missing semantic errors ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));
CREATE TEMP TABLE _bad(name TEXT, q TEXT, detail TEXT);

INSERT INTO _bad VALUES
 ('CALL body reads a variable it did not import', 'MATCH (a), (b) CALL { WITH a RETURN b AS c } RETURN c', 'UndefinedVariable'),
 ('CALL body: a WITH drops the imported variable', 'MATCH (a) CALL { WITH a WITH 1 AS b RETURN a AS c } RETURN c', 'UndefinedVariable'),
 ('CALL body imports a variable that is not there', 'MATCH (a) CALL { WITH zz RETURN 1 AS c } RETURN c', 'UndefinedVariable'),
 ('CALL: unaliased RETURN item', 'CALL { RETURN 5 } RETURN `5` AS five', 'NoExpressionAlias'),
 ('CALL: unaliased RETURN item, correlated', 'MATCH (n) CALL { WITH n RETURN n.x + 1 } RETURN n', 'NoExpressionAlias'),
 ('CALL: unaliased RETURN item in a union', 'MATCH (n) CALL { WITH n RETURN 5 + 5 UNION WITH n RETURN 5 + 5 } RETURN `5 + 5` AS plus', 'NoExpressionAlias'),
 ('LIMIT of a pattern count', 'MATCH (p) RETURN p.name AS name LIMIT size(()-->())', 'NonConstantExpression'),
 ('SKIP of a pattern comprehension', 'MATCH (p) RETURN p.name AS name SKIP size([(a)-->(b) | b.age])', 'NonConstantExpression'),
 ('LIMIT of reduce() over a variable', 'MATCH (p) RETURN p.name AS name LIMIT reduce(sum = 0, x IN [p.age] | sum + x)', 'NonConstantExpression'),
 ('LIMIT of reduce() over a pattern comprehension', 'MATCH (p) RETURN p.name AS name LIMIT reduce(sum = 0, x IN [(a)-->(b) | b.age] | sum + x)', 'NonConstantExpression'),
 ('isEmpty of a path', 'MATCH path = (a)--(b) RETURN isEmpty(path) AS r', 'SyntaxError: InvalidArgumentType'),
 ('isEmpty of a node', 'MATCH (a) RETURN isEmpty(a) AS r', 'TypeError: InvalidArgumentValue'),
 ('isEmpty of a relationship', 'MATCH ()-[r]->() RETURN isEmpty(r) AS x', 'TypeError: InvalidArgumentValue'),
 ('pattern expression with a new node variable', 'MATCH (n) RETURN (n)-[:T]->(b)', 'UndefinedVariable'),
 ('pattern expression with a new relationship variable', 'MATCH (n) RETURN (n)-[r:T]->()', 'UndefinedVariable'),
 ('a property value as a node', 'MATCH (a:A) WITH a.prop AS prop MATCH (b:B)<--(prop) RETURN b', 'VariableTypeConflict'),
 ('a property value as a relationship', 'MATCH (a:A) WITH a.prop AS prop MATCH (b:B)<-[prop]-() RETURN b', 'VariableTypeConflict');

INSERT INTO _assert
SELECT name,
       json_extract(cypher_validate(q), '$.valid') = 0
       AND instr(json_extract(cypher_validate(q), '$.error'), detail) > 0
FROM _bad;

-- still valid --------------------------------------------------------------
CREATE TEMP TABLE _good(name TEXT, q TEXT);
INSERT INTO _good VALUES
 ('CALL: imported variable under an alias', 'MATCH (a) CALL { WITH a AS b RETURN b.prop AS c } RETURN c'),
 ('CALL: WITH *, item keeps the scope', 'MATCH (a) CALL { WITH a WITH *, 2 AS t RETURN a.prop + t AS c } RETURN c'),
 ('CALL: a variable of the body', 'MATCH (a) CALL { WITH a MATCH (a)-->(b) RETURN b } RETURN b'),
 ('CALL: UNWIND and a path variable in the body', 'MATCH (a) CALL { WITH a UNWIND [1, 2] AS i MATCH p = (a) RETURN i, length(p) AS l } RETURN i, l'),
 ('CALL: a map projection has the name of its variable', 'CALL { MATCH (n) RETURN n {.prop} } RETURN n'),
 ('LIMIT of a constant expression', 'MATCH (p) RETURN p.name AS name LIMIT toInteger(ceil(1.7))'),
 ('LIMIT of reduce() over constants', 'MATCH (p) RETURN p.name AS name LIMIT reduce(s = 0, x IN [1, 2] | s + x)'),
 ('isEmpty of a property', 'MATCH (a) RETURN isEmpty(a.name) AS r'),
 ('a map entry that holds a node, used as a node', 'MATCH (a) WITH {k: a} AS map WITH map.k AS nd MATCH (p)<--(nd) RETURN p'),
 ('a name out of scope, bound again as a node', 'MATCH (a) WITH a.prop AS prop WITH 1 AS one MATCH (prop) RETURN prop'),
 ('exists(pattern) in RETURN', 'MATCH (n) RETURN n, exists((n)--()) AS b');
INSERT INTO _assert SELECT name, json_extract(cypher_validate(q), '$.valid') = 1 FROM _good;

-- results --------------------------------------------------------------------
SELECT cypher('CREATE (a:A151 {prop: 1})-[:T]->(b:B151 {name: ''b''})') as setup;

INSERT INTO _assert SELECT 'exists(pattern) as a RETURN value',
    json(cypher('MATCH (n) WHERE n:A151 OR n:B151 RETURN labels(n)[0] AS l, exists((n)-->()) AS o, exists((n)<--()) AS i ORDER BY l'))
    = json('[{"l":"A151","o":true,"i":false},{"l":"B151","o":false,"i":true}]');

INSERT INTO _assert SELECT 'CALL: a map projection is named by its variable',
    json(cypher('CALL { MATCH (n:A151) RETURN n {.prop, .foo} } RETURN n')) = json('[{"n":{"prop":1,"foo":null}}]');

SELECT 'Test 151 passed: ' || count(*) || ' assertions' AS result FROM _assert;
