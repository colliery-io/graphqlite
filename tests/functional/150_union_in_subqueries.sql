-- ========================================================================
-- Test 150: UNION inside subquery bodies (GQLITE-T-0537)
-- ========================================================================
-- PURPOSE: COUNT { A UNION B } and EXISTS { A UNION B } did not parse.
--          CALL { A UNION B } ran as one read-only query: a branch did not
--          see the imported variables ("no such table: _gql_pipe_0", "no
--          such column: p"), a write in a branch did not run, and ORDER BY
--          ... LIMIT in a branch was an SQL error. Each branch now runs as
--          its own subquery. UNION matches columns by name.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 150: UNION inside subquery bodies ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:P150 {name: ''Ada''})-[:FOLLOWS]->(b:P150 {name: ''Bob''}), (a)-[:LIKES]->(b),
                      (a)-[:LIKES]->(c:P150 {name: ''Cat''}), (b)-[:HAS_DOG]->(:Dog150), (c)-[:HAS_CAT]->(:Cat150)') as setup;
SELECT cypher('UNWIND range(1, 10) AS p CREATE (:N150 {prop: p})') as setup2;
SELECT cypher('CREATE (:Cfg150 {threshold: 2})') as setup3;

-- COUNT { } ------------------------------------------------------------------
INSERT INTO _assert SELECT 'COUNT of a union of RETURNs',
    json(cypher('MATCH (p:P150) WHERE COUNT { RETURN 1 AS a UNION RETURN 2 AS a } = 2 RETURN p.name AS n ORDER BY n'))
    = json('[{"n":"Ada"},{"n":"Bob"},{"n":"Cat"}]');

INSERT INTO _assert SELECT 'COUNT, correlated UNION: equal rows count once',
    json(cypher('MATCH (p:P150) RETURN p.name AS n, COUNT { MATCH (p)-[:FOLLOWS]->(f:P150) RETURN f.name AS name UNION MATCH (p)-[:LIKES]->(o:P150) RETURN o.name AS name } AS c ORDER BY n'))
    = json('[{"n":"Ada","c":2},{"n":"Bob","c":0},{"n":"Cat","c":0}]');

INSERT INTO _assert SELECT 'COUNT, correlated UNION ALL',
    json(cypher('MATCH (p:P150) RETURN p.name AS n, COUNT { MATCH (p)-[:FOLLOWS]->(f:P150) RETURN f.name AS name UNION ALL MATCH (p)-[:LIKES]->(o:P150) RETURN o.name AS name } AS c ORDER BY n'))
    = json('[{"n":"Ada","c":3},{"n":"Bob","c":0},{"n":"Cat","c":0}]');

INSERT INTO _assert SELECT 'COUNT, UNION ALL with no RETURN',
    json(cypher('MATCH (p:P150) RETURN p.name AS n, COUNT { MATCH (p)-[:FOLLOWS]->(f:P150) UNION ALL MATCH (p)-[:LIKES]->(o:P150) } AS c ORDER BY n'))
    = json('[{"n":"Ada","c":3},{"n":"Bob","c":0},{"n":"Cat","c":0}]');

-- each branch keeps its ORDER BY ... LIMIT; correlated UNION ALL of such
-- branches is evaluated for each outer row (SQLite 3.53 ran it once)
INSERT INTO _assert SELECT 'COUNT, branches with ORDER BY and LIMIT, correlated',
    json(cypher('MATCH (q:P150) RETURN q.name AS n, COUNT { MATCH (q)-[:LIKES]->(p:P150) RETURN p.name AS x ORDER BY x LIMIT 1 UNION ALL MATCH (q)-[:LIKES]->(p:P150) RETURN p.name AS x ORDER BY x DESC LIMIT 1 } AS c ORDER BY n'))
    = json('[{"n":"Ada","c":2},{"n":"Bob","c":0},{"n":"Cat","c":0}]');

-- EXISTS { } -----------------------------------------------------------------
INSERT INTO _assert SELECT 'EXISTS of a union with no RETURN',
    json(cypher('MATCH (p:P150) RETURN p.name AS n, EXISTS { MATCH (p)-[:HAS_DOG]->(:Dog150) UNION MATCH (p)-[:HAS_CAT]->(:Cat150) } AS e ORDER BY n'))
    = json('[{"n":"Ada","e":false},{"n":"Bob","e":true},{"n":"Cat","e":true}]');

INSERT INTO _assert SELECT 'EXISTS of a UNION ALL with RETURN, in WHERE',
    json(cypher('MATCH (p:P150) WHERE EXISTS { MATCH (p)-[:HAS_DOG]->(d:Dog150) RETURN d UNION ALL MATCH (p)-[:HAS_CAT]->(d:Cat150) RETURN d } RETURN p.name AS n ORDER BY n'))
    = json('[{"n":"Bob"},{"n":"Cat"}]');

-- CALL { } -------------------------------------------------------------------
INSERT INTO _assert SELECT 'CALL: ORDER BY ... LIMIT in each branch',
    json(cypher('CALL { MATCH (x:N150) RETURN x ORDER BY x.prop LIMIT 3 UNION MATCH (x:N150) RETURN x ORDER BY x.prop DESC LIMIT 3 } RETURN sum(x.prop) AS s'))
    = json('[{"s":33}]');

INSERT INTO _assert SELECT 'CALL: correlated branches',
    json(cypher('MATCH (c:Cfg150) CALL { WITH c MATCH (x:N150) WHERE x.prop > c.threshold RETURN x ORDER BY x.prop LIMIT 3 UNION WITH c MATCH (x:N150) WHERE x.prop > c.threshold RETURN x ORDER BY x.prop DESC LIMIT 3 } RETURN sum(x.prop) AS s'))
    = json('[{"s":39}]');

INSERT INTO _assert SELECT 'CALL: a row value imported into each branch',
    json(cypher('UNWIND [1, 2] AS i CALL { WITH i RETURN i * 10 AS v UNION WITH i RETURN i * 100 AS v } RETURN i, v'))
    = json('[{"i":1,"v":10},{"i":1,"v":100},{"i":2,"v":20},{"i":2,"v":200}]');

INSERT INTO _assert SELECT 'CALL: a path imported into each branch',
    json(cypher('MATCH p = (n:Cfg150) CALL { WITH p RETURN length(p) AS l UNION ALL WITH p RETURN length(p) AS l } RETURN l'))
    = json('[{"l":0},{"l":0}]');

INSERT INTO _assert SELECT 'CALL: UNION drops equal rows, UNION ALL keeps them',
    json(cypher('CALL { RETURN 1 AS x UNION RETURN 1 AS x UNION RETURN 2 AS x } RETURN x')) = json('[{"x":1},{"x":2}]')
    AND json(cypher('CALL { RETURN 1 AS x UNION ALL RETURN 1 AS x } RETURN x')) = json('[{"x":1},{"x":1}]');

INSERT INTO _assert SELECT 'CALL: columns matched by name',
    json(cypher('CALL { RETURN 1 AS x, 2 AS y UNION RETURN 3 AS y, 2 AS x } RETURN x, y'))
    = json('[{"x":1,"y":2},{"x":2,"y":3}]');

-- writes in the branches
SELECT cypher('MATCH (n:P150) CALL { CREATE (x:Foo150) UNION CREATE (x:Foo150) }') as writes1;
INSERT INTO _assert SELECT 'CALL: each branch writes for each row',
    json(cypher('MATCH (n:Foo150) RETURN count(n) AS c')) = json('[{"c":6}]');

INSERT INTO _assert SELECT 'CALL: a correlated SET branch and a CREATE branch',
    json(cypher('MATCH (x:P150) CALL { WITH x SET x.prop = 1 UNION CREATE (y:A150) } RETURN count(*) AS c')) = json('[{"c":3}]')
    AND json(cypher('MATCH (x:P150) WHERE x.prop = 1 RETURN count(x) AS c')) = json('[{"c":3}]')
    AND json(cypher('MATCH (y:A150) RETURN count(y) AS c')) = json('[{"c":3}]');

-- top-level UNION ------------------------------------------------------------
INSERT INTO _assert SELECT 'UNION: columns matched by name',
    json(cypher('RETURN 1 AS x, 2 AS y UNION RETURN 3 AS y, 2 AS x')) = json('[{"x":1,"y":2},{"x":2,"y":3}]');

INSERT INTO _assert SELECT 'UNION ALL of unaliased items with the same text',
    json(cypher('RETURN 5 UNION ALL RETURN 5')) = json('[{"5":5},{"5":5}]');

SELECT 'Test 150 passed: ' || count(*) || ' assertions' AS result FROM _assert;
