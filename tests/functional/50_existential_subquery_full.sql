-- ========================================================================
-- Test 50: full existential subquery EXISTS { MATCH ... RETURN ... } (GQLITE-T-0139)
-- ========================================================================
-- PURPOSE: CIP2015-05-13-EXISTS level 3. The body is a read-only query
--          (MATCH / WITH / UNWIND / RETURN) correlated to the outer scope;
--          nested EXISTS { } and the brace pattern form inside it work.
--          Updating clauses inside the body raise
--          SyntaxError: InvalidClauseComposition (not asserted here: an
--          error aborts sqlite3 -bail; covered by the TCK and bindings).
-- COVERS:  openCypher TCK ExistentialSubquery2 [1]/[2], ExistentialSubquery3
--          [1]/[2]/[3]; GQLITE-T-0139 (Clotho report).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 50: full existential subquery ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A50 {prop: 1}), (b:B50 {prop: 1}), (c:C50 {prop: 2}), (d:D50 {prop: 3}) CREATE (a)-[:R]->(b), (a)-[:R]->(c), (a)-[:R]->(d)') as setup;

-- The Clotho query shape from the report: NOT EXISTS { MATCH ... }
SELECT cypher('CREATE (:Transcript50 {title: "orphan"}), (:Transcript50 {title: "linked"})<-[:EXTRACTED_FROM]-(:Entity50)') as setup2;
INSERT INTO _assert SELECT 'T-0139 1.1 NOT EXISTS { MATCH } filters linked transcripts',
    json(cypher('MATCH (t:Transcript50) WHERE NOT EXISTS { MATCH (e)-[:EXTRACTED_FROM]->(t) } RETURN t.title AS title'))
    = json('[{"title":"orphan"}]');

-- Full subquery with RETURN true, correlated on n.
-- (A `WITH n WHERE exists { ... }` variant hits the post-WITH id alias bug
-- tracked as GQLITE-T-0375 and is not asserted here.)
INSERT INTO _assert SELECT 'T-0139 1.2 EXISTS { MATCH (n)-->() RETURN true }',
    json(cypher('MATCH (n) WHERE (n:A50 OR n:B50 OR n:C50 OR n:D50) AND exists { MATCH (n)-->() RETURN true } RETURN labels(n)[0] AS l'))
    = json('[{"l":"A50"}]');

-- Aggregation + WHERE inside the body.
INSERT INTO _assert SELECT 'T-0139 1.3 EXISTS body with WITH count(*) WHERE',
    json(cypher('MATCH (n:A50) WHERE exists { MATCH (n)-->(m) WITH n, count(*) AS c WHERE c = 3 RETURN true } RETURN labels(n)[0] AS l'))
    = json('[{"l":"A50"}]');
INSERT INTO _assert SELECT 'T-0139 1.4 EXISTS body aggregation can fail the filter',
    json_array_length(cypher('MATCH (n:A50) WHERE exists { MATCH (n)-->(m) WITH n, count(*) AS c WHERE c = 99 RETURN true } RETURN n')) = 0;

-- Nested: full inside full, and brace-pattern inside full.
INSERT INTO _assert SELECT 'T-0139 2.1 nested full EXISTS',
    json(cypher('MATCH (n:A50) WHERE exists { MATCH (m:B50) WHERE exists { MATCH (l)<-[:R]-(n)-[:R]->(m) RETURN true } RETURN true } RETURN labels(n)[0] AS l'))
    = json('[{"l":"A50"}]');
INSERT INTO _assert SELECT 'T-0139 2.2 brace pattern with WHERE inside full EXISTS',
    json(cypher('MATCH (n:A50) WHERE exists { MATCH (m) WHERE exists { (n)-[]->(m) WHERE n.prop = m.prop } RETURN true } RETURN labels(n)[0] AS l'))
    = json('[{"l":"A50"}]');

-- Existing forms unaffected.
INSERT INTO _assert SELECT 'T-0139 3.1 EXISTS((pattern)) still works',
    json_array_length(cypher('MATCH (n:A50) WHERE EXISTS((n)-[:R]->()) RETURN n')) = 1;
INSERT INTO _assert SELECT 'T-0139 3.2 EXISTS { pattern } still works',
    json_array_length(cypher('MATCH (n:A50) WHERE EXISTS { (n)-[:R]->(:D50) } RETURN n')) = 1;

SELECT name, ok FROM _assert;
SELECT '=== Test 50 Complete ===' as test_section;
