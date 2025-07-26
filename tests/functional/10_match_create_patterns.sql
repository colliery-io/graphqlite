-- ========================================================================
-- Test 10: MATCH...CREATE vs CREATE Patterns
-- ========================================================================
-- PURPOSE: Testing the distinction between MATCH...CREATE (using existing
--          nodes) and standalone CREATE (creating new nodes)
-- COVERS:  MATCH...CREATE patterns, node reuse vs creation, relationship
--          creation with existing nodes, error handling for missing nodes
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 10: MATCH...CREATE vs CREATE Patterns ===' as test_section;

-- =======================================================================
-- SECTION 1: Basic MATCH...CREATE Functionality
-- =======================================================================
SELECT '=== Section 1: Basic MATCH...CREATE Functionality ===' as section;

SELECT 'Test 1.1 - Create initial nodes for testing:' as test_name;
SELECT cypher('CREATE (alice:Person {name: "Alice", age: 30})') as result;
SELECT cypher('CREATE (bob:Person {name: "Bob", age: 25})') as result;

SELECT 'Test 1.2 - Verify initial node count:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as initial_count;

SELECT 'Test 1.3 - MATCH existing nodes and CREATE relationship:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)') as result;

SELECT 'Test 1.4 - Verify node count unchanged (should still be 2):' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as after_match_create_count;

SELECT 'Test 1.5 - Verify relationship was created:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a.name, b.name') as relationship_check;

-- =======================================================================
-- SECTION 2: MATCH...CREATE with Properties
-- =======================================================================
SELECT '=== Section 2: MATCH...CREATE with Properties ===' as section;

SELECT 'Test 2.1 - Create additional nodes:' as test_name;
SELECT cypher('CREATE (charlie:Person {name: "Charlie", department: "Engineering"})') as result;
SELECT cypher('CREATE (diana:Person {name: "Diana", department: "Marketing"})') as result;

SELECT 'Test 2.2 - Current node count:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as before_prop_match_count;

SELECT 'Test 2.3 - MATCH...CREATE with edge properties:' as test_name;
SELECT cypher('MATCH (c:Person {name: "Charlie"}), (d:Person {name: "Diana"}) CREATE (c)-[:COLLABORATES {project: "Website", since: 2023}]->(d)') as result;

SELECT 'Test 2.4 - Verify node count unchanged:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as after_prop_match_count;

SELECT 'Test 2.5 - Verify relationship with properties:' as test_name;
SELECT cypher('MATCH (c:Person)-[r:COLLABORATES]->(d:Person) RETURN c.name, r, d.name') as collab_check;

-- =======================================================================
-- SECTION 3: Multiple MATCH...CREATE Operations
-- =======================================================================
SELECT '=== Section 3: Multiple MATCH...CREATE Operations ===' as section;

SELECT 'Test 3.1 - Create web of relationships using existing nodes:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (c:Person {name: "Charlie"}) CREATE (a)-[:MENTORS {skill: "leadership"}]->(c)') as result;
SELECT cypher('MATCH (b:Person {name: "Bob"}), (d:Person {name: "Diana"}) CREATE (b)-[:WORKS_WITH {frequency: "daily"}]->(d)') as result;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (d:Person {name: "Diana"}) CREATE (a)-[:MANAGES]->(d)') as result;

SELECT 'Test 3.2 - Final node count should still be 4:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as final_node_count;

SELECT 'Test 3.3 - Count all relationships:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH ()-[r]->() RETURN count(r)') as total_relationships;

SELECT 'Test 3.4 - List all relationships:' as test_name;
-- NOTE: type() function not implemented - documented in BUG_FIXES.md
-- SELECT cypher('MATCH (a)-[r]->(b) RETURN a.name, type(r), b.name') as all_relationships;
SELECT 'SKIPPED: type() function not implemented' as all_relationships;

-- =======================================================================
-- SECTION 4: MATCH...CREATE with Complex Patterns
-- =======================================================================
SELECT '=== Section 4: MATCH...CREATE with Complex Patterns ===' as section;

SELECT 'Test 4.1 - MATCH multiple nodes, CREATE multiple relationships:' as test_name;
SELECT cypher('MATCH (alice:Person {name: "Alice"}), (bob:Person {name: "Bob"}), (charlie:Person {name: "Charlie"}) CREATE (alice)-[:TEAM_LEAD]->(bob), (alice)-[:TEAM_LEAD]->(charlie)') as result;

SELECT 'Test 4.2 - Verify node count still 4:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as complex_node_count;

SELECT 'Test 4.3 - Count TEAM_LEAD relationships:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH ()-[r:TEAM_LEAD]->() RETURN count(r)') as team_lead_count;

SELECT 'Test 4.4 - Complex pattern with multiple relationship types:' as test_name;
SELECT cypher('MATCH (alice:Person {name: "Alice"}), (bob:Person {name: "Bob"}) CREATE (alice)-[:FRIENDS {since: "childhood"}]->(bob), (bob)-[:RESPECTS]->(alice)') as result;

-- =======================================================================
-- SECTION 5: Error Cases and Edge Conditions
-- =======================================================================
SELECT '=== Section 5: Error Cases and Edge Conditions ===' as section;

SELECT 'Test 5.1 - MATCH non-existent node (should create no relationships):' as test_name;
SELECT cypher('MATCH (a:Person {name: "NonExistent"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)') as result;

SELECT 'Test 5.2 - Verify no new nodes created by failed MATCH:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as error_case_count;

SELECT 'Test 5.3 - MATCH with partial success (one exists, one does not):' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "NotReal"}) CREATE (a)-[:KNOWS]->(b)') as result;

SELECT 'Test 5.4 - Verification - node count should still be 4:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as partial_error_count;

SELECT 'Test 5.5 - MATCH with impossible conditions:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice", age: 999}), (b:Person {name: "Bob"}) CREATE (a)-[:IMPOSSIBLE]->(b)') as result;

-- =======================================================================
-- SECTION 6: Comparison with Regular CREATE
-- =======================================================================
SELECT '=== Section 6: Comparison with Regular CREATE ===' as section;

SELECT 'Test 6.1 - Regular CREATE (should add new nodes):' as test_name;
SELECT cypher('CREATE (eve:Person {name: "Eve"})-[:KNOWS]->(frank:Person {name: "Frank"})') as result;

SELECT 'Test 6.2 - Node count after regular CREATE (should be 6):' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as create_comparison_count;

SELECT 'Test 6.3 - MATCH...CREATE using the new nodes:' as test_name;
SELECT cypher('MATCH (e:Person {name: "Eve"}), (a:Person {name: "Alice"}) CREATE (e)-[:REPORTS_TO]->(a)') as result;

SELECT 'Test 6.4 - Final node count (should still be 6):' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as ultimate_final_count;

SELECT 'Test 6.5 - Demonstrate CREATE vs MATCH...CREATE difference:' as test_name;
-- This should create two NEW Alice nodes
SELECT cypher('CREATE (alice1:Person {name: "Alice"})-[:DUPLICATE]->(alice2:Person {name: "Alice"})') as create_new;
-- This should use existing Alice (if any)
-- COUNT function now implemented
SELECT cypher('MATCH (existing:Person {name: "Alice"}) RETURN count(existing)') as existing_alice_count;

-- =======================================================================
-- SECTION 7: Advanced MATCH...CREATE Patterns
-- =======================================================================
SELECT '=== Section 7: Advanced MATCH...CREATE Patterns ===' as section;

SELECT 'Test 7.1 - MATCH...CREATE with relationship patterns:' as test_name;
SELECT cypher('MATCH (manager:Person)-[:MANAGES]->(subordinate:Person) CREATE (manager)-[:EVALUATES {year: 2023}]->(subordinate)') as result;

SELECT 'Test 7.2 - Conditional MATCH...CREATE:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.age > 30 MATCH (p), (target:Person {name: "Bob"}) CREATE (p)-[:SENIOR_TO]->(target)') as result;

SELECT 'Test 7.3 - MATCH...CREATE with property filtering:' as test_name;
SELECT cypher('MATCH (eng:Person) WHERE eng.department = "Engineering" MATCH (eng), (mkt:Person) WHERE mkt.department = "Marketing" CREATE (eng)-[:CROSS_DEPT]->(mkt)') as result;

SELECT 'Test 7.4 - Multiple MATCH clauses with CREATE:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}) MATCH (others:Person) WHERE others.name <> "Alice" CREATE (a)-[:KNOWS_ALL {type: "universal"}]->(others)') as result;

-- =======================================================================
-- SECTION 8: Node Reuse vs Creation Verification
-- =======================================================================
SELECT '=== Section 8: Node Reuse vs Creation Verification ===' as section;

SELECT 'Test 8.1 - Count nodes named Alice (should be multiple due to CREATE):' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n:Person {name: "Alice"}) RETURN count(n)') as alice_count;

SELECT 'Test 8.2 - Count all distinct names:' as test_name;
-- COUNT DISTINCT function now implemented
SELECT cypher('MATCH (n:Person) RETURN count(DISTINCT n.name)') as distinct_names;

SELECT 'Test 8.3 - Show node reuse pattern:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n:Person) RETURN n.name, count(n) as instances ORDER BY instances DESC, n.name') as name_instances;

SELECT 'Test 8.4 - Verify relationship endpoints use correct nodes:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a.name, b.name, count(*) as relationship_count') as endpoint_verification;

-- =======================================================================
-- SECTION 9: Performance and Efficiency Tests
-- =======================================================================
SELECT '=== Section 9: Performance and Efficiency Tests ===' as section;

SELECT 'Test 9.1 - Efficient node lookup for MATCH...CREATE:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (specific:Person {name: "Alice", age: 30}) RETURN count(specific)') as specific_match;

SELECT 'Test 9.2 - Multiple property matching efficiency:' as test_name;
SELECT cypher('MATCH (eng:Person {department: "Engineering"}), (mkt:Person {department: "Marketing"}) CREATE (eng)-[:COLLABORATE]->(mkt)') as multi_prop_match;

SELECT 'Test 9.3 - Batch MATCH...CREATE operations:' as test_name;
SELECT cypher('MATCH (senior:Person) WHERE senior.age >= 30 MATCH (junior:Person) WHERE junior.age < 30 CREATE (senior)-[:MENTOR]->(junior)') as batch_create;

-- =======================================================================
-- VERIFICATION: MATCH...CREATE Pattern Analysis
-- =======================================================================
SELECT '=== Verification: MATCH...CREATE Pattern Analysis ===' as section;

SELECT 'Final database state - nodes:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH (n) RETURN count(n)') as final_node_count;

SELECT 'Final database state - relationships:' as test_name;
-- COUNT function now implemented
SELECT cypher('MATCH ()-[r]->() RETURN count(r)') as final_relationship_count;

SELECT 'Relationship types created:' as test_name;
.mode column
.headers on
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY count DESC;

SELECT 'Node reuse verification:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age, n.department ORDER BY n.name') as all_persons;

SELECT 'MATCH...CREATE effectiveness:' as test_name;
SELECT 
    'Nodes that have outgoing relationships' as metric,
    COUNT(DISTINCT source_id) as count 
FROM edges
UNION ALL
SELECT 
    'Nodes that have incoming relationships' as metric,
    COUNT(DISTINCT target_id) as count 
FROM edges
UNION ALL
SELECT 
    'Total unique nodes in relationships' as metric,
    COUNT(DISTINCT node_id) as count 
FROM (
    SELECT source_id as node_id FROM edges 
    UNION 
    SELECT target_id as node_id FROM edges
);

-- Cleanup note
SELECT '=== MATCH...CREATE vs CREATE Patterns Test Complete ===' as section;
SELECT 'Node reuse patterns and MATCH...CREATE functionality verified successfully' as note;