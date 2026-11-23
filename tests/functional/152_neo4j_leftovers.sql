-- ========================================================================
-- Test 152: single bugs from the Neo4j acceptance triage (GQLITE-T-0540)
-- ========================================================================
-- PURPOSE: one or two scenarios each:
--   - an undirected pattern predicate, `WHERE (a)--(b)` and `(a)-[r]-(b)`,
--     was "syntax is ambiguous"
--   - a pattern predicate with two relationships let one relationship
--     match twice
--   - OPTIONAL MATCH of bound variables with a WHERE: SQL error, or the
--     WHERE filtered the rows of the clause before
--   - COUNT { MATCH (p) RETURN * } did not come back
--   - COUNT { (n) } of a null n was 1
--   - `… UNION … RETURN *`: the columns of RETURN * matched by name
--   - a reduce() or any() variable with the name of an outer variable
--     broke the outer variable after the expression
--   - coalesce(x, false) = false was false
--   - MATCH (n:Label) had no rows for a node that came out of a list or map
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 152: Neo4j acceptance leftovers ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:Foo152 {n: ''a''}), (b:Bar152 {name: ''b''}), (c:Foo152 {n: ''c''}), (d:Foo152 {n: ''d''}),
                      (e:Bar152 {name: ''e''}), (:Bar152), (a)-[:HAS {w: 7}]->(b), (c)-[:HAS]->(b), (d)-[:HAS]->(e)') as setup;

-- undirected pattern predicates ------------------------------------------------
INSERT INTO _assert SELECT 'WHERE (a)--(b)',
    json(cypher('MATCH (a:Foo152), (b:Bar152) WHERE (a)--(b) RETURN a.n AS a, b.name AS b ORDER BY a'))
    = json('[{"a":"a","b":"b"},{"a":"c","b":"b"},{"a":"d","b":"e"}]');
INSERT INTO _assert SELECT 'WHERE (b)-[r]-(a), r bound',
    json(cypher('MATCH (a:Foo152)-[r]-(b:Bar152) WHERE (b)-[r]-(a) RETURN count(*) AS c')) = json('[{"c":3}]');
INSERT INTO _assert SELECT 'WHERE NOT (a)--(b)',
    json(cypher('MATCH (a:Foo152 {n: ''a''}), (b:Bar152) WHERE NOT (a)--(b) RETURN count(*) AS c')) = json('[{"c":2}]');
INSERT INTO _assert SELECT 'minus is still minus',
    json(cypher('WITH 1 AS a, 2 AS b RETURN (a)-(b) AS v, a - b AS w, a - -b AS x, 5 - 3 - 1 AS y, -a - -b AS z'))
    = json('[{"v":-1,"w":-1,"x":3,"y":1,"z":1}]');

-- two relationships in a pattern predicate are two different relationships
INSERT INTO _assert SELECT '(a)--(b)--() needs a second relationship of b',
    json(cypher('MATCH (a:Foo152) OPTIONAL MATCH (a)--(b:Bar152) WHERE (a)--(b:Bar152)--() RETURN a.n AS a, b.name AS b ORDER BY a'))
    = json('[{"a":"a","b":"b"},{"a":"c","b":"b"},{"a":"d","b":null}]');
INSERT INTO _assert SELECT 'NOT ((a)--(b)--())',
    json(cypher('MATCH (a:Foo152) OPTIONAL MATCH (a)--(b:Bar152) WHERE NOT ((a)--(b:Bar152)--()) RETURN a.n AS a, b.name AS b ORDER BY a'))
    = json('[{"a":"a","b":null},{"a":"c","b":null},{"a":"d","b":"e"}]');

-- OPTIONAL MATCH of bound variables with a WHERE ------------------------------
INSERT INTO _assert SELECT 'OPTIONAL MATCH (p) WHERE on a null p',
    json(cypher('OPTIONAL MATCH (o:Foo152)-[:NOPE]->(p) WITH p OPTIONAL MATCH (p) WHERE p.prop2 IS NULL RETURN DISTINCT p'))
    = json('[{"p":null}]');
INSERT INTO _assert SELECT 'OPTIONAL MATCH (a) WHERE: the rows and a stay',
    json(cypher('MATCH (a:Foo152) OPTIONAL MATCH (a) WHERE a.n = ''a'' RETURN a.n AS a ORDER BY a'))
    = json('[{"a":"a"},{"a":"c"},{"a":"d"}]');
INSERT INTO _assert SELECT 'the same after a WITH',
    json(cypher('MATCH (a:Foo152) WITH a OPTIONAL MATCH (a) WHERE a.n = ''a'' RETURN a.n AS a ORDER BY a'))
    = json('[{"a":"a"},{"a":"c"},{"a":"d"}]');

-- COUNT { } ----------------------------------------------------------------
INSERT INTO _assert SELECT 'COUNT { MATCH (p) RETURN * }',
    json(cypher('MATCH (p:Foo152) WHERE COUNT { MATCH (p) RETURN * } = 1 RETURN p.n AS n ORDER BY n'))
    = json('[{"n":"a"},{"n":"c"},{"n":"d"}]');
INSERT INTO _assert SELECT 'COUNT { (n) } of a null n is 0',
    json(cypher('OPTIONAL MATCH (n:DoesNotExist152) RETURN [COUNT { (n) }, COUNT { (n)-->() }, COUNT { (n)-->({prop: 1}) }] AS counts'))
    = json('[{"counts":[0,0,0]}]');
INSERT INTO _assert SELECT 'COUNT { (b) } after OPTIONAL MATCH',
    json(cypher('MATCH (a:Foo152 {n: ''a''}) OPTIONAL MATCH (a)-[:NOPE]->(b) RETURN COUNT { (a) } AS a, COUNT { (b) } AS b, EXISTS { MATCH (b) } AS e'))
    = json('[{"a":1,"b":0,"e":false}]');

-- UNION and RETURN * ---------------------------------------------------------
INSERT INTO _assert SELECT 'RETURN * in a UNION branch, other column order',
    json(cypher('WITH 1 AS y, 2 AS x RETURN y, x UNION WITH 2 AS x, 1 AS y RETURN *')) = json('[{"y":1,"x":2}]');
INSERT INTO _assert SELECT 'RETURN * in both branches',
    json(cypher('WITH 1 AS y, 2 AS x RETURN * UNION WITH 3 AS x, 4 AS y RETURN *')) = json('[{"y":1,"x":2},{"y":4,"x":3}]');

-- a local variable with the name of an outer variable ---------------------------
INSERT INTO _assert SELECT 'reduce() variable named as a relationship',
    json(cypher('MATCH (a:Foo152 {n: ''a''})-[r:HAS]->(b) RETURN reduce(acc = 0, r IN [1, 2] | acc + r) AS s, r.w AS w'))
    = json('[{"s":3,"w":7}]');
INSERT INTO _assert SELECT 'any() variable named as a node',
    json(cypher('MATCH (a:Foo152 {n: ''a''})-[r:HAS]->(b) RETURN any(b IN [1, 2] WHERE b > 1) AS t, b.name AS n'))
    = json('[{"t":true,"n":"b"}]');
INSERT INTO _assert SELECT 'reduce() accumulator and list read the outer names',
    json(cypher('WITH 3 AS acc, 4 AS v RETURN reduce(acc = acc, v IN [v, 1] | acc + v) AS s, acc, v'))
    = json('[{"s":8,"acc":3,"v":4}]');
INSERT INTO _assert SELECT 'nested reduce() and list predicates',
    json(cypher('RETURN reduce(a = 0, i IN [1, 2, 3] | a + reduce(b = 0, j IN [i, 10] | b + j)) AS r, none(x IN [1, 2] WHERE single(y IN [x, 5] WHERE y = 5 AND x = 3)) AS q'))
    = json('[{"r":36,"q":true}]');

-- coalesce with a boolean literal ----------------------------------------------
INSERT INTO _assert SELECT 'coalesce(missing, false) = false',
    json(cypher('MATCH (a:Foo152 {n: ''a''}) RETURN coalesce(a.x, false) = false AS a, coalesce(null, true) = true AS b, coalesce(a.x, false) AS c'))
    = json('[{"a":true,"b":true,"c":false}]');
INSERT INTO _assert SELECT 'in WHERE and in OPTIONAL MATCH ... WHERE',
    json(cypher('MATCH (a:Foo152) WHERE coalesce(a.deleted, false) = false RETURN count(a) AS c')) = json('[{"c":3}]')
    AND json(cypher('MATCH (a:Foo152 {n: ''a''}) OPTIONAL MATCH (a)-[:HAS]->(b) WHERE coalesce(b.deleted, false) = false RETURN b.name AS b')) = json('[{"b":"b"}]');

-- a label on a node that came out of a list or a map ------------------------------
INSERT INTO _assert SELECT 'MATCH (n:L) of a node from a map',
    json(cypher('MATCH (a:Foo152 {n: ''a''}) WITH {k: a} AS m WITH m.k AS a MATCH (a:Foo152)-[:HAS]->(b) RETURN b.name AS b')) = json('[{"b":"b"}]')
    AND json(cypher('MATCH (a:Foo152 {n: ''a''}) WITH {k: a} AS m WITH m.k AS a MATCH (a:Bar152) RETURN a.n AS n')) = json('[]');
INSERT INTO _assert SELECT 'MATCH (n:L) of a node from a list',
    json(cypher('MATCH (n) WHERE n:Foo152 OR n:Bar152 WITH collect(n) AS ns UNWIND ns AS n MATCH (n:Foo152) RETURN count(n) AS c')) = json('[{"c":3}]')
    AND json(cypher('MATCH (n:Foo152) WITH collect(n) AS ns UNWIND ns AS n MATCH (n:Foo152|Bar152) RETURN count(n) AS c')) = json('[{"c":3}]');

SELECT 'Test 152 passed: ' || count(*) || ' assertions' AS result FROM _assert;
