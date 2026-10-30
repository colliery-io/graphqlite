-- ========================================================================
-- Test 90: property id / labels / type of an entity value (GQLITE-T-0474)
-- ========================================================================
-- PURPOSE: a node or relationship held as a value (an element of nodes(p),
--          a list comprehension variable) is its JSON, and `x.id`,
--          `x.labels`, `x.type` read the JSON's own keys: the internal id,
--          the label list, the relationship type. They are properties, as
--          on a node variable.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 90: properties of entity values ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A90 {id: 10, labels: ''L'', n: 1})-[:R90 {id: 7, type: ''T''}]->(:B90 {id: 20})') as setup;

INSERT INTO _assert SELECT 'nodes(p): id and labels are properties',
    json(cypher('MATCH p = (a:A90)-->(b) RETURN [x IN nodes(p) | x.id] AS ids, [x IN nodes(p) | x.labels] AS ls'))
    = json('[{"ids":[10,20],"ls":["L",null]}]');
INSERT INTO _assert SELECT 'relationships(p): id and type are properties',
    json(cypher('MATCH p = (a:A90)-->(b) RETURN [x IN relationships(p) | x.id] AS ids, [x IN relationships(p) | x.type] AS ts'))
    = json('[{"ids":[7],"ts":["T"]}]');
INSERT INTO _assert SELECT 'a node in a list',
    json(cypher('MATCH (a:A90) RETURN [x IN [a] | x.id] AS ids')) = json('[{"ids":[10]}]');
INSERT INTO _assert SELECT 'id() and labels() still read the entity',
    json(cypher('MATCH p = (a:A90)-->(b) RETURN [x IN nodes(p) | labels(x)] AS ls, [x IN relationships(p) | type(x)] AS ts'))
    = json('[{"ls":[["A90"],["B90"]],"ts":["R90"]}]');
INSERT INTO _assert SELECT 'a plain map reads its own keys',
    json(cypher('RETURN [m IN [{id: 1, type: ''t''}] | m.id] AS ids, {type: ''t''}.type AS t'))
    = json('[{"ids":[1],"t":"t"}]');

SELECT 'Test 90 passed: ' || count(*) || ' assertions' AS result FROM _assert;
