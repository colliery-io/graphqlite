-- ========================================================================
-- Test 97: variable-length relationships and type lists in a pattern
--          comprehension (GQLITE-T-0472)
-- ========================================================================
-- PURPOSE: a pattern comprehension joined one edge for `-[*1..2]->` and
--          dropped the range, and ignored a type list `:R|S`. A
--          var-length relationship is now a walk table (distinct edges, the
--          pattern's direction, anchored at an outer start node); a type
--          list is applied. A named path over one var-length relationship
--          is the walk's nodes and relationships (another shape is refused
--          with a clear error). Found while working GQLITE-T-0459.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 97: var-length pattern comprehensions ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A97 {id: 1})-[:S97]->(:B97 {id: 2})-[:S97]->(:C97 {id: 3}), (a)-[:R97]->(:D97 {id: 4}), (a)-[:T97]->(:E97 {id: 5})') as setup;

INSERT INTO _assert SELECT '*1..2',
    (SELECT group_concat(value) FROM (SELECT value FROM json_each(json_extract(cypher('MATCH (a:A97) RETURN [(a)-[*1..2]->(b) | b.id] AS l'), '$[0].l')) ORDER BY value)) = '2,3,4,5';
INSERT INTO _assert SELECT '*2',
    json(cypher('MATCH (a:A97) RETURN [(a)-[*2]->(b) | b.id] AS l')) = json('[{"l":[3]}]');
INSERT INTO _assert SELECT 'a type and no upper bound',
    (SELECT group_concat(value) FROM (SELECT value FROM json_each(json_extract(cypher('MATCH (a:A97) RETURN [(a)-[:S97*]->(b) | b.id] AS l'), '$[0].l')) ORDER BY value)) = '2,3';
INSERT INTO _assert SELECT '*0..1 includes the start',
    (SELECT group_concat(value) FROM (SELECT value FROM json_each(json_extract(cypher('MATCH (a:A97) RETURN [(a)-[*0..1]->(b) | b.id] AS l'), '$[0].l')) ORDER BY value)) = '1,2,4,5';
INSERT INTO _assert SELECT 'left arrow',
    (SELECT group_concat(value) FROM (SELECT value FROM json_each(json_extract(cypher('MATCH (c:C97) RETURN [(c)<-[*1..2]-(b) | b.id] AS l'), '$[0].l')) ORDER BY value)) = '1,2';
INSERT INTO _assert SELECT 'undirected, counted as OPTIONAL MATCH does',
    json(cypher('MATCH (a:A97) RETURN size([(a)-[*1..3]-(b) | 1]) AS n'))
    = json(cypher('MATCH (a:A97) OPTIONAL MATCH (a)-[*1..3]-(b) RETURN count(b) AS n'));
INSERT INTO _assert SELECT 'a type list',
    (SELECT group_concat(value) FROM (SELECT value FROM json_each(json_extract(cypher('MATCH (a:A97) RETURN [(a)-[:R97|S97]->(b) | b.id] AS l'), '$[0].l')) ORDER BY value)) = '2,4';
INSERT INTO _assert SELECT 'a type list on a var-length relationship, with a label',
    json(cypher('MATCH (a:A97) RETURN [(a)-[:R97|S97*1..2]->(b:C97) | b.id] AS l')) = json('[{"l":[3]}]');
INSERT INTO _assert SELECT 'a named path over it: nodes, relationships, length',
    json(cypher('MATCH (b:B97) RETURN [p = (b)<-[*1..1]-(a) | [x IN nodes(p) | x.id]] AS ns, [p = (b)<-[*1..1]-(a) | length(p)] AS ls'))
    = json('[{"ns":[[2,1]],"ls":[1]}]');
INSERT INTO _assert SELECT 'a named path of two hops',
    json(cypher('MATCH (c:C97) RETURN [p = (c)<-[:S97*2]-(a) | [x IN nodes(p) | x.id]] AS ns, [p = (c)<-[:S97*2]-(a) | size(relationships(p))] AS rs'))
    = json('[{"ns":[[3,2,1]],"rs":[2]}]');

SELECT 'Test 97 passed: ' || count(*) || ' assertions' AS result FROM _assert;
