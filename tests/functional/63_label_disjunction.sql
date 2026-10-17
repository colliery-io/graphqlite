-- ========================================================================
-- Test 63: label disjunction (n:A|B); OPTIONAL MATCH with a left arrow
-- ========================================================================
-- PURPOSE: `MATCH (n:A|B)` matches a node with either label (GQLITE-T-0125);
--          CREATE / MERGE reject it. `OPTIONAL MATCH (p)<-[r:K]-(q)` with p
--          bound matched nothing: the edge columns came from the positions.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 63: label disjunction ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:P63 {n: "p"}), (:E63 {n: "e"}), (:O63 {n: "o"}), (:P63:E63 {n: "pe"})') as setup1;
INSERT INTO _assert SELECT 'either label',
    json(cypher('MATCH (x:P63|E63) RETURN x.n AS n ORDER BY n')) = json('[{"n":"e"},{"n":"p"},{"n":"pe"}]');
INSERT INTO _assert SELECT 'three labels',
    json_extract(cypher('MATCH (x:P63|E63|O63) RETURN count(x) AS c'), '$[0].c') = 4;
INSERT INTO _assert SELECT 'with properties',
    json(cypher('MATCH (x:P63|E63 {n: "e"}) RETURN x.n AS n')) = json('[{"n":"e"}]');
INSERT INTO _assert SELECT 'OPTIONAL miss',
    json(cypher('OPTIONAL MATCH (x:None63|Nada63) RETURN x')) = json('[{"x":null}]');
INSERT INTO _assert SELECT 'CREATE rejects it',
    json_extract(cypher_validate('CREATE (x:A63|B63)'), '$.error') LIKE 'SyntaxError: InvalidLabelExpression:%';

SELECT cypher('CREATE (:P63 {n: "p2"})-[:K63]->(:O63 {n: "o2"})') as setup2;
INSERT INTO _assert SELECT 'in a relationship pattern',
    json(cypher('MATCH (a:P63|E63)-[:K63]->(b:O63|None63) RETURN a.n AS a, b.n AS b')) = json('[{"a":"p2","b":"o2"}]');
INSERT INTO _assert SELECT 'in a pattern predicate',
    json(cypher('MATCH (a) WHERE (a)-[:K63]->(:O63|E63) RETURN a.n AS a')) = json('[{"a":"p2"}]');
INSERT INTO _assert SELECT 'OPTIONAL left arrow, anonymous relationship',
    json(cypher('MATCH (p:O63) OPTIONAL MATCH (p)<-[:K63]-(q:P63|E63) RETURN p.n AS p, q.n AS q ORDER BY p')) = json('[{"p":"o","q":null},{"p":"o2","q":"p2"}]');
INSERT INTO _assert SELECT 'OPTIONAL left arrow, named relationship',
    json(cypher('MATCH (p:O63 {n: "o2"}) OPTIONAL MATCH (p)<-[r:K63]-(q) RETURN q.n AS q, type(r) AS t')) = json('[{"q":"p2","t":"K63"}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 63 Complete ===' as test_section;
