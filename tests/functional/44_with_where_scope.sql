-- ========================================================================
-- Test 44: WITH ... WHERE scope (GQLITE-T-0324)
-- ========================================================================
-- PURPOSE: The WHERE that belongs to a WITH clause sees BOTH the variables
--          bound before the WITH and the aliases the WITH projects. Clauses
--          after the WITH see only the projected aliases.
-- COVERS:  openCypher TCK WithWhere7 [3]; GQLITE-T-0324.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 44: WITH WHERE scope ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:W44 {name2: "A"}), (:W44 {name2: "B"}), (:W44 {name2: "C"})') as setup;

-- Mixed scope: projected alias AND pre-WITH variable in one WHERE
INSERT INTO _assert SELECT 'T-0324 1.1 mixed-scope row count',
    json_array_length(cypher('MATCH (a:W44) WITH a.name2 AS name WHERE name = "B" OR a.name2 = "C" RETURN name ORDER BY name')) = 2;
INSERT INTO _assert SELECT 'T-0324 1.2 mixed-scope values',
    json(cypher('MATCH (a:W44) WITH a.name2 AS name WHERE name = "B" OR a.name2 = "C" RETURN name ORDER BY name'))
    = json('[{"name":"B"},{"name":"C"}]');

-- Projected alias only (post-WITH path still works)
INSERT INTO _assert SELECT 'T-0324 2.1 alias-only filter',
    json(cypher('MATCH (a:W44) WITH a.name2 AS name WHERE name = "B" RETURN name')) = json('[{"name":"B"}]');

-- Pre-WITH variable only (pre-WITH path still works)
INSERT INTO _assert SELECT 'T-0324 2.2 pre-WITH-only filter',
    json(cypher('MATCH (a:W44) WITH a.name2 AS name WHERE a.name2 = "C" RETURN name')) = json('[{"name":"C"}]');

-- Aggregate alias in WHERE is still a post-grouping filter
INSERT INTO _assert SELECT 'T-0324 2.3 aggregate alias filter',
    json(cypher('MATCH (a:W44) WITH count(a) AS n WHERE n = 3 RETURN n')) = json('[{"n":3}]');

-- Strict scope after the WITH (`a` not visible to RETURN) is a runtime
-- error, which sqlite3 -bail would abort on; it is covered by the TCK
-- With*/WithWhere* UndefinedVariable scenarios instead.

SELECT name, ok FROM _assert;
SELECT '=== Test 44 Complete ===' as test_section;
