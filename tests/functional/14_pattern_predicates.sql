-- ========================================================================
-- Test 14: Pattern Predicates (Bare Relationship Patterns in WHERE)
-- ========================================================================
-- PURPOSE: Validates pattern predicates per openCypher 9 spec where a
--          RelationshipsPattern in boolean context is an implicit EXISTS.
--          e.g., WHERE (n)-[:REL]->() is equivalent to WHERE EXISTS((n)-[:REL]->())
-- COVERS:  Outgoing, incoming, undirected directions; typed and untyped
--          relationships; NOT, AND, OR combinations; compatibility with EXISTS()
-- ========================================================================

.load ./build/graphqlite

-- Clean up any existing test data
SELECT cypher('MATCH (n:PatPredTest) DETACH DELETE n') as cleanup;

-- =======================================================================
-- SECTION 1: Setup Test Data
-- =======================================================================
SELECT '=== Section 1: Setup Test Data ===' as section;

SELECT cypher('CREATE (:PatPredTest:Person {name: "Alice"})') as setup1;
SELECT cypher('CREATE (:PatPredTest:Person {name: "Bob"})') as setup2;
SELECT cypher('CREATE (:PatPredTest:Person {name: "Charlie"})') as setup3;
SELECT cypher('CREATE (:PatPredTest:Team {name: "Alpha"})') as setup4;

-- Alice KNOWS Bob, Alice WORKS_WITH Charlie, Bob BELONGS_TO Alpha
SELECT cypher('MATCH (a:PatPredTest {name: "Alice"}), (b:PatPredTest {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)') as rel1;
SELECT cypher('MATCH (a:PatPredTest {name: "Alice"}), (c:PatPredTest {name: "Charlie"}) CREATE (a)-[:WORKS_WITH]->(c)') as rel2;
SELECT cypher('MATCH (b:PatPredTest {name: "Bob"}), (t:PatPredTest {name: "Alpha"}) CREATE (b)-[:BELONGS_TO]->(t)') as rel3;

-- =======================================================================
-- SECTION 2: Outgoing Pattern Predicates
-- =======================================================================
SELECT '=== Section 2: Outgoing Pattern Predicates ===' as section;

SELECT 'Test 2.1 - Positive outgoing typed:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[:KNOWS]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Alice (only she has outgoing KNOWS)

SELECT 'Test 2.2 - NOT outgoing typed:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT (n)-[:KNOWS]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Bob, Charlie

SELECT 'Test 2.3 - Outgoing untyped:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Alice (KNOWS, WORKS_WITH), Bob (BELONGS_TO)

SELECT 'Test 2.4 - NOT outgoing untyped (no outgoing rels at all):' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT (n)-[]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Charlie (has no outgoing rels)

-- =======================================================================
-- SECTION 3: Incoming Pattern Predicates
-- =======================================================================
SELECT '=== Section 3: Incoming Pattern Predicates ===' as section;

SELECT 'Test 3.1 - Incoming typed:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)<-[:KNOWS]-() RETURN n.name ORDER BY n.name') as result;
-- Expected: Bob (Alice KNOWS Bob)

SELECT 'Test 3.2 - NOT incoming typed:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT (n)<-[:KNOWS]-() RETURN n.name ORDER BY n.name') as result;
-- Expected: Alice, Charlie

SELECT 'Test 3.3 - Incoming to team:' as test_name;
SELECT cypher('MATCH (t:PatPredTest:Team) WHERE (t)<-[:BELONGS_TO]-() RETURN t.name') as result;
-- Expected: Alpha

-- =======================================================================
-- SECTION 4: Undirected Pattern Predicates
-- =======================================================================
SELECT '=== Section 4: Undirected Pattern Predicates ===' as section;

SELECT 'Test 4.1 - Undirected typed:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[:KNOWS]-() RETURN n.name ORDER BY n.name') as result;
-- Expected: Alice (outgoing), Bob (incoming) - both participate in KNOWS

SELECT 'Test 4.2 - NOT undirected typed:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT (n)-[:KNOWS]-() RETURN n.name ORDER BY n.name') as result;
-- Expected: Charlie

-- =======================================================================
-- SECTION 5: Combined Boolean Expressions
-- =======================================================================
SELECT '=== Section 5: Combined Boolean Expressions ===' as section;

SELECT 'Test 5.1 - AND with two pattern predicates:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[:KNOWS]->() AND (n)-[:WORKS_WITH]->() RETURN n.name') as result;
-- Expected: Alice (has both)

SELECT 'Test 5.2 - OR with two pattern predicates:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[:KNOWS]->() OR (n)-[:BELONGS_TO]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Alice, Bob

SELECT 'Test 5.3 - NOT AND NOT (no outgoing rels of these types):' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT (n)-[:KNOWS]->() AND NOT (n)-[:WORKS_WITH]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Bob, Charlie (neither has KNOWS or WORKS_WITH outgoing)
-- Wait: Bob has BELONGS_TO but not KNOWS or WORKS_WITH, so Bob is included

SELECT 'Test 5.4 - Mixed with regular conditions:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE n.name > "B" AND NOT (n)-[:KNOWS]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Bob, Charlie (name > "B" and no outgoing KNOWS)

SELECT 'Test 5.5 - Pattern predicate with XOR:' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[:KNOWS]->() XOR (n)-[:BELONGS_TO]->() RETURN n.name ORDER BY n.name') as result;
-- Expected: Alice (has KNOWS, no BELONGS_TO), Bob (has BELONGS_TO, no KNOWS)

-- =======================================================================
-- SECTION 6: Equivalence with EXISTS()
-- =======================================================================
SELECT '=== Section 6: Equivalence with EXISTS ===' as section;

SELECT 'Test 6.1 - Pattern predicate vs EXISTS (outgoing):' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE (n)-[:KNOWS]->() RETURN n.name ORDER BY n.name') as pred_result;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE EXISTS((n)-[:KNOWS]->()) RETURN n.name ORDER BY n.name') as exists_result;
-- Both should return: Alice

SELECT 'Test 6.2 - NOT pattern predicate vs NOT EXISTS (incoming):' as test_name;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT (n)<-[:KNOWS]-() RETURN n.name ORDER BY n.name') as pred_result;
SELECT cypher('MATCH (n:PatPredTest:Person) WHERE NOT EXISTS((n)<-[:KNOWS]-()) RETURN n.name ORDER BY n.name') as exists_result;
-- Both should return: Alice, Charlie

SELECT '=== Pattern Predicate Tests Complete ===' as done;
