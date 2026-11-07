-- ========================================================================
-- Test 123: property maps in pattern predicates (GQLITE-T-0514)
-- ========================================================================
-- PURPOSE: WHERE (n)-[{rel: 1}]->() matched any relationship: the pattern
--          predicate ignored property maps. It now runs as EXISTS { MATCH
--          pattern }. A variable-length pattern in an EXISTS / COUNT
--          subquery body gave invalid SQL (no FROM); it now runs.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 123: pattern predicate properties ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:S123 {id: 1, p: 12})-[:R {rel: 1}]->({prop: 42}), (:S123 {id: 2, p: 25})-[:R {rel: 2}]->({prop: 1}), (:S123 {id: 3})') as setup;
SELECT cypher('CREATE (:V123 {id: 1})-[:T {w: 42}]->()-[:T {w: 42}]->(), (:V123 {id: 2})-[:T {w: 42}]->()-[:T {w: 1}]->()') as setup2;

INSERT INTO _assert SELECT 'relationship property map',
    json(cypher('MATCH (n:S123) WHERE (n)-[{rel: 1}]->() RETURN n.id AS id')) = json('[{"id":1}]');
INSERT INTO _assert SELECT 'under NOT',
    json(cypher('MATCH (n:S123) WHERE NOT (n)-[{rel: 1}]->() RETURN n.id AS id ORDER BY id')) = json('[{"id":2},{"id":3}]');
INSERT INTO _assert SELECT 'node property map under OR',
    json(cypher('MATCH (n:S123) WHERE (n)-->({prop: 42}) OR NOT (n)-->() RETURN n.id AS id ORDER BY id')) = json('[{"id":1},{"id":3}]');
INSERT INTO _assert SELECT 'variable length with a property map',
    json(cypher('MATCH (n:V123) WHERE (n)-[*2 {w: 42}]->() RETURN n.id AS id')) = json('[{"id":1}]');
INSERT INTO _assert SELECT 'variable length in EXISTS and COUNT bodies',
    json(cypher('MATCH (n:V123) WHERE EXISTS { MATCH (n)-[*2]->() } RETURN n.id AS id, COUNT { (n)-[*1..2]->() } AS c ORDER BY id'))
    = json('[{"id":1,"c":2},{"id":2,"c":2}]');

SELECT 'Test 123 passed: ' || count(*) || ' assertions' AS result FROM _assert;
