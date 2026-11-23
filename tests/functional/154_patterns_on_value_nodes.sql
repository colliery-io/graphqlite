-- ========================================================================
-- Test 154: patterns that start at a node held as a value (GQLITE-T-0553)
-- ========================================================================
-- PURPOSE: the variable of a list comprehension, of reduce() and of a list
--          predicate holds a node as a value when the list is a list of
--          nodes. A pattern that starts at it:
--          - a pattern comprehension with a path variable read
--            `<alias>.id` ("no such column: _je_0.value.id")
--          - a pattern predicate compared the relationship with the JSON
--            text of the node: always false, with no error
--          - COUNT { } / EXISTS { } put the table of the value in their own
--            FROM ("no such table: _je_0")
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 154: patterns on node values ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (n1:X154 {n: 1})-[:T]->(m1:Y154 {k: ''m1''}), (m1)-[:T]->(:Y154 {k: ''i1''}), (m1)-[:T]->(:Y154 {k: ''i2''}),
                      (n2:X154 {n: 2})-[:T]->(m2:M154 {k: ''m2''}), (m2)-[:T]->(:L154 {k: ''i3''}), (m2)-[:T]->(:Y154 {k: ''i4''})') as setup;

INSERT INTO _assert SELECT 'a pattern comprehension with a path variable, inside a list comprehension',
    json(cypher('MATCH p = (n:X154)-->(b) RETURN n.n AS n, [x IN nodes(p) | size([p = (x)-->(:Y154) | p])] AS list, length(p) AS len ORDER BY n'))
    = json('[{"n":1,"list":[1,2],"len":1},{"n":2,"list":[0,1],"len":1}]');

INSERT INTO _assert SELECT 'the path of the inner pattern starts at the value node',
    json(cypher('MATCH (n:X154 {n: 1}) RETURN [x IN [n] | [q = (x)-->(y) | [nodes(q)[0].n, nodes(q)[1].k, length(q)]]] AS l'))
    = json('[{"l":[[[1,"m1",1]]]}]');

INSERT INTO _assert SELECT 'without a path variable (as before)',
    json(cypher('MATCH (n:X154) RETURN n.n AS n, [x IN [n] | [(x)-->(y) | y.k]] AS l ORDER BY n'))
    = json('[{"n":1,"l":[["m1"]]},{"n":2,"l":[["m2"]]}]');

INSERT INTO _assert SELECT 'a pattern predicate in the WHERE of a list comprehension',
    json(cypher('MATCH (n:X154) RETURN n.n AS n, [x IN [n] WHERE (x)-->(:Y154) | x.n] AS l ORDER BY n'))
    = json('[{"n":1,"l":[1]},{"n":2,"l":[]}]');

INSERT INTO _assert SELECT 'a pattern predicate in a list predicate',
    json(cypher('MATCH (n:X154) RETURN n.n AS n, any(x IN [n] WHERE (x)-->(:Y154)) AS a, none(x IN [n] WHERE (x)-->(:M154)) AS b ORDER BY n'))
    = json('[{"n":1,"a":true,"b":true},{"n":2,"a":false,"b":false}]');

INSERT INTO _assert SELECT 'a pattern comprehension in reduce()',
    json(cypher('MATCH p = (n:X154)-->(b) RETURN n.n AS n, reduce(s = 0, x IN nodes(p) | s + size([(x)-->() | 1])) AS r ORDER BY n'))
    = json('[{"n":1,"r":3},{"n":2,"r":3}]');

INSERT INTO _assert SELECT 'COUNT { } and EXISTS { } on the variable of a list comprehension',
    json(cypher('MATCH p = (n:X154)-->(b) RETURN n.n AS n, [x IN nodes(p) | COUNT { (x)-->(:Y154) }] AS c, [x IN nodes(p) | EXISTS { (x)-->(:L154) }] AS e ORDER BY n'))
    = json('[{"n":1,"c":[1,2],"e":[false,false]},{"n":2,"c":[0,1],"e":[false,true]}]');

INSERT INTO _assert SELECT 'COUNT { MATCH … } with a WHERE on the value node',
    json(cypher('MATCH (n:X154) RETURN n.n AS n, [x IN [n] | COUNT { MATCH (x)-->(y) WHERE y.k STARTS WITH ''m'' }] AS c ORDER BY n'))
    = json('[{"n":1,"c":[1]},{"n":2,"c":[1]}]');

SELECT 'Test 154 passed: ' || count(*) || ' assertions' AS result FROM _assert;
