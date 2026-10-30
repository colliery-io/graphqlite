-- ========================================================================
-- Test 92: label disjunction :A|B outside MATCH (GQLITE-T-0439)
-- ========================================================================
-- PURPOSE: `:A|B` was read only by MATCH. Elsewhere the node had no label:
--          - a pattern predicate `(n)-->(:A|B)` matched any node,
--          - the same with a var-length relationship,
--          - a pattern comprehension `[(n)-->(m:A|B) | m]`,
--          - the RETURN after REMOVE re-matched the pattern with the
--            alternatives still on, and lost the row whose label was removed,
--          - CREATE or MERGE inside FOREACH took `:A|B` silently.
--          Found by code review of 0.12.0.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 92: label disjunction outside MATCH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:S92 {n: 1})-[:R92]->(:A92), (:S92 {n: 2})-[:R92]->(:C92), (:S92 {n: 3})-[:R92]->(:B92), (:S92 {n: 4})-[:R92]->(:Q92)-[:R92]->(:A92)') as setup;

INSERT INTO _assert SELECT 'pattern predicate',
    json(cypher('MATCH (n:S92) WHERE (n)-->(:A92|B92) RETURN n.n AS n ORDER BY n'))
    = json('[{"n":1},{"n":3}]');
INSERT INTO _assert SELECT 'negated pattern predicate',
    json(cypher('MATCH (n:S92) WHERE NOT (n)-->(:A92|B92) RETURN n.n AS n ORDER BY n'))
    = json('[{"n":2},{"n":4}]');
INSERT INTO _assert SELECT 'var-length pattern predicate',
    json(cypher('MATCH (n:S92) WHERE (n)-[*1..2]->(:A92|B92) RETURN n.n AS n ORDER BY n'))
    = json('[{"n":1},{"n":3},{"n":4}]');
INSERT INTO _assert SELECT 'pattern comprehension',
    json(cypher('MATCH (n:S92) RETURN n.n AS n, size([(n)-->(m:A92|B92) | m]) AS k ORDER BY n'))
    = json('[{"n":1,"k":1},{"n":2,"k":0},{"n":3,"k":1},{"n":4,"k":0}]');

SELECT cypher('CREATE (:D92 {k: 1}), (:E92 {k: 2})') as setup2;
INSERT INTO _assert SELECT 'RETURN after REMOVE of the matched label keeps the row',
    json(cypher('MATCH (n:D92|E92) REMOVE n:D92 RETURN n.k AS k, labels(n) AS l ORDER BY k'))
    = json('[{"k":1,"l":[]},{"k":2,"l":["E92"]}]');
INSERT INTO _assert SELECT 'CREATE in FOREACH rejects it',
    json_extract(cypher_validate('FOREACH (x IN [1] | CREATE (:A92|B92))'), '$.error') LIKE 'SyntaxError: InvalidLabelExpression:%';
INSERT INTO _assert SELECT 'MERGE in a nested FOREACH rejects it',
    json_extract(cypher_validate('FOREACH (x IN [1] | FOREACH (y IN [1] | MERGE (:A92|B92)))'), '$.error') LIKE 'SyntaxError: InvalidLabelExpression:%';

SELECT 'Test 92 passed: ' || count(*) || ' assertions' AS result FROM _assert;
