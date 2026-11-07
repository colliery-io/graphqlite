-- ========================================================================
-- Test 125: MERGE of a relationship pattern matches the whole pattern
--           (GQLITE-T-0515)
-- ========================================================================
-- PURPOSE: MERGE matched a relationship pattern one node at a time, so an
--          anonymous or unbound node took any existing node that fit it:
--          `MATCH (a:A) MERGE (a)-[:T]->()` made a self-loop on a, and
--          `MERGE (:A)-[:T]->(:B)` reused unconnected A and B nodes. Now
--          the whole pattern is matched, with the bound variables held
--          fixed, and if it has no match each relationship and each
--          unbound node is created. Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 125: MERGE whole pattern ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));
CREATE TEMP VIEW _shape AS
    SELECT (SELECT count(*) FROM nodes) || ':' ||
           coalesce((SELECT group_concat((source_id - (SELECT min(id) FROM nodes)) || '>' ||
                                         (target_id - (SELECT min(id) FROM nodes)), ' ')
                     FROM (SELECT * FROM edges ORDER BY id)), '') AS s;

-- a bound node and an anonymous end: a new end node, not a self-loop
SELECT cypher('CREATE (:A)');
INSERT INTO _assert SELECT 'bound start, anonymous end',
    json(cypher('MATCH (a:A) MERGE (a)-[r:T]->() RETURN count(r) AS c')) = json('[{"c":1}]')
    AND (SELECT s FROM _shape) = '2:0>1';
INSERT INTO _assert SELECT 'the same MERGE again matches',
    json(cypher('MATCH (a:A) MERGE (a)-[r:T]->() RETURN count(r) AS c')) = json('[{"c":1}]')
    AND (SELECT s FROM _shape) = '2:0>1';
SELECT cypher('MATCH (n) DETACH DELETE n');

-- a longer pattern from a bound node
SELECT cypher('CREATE (:A)');
INSERT INTO _assert SELECT 'longer pattern creates two nodes and two relationships',
    json(cypher('MATCH (a:A) MERGE (a)-[r:T]->()<-[:T]-() RETURN count(r) AS c')) = json('[{"c":1}]')
    AND (SELECT s FROM _shape) = '3:0>1 2>1';
SELECT cypher('MATCH (n) DETACH DELETE n');

-- unconnected nodes that fit the end patterns are not reused
SELECT cypher('CREATE (:A), (:B)');
INSERT INTO _assert SELECT 'unconnected A and B are not reused',
    json(cypher('MERGE (:A)-[r:T]->(:B) RETURN count(r) AS c')) = json('[{"c":1}]')
    AND (SELECT s FROM _shape) = '4:2>3';
INSERT INTO _assert SELECT 'the existing path is matched',
    json(cypher('MERGE (:A)-[r:T]->(:B) RETURN count(r) AS c')) = json('[{"c":1}]')
    AND (SELECT s FROM _shape) = '4:2>3';
SELECT cypher('MATCH (n) DETACH DELETE n');

-- a bound node in the middle of the pattern
SELECT cypher('CREATE (:B)');
INSERT INTO _assert SELECT 'bound node mid-pattern',
    json(cypher('MATCH (b:B) MERGE (a)-[r1:T]->(b)<-[r2:T]-(c) RETURN type(r1) AS t1, type(r2) AS t2'))
        = json('[{"t1":"T","t2":"T"}]')
    AND (SELECT s FROM _shape) = '3:1>0 2>0';
SELECT cypher('MATCH (n) DETACH DELETE n');

-- relationship properties take part in the match; ON MATCH runs once
SELECT cypher('CREATE (:A)-[:T {w: 1}]->(:B)');
INSERT INTO _assert SELECT 'a different relationship property creates a new path',
    json(cypher('MERGE (:A)-[r:T {w: 2}]->(:B) RETURN r.w AS w')) = json('[{"w":2}]')
    AND (SELECT s FROM _shape) = '4:0>1 2>3';
INSERT INTO _assert SELECT 'ON MATCH SET on a matched path',
    json(cypher('MERGE (:A)-[r:T {w: 1}]->(:B) ON MATCH SET r.seen = true ON CREATE SET r.made = true RETURN r.seen AS s, r.made AS m'))
        = json('[{"s":true,"m":null}]')
    AND (SELECT s FROM _shape) = '4:0>1 2>3';
SELECT cypher('MATCH (n) DETACH DELETE n');

-- a named path
INSERT INTO _assert SELECT 'named path over a merged node',
    json_extract(cypher('MERGE (a:A) MERGE p = (a)-[:R]->() RETURN length(p) AS l'), '$[0].l') = 1
    AND (SELECT s FROM _shape) = '2:0>1';

SELECT 'Test 125 passed: ' || count(*) || ' assertions' AS result FROM _assert;
