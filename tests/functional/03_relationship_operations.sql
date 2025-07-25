-- ========================================================================
-- Test 03: Relationship Operations (CREATE, MATCH, Properties)
-- ========================================================================
-- PURPOSE: Comprehensive testing of relationship creation, direction handling,
--          relationship properties, and relationship pattern matching
-- COVERS:  Directed/undirected relationships, relationship types,
--          edge properties, relationship variables, complex patterns
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 03: Relationship Operations ===' as test_section;

-- =======================================================================
-- SECTION 1: Basic Relationship Creation Patterns
-- =======================================================================
SELECT '=== Section 1: Basic Relationship Creation Patterns ===' as section;

SELECT 'Test 1.1 - Simple directed relationship:' as test_name;
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS]->(b:Person {name: "Bob"})') as result;

SELECT 'Test 1.2 - Reverse directed relationship:' as test_name;
SELECT cypher('CREATE (c:Person {name: "Charlie"})<-[:FOLLOWS]-(d:Person {name: "David"})') as result;

SELECT 'Test 1.3 - Undirected relationship:' as test_name;
SELECT cypher('CREATE (e:Person {name: "Eve"})-[:FRIENDS]-(f:Person {name: "Frank"})') as result;

SELECT 'Test 1.4 - Relationship without type:' as test_name;
SELECT cypher('CREATE (g:Person {name: "Grace"})-[]->(h:Person {name: "Henry"})') as result;

SELECT 'Test 1.5 - Relationship with variable:' as test_name;
SELECT cypher('CREATE (i:Person {name: "Iris"})-[rel:MENTORS]->(j:Person {name: "Jack"})') as result;

-- =======================================================================
-- SECTION 2: Relationship Types and Properties
-- =======================================================================
SELECT '=== Section 2: Relationship Types and Properties ===' as section;

SELECT 'Test 2.1 - Multiple relationship types:' as test_name;
SELECT cypher('CREATE (k:Person {name: "Kelly"})-[:WORKS_FOR]->(l:Company {name: "TechCorp"})') as result;
SELECT cypher('CREATE (k:Person {name: "Kelly"})-[:LIVES_IN]->(m:City {name: "NYC"})') as result;

SELECT 'Test 2.2 - Relationship with integer property:' as test_name;
SELECT cypher('CREATE (n:Person {name: "Nina"})-[:KNOWS {since: 2020}]->(o:Person {name: "Oscar"})') as result;

SELECT 'Test 2.3 - Relationship with multiple property types:' as test_name;
SELECT cypher('CREATE (p:Person {name: "Paul"})-[:WORKS_WITH {years: 5, salary: 75000.50, verified: true, department: "Engineering"}]->(q:Person {name: "Quinn"})') as result;

SELECT 'Test 2.4 - Relationship with string properties:' as test_name;
SELECT cypher('CREATE (r:Person {name: "Rachel"})-[:COLLABORATES {project: "GraphDB", role: "lead"}]->(s:Person {name: "Sam"})') as result;

SELECT 'Test 2.5 - Relationship with empty properties:' as test_name;
SELECT cypher('CREATE (t:Person {name: "Tom"})-[:KNOWS {}]->(u:Person {name: "Uma"})') as result;

-- =======================================================================
-- SECTION 3: Different Relationship Directions
-- =======================================================================
SELECT '=== Section 3: Different Relationship Directions ===' as section;

SELECT 'Test 3.1 - Left-directed relationship with properties:' as test_name;
SELECT cypher('CREATE (v:Person {name: "Victor"})<-[:MENTORS {skill: "programming"}]-(w:Person {name: "Wendy"})') as result;

SELECT 'Test 3.2 - Bidirectional relationships:' as test_name;
SELECT cypher('CREATE (x:Person {name: "Xavier"})-[:FRIENDS {duration: "lifelong"}]-(y:Person {name: "Yuki"})') as result;

SELECT 'Test 3.3 - Multiple outgoing relationships:' as test_name;
SELECT cypher('CREATE (z:Person {name: "Zoe"})-[:MANAGES]->(aa:Person {name: "Alex"})') as result;
SELECT cypher('CREATE (z:Person {name: "Zoe"})-[:LEADS]->(bb:Person {name: "Blake"})') as result;

-- =======================================================================
-- SECTION 4: Complex Relationship Patterns
-- =======================================================================
SELECT '=== Section 4: Complex Relationship Patterns ===' as section;

SELECT 'Test 4.1 - Relationship chain creation:' as test_name;
SELECT cypher('CREATE (cc:Person {name: "Cameron"})-[:MANAGES]->(dd:Person {name: "Dana"})-[:REPORTS_TO]->(ee:Company {name: "MegaCorp"})') as result;

SELECT 'Test 4.2 - Multiple relationships in single CREATE:' as test_name;
SELECT cypher('CREATE (ff:Person {name: "Felix"})-[:KNOWS]->(gg:Person {name: "Gina"}), (ff)-[:WORKS_WITH]->(gg)') as result;

SELECT 'Test 4.3 - Self-referencing relationship:' as test_name;
SELECT cypher('CREATE (hh:Person {name: "Harper"})-[:MANAGES]->(hh)') as result;

-- =======================================================================
-- SECTION 5: Relationship Matching and Querying
-- =======================================================================
SELECT '=== Section 5: Relationship Matching and Querying ===' as section;

SELECT 'Test 5.1 - Match by relationship type:' as test_name;
SELECT cypher('MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name LIMIT 3') as result;

SELECT 'Test 5.2 - Match any relationship type:' as test_name;
SELECT cypher('MATCH (a)-[r]->(b) RETURN a.name, b.name LIMIT 3') as result;

SELECT 'Test 5.3 - Match undirected (both directions):' as test_name;
SELECT cypher('MATCH (a)-[:FRIENDS]-(b) RETURN a.name, b.name') as result;

SELECT 'Test 5.4 - Match reverse direction:' as test_name;
SELECT cypher('MATCH (a)<-[:FOLLOWS]-(b) RETURN a.name, b.name') as result;

SELECT 'Test 5.5 - Match with relationship variable:' as test_name;
SELECT cypher('MATCH (a)-[r:MENTORS]->(b) RETURN a.name, r, b.name') as result;

SELECT 'Test 5.6 - Match relationship without type constraint:' as test_name;
SELECT cypher('MATCH ()-[r]-() RETURN r LIMIT 3') as result;

-- =======================================================================
-- SECTION 6: Relationship Property Access
-- =======================================================================
SELECT '=== Section 6: Relationship Property Access ===' as section;

SELECT 'Test 6.1 - Return relationship with properties:' as test_name;
SELECT cypher('MATCH ()-[r:WORKS_WITH]->() RETURN r LIMIT 1') as result;

SELECT 'Test 6.2 - Return relationship properties separately:' as test_name;
SELECT cypher('MATCH (a)-[r:COLLABORATES]->(b) RETURN a.name, r.project, r.role, b.name') as result;

SELECT 'Test 6.3 - Filter by relationship property:' as test_name;
SELECT cypher('MATCH (a)-[r:KNOWS]->(b) WHERE r.since = 2020 RETURN a.name, b.name') as result;

-- =======================================================================
-- SECTION 7: Advanced Relationship Patterns
-- =======================================================================
SELECT '=== Section 7: Advanced Relationship Patterns ===' as section;

SELECT 'Test 7.1 - Match relationship chains:' as test_name;
SELECT cypher('MATCH (a)-[:MANAGES]->(b)-[:REPORTS_TO]->(c) RETURN a.name, b.name, c.name') as result;

SELECT 'Test 7.2 - Match multiple relationships from same node:' as test_name;
SELECT cypher('MATCH (a:Person)-[:WORKS_FOR]->(company) RETURN a.name, company.name LIMIT 2') as result;

SELECT 'Test 7.3 - Match nodes with specific relationship counts:' as test_name;
SELECT cypher('MATCH (a)-[]->(b) RETURN a.name ORDER BY a.name LIMIT 3') as result;

SELECT 'Test 7.4 - Match nodes by label with relationships:' as test_name;
SELECT cypher('MATCH (p:Person)-[r]->(target) RETURN p.name, target ORDER BY p.name LIMIT 3') as result;

-- =======================================================================
-- SECTION 8: Edge Cases and Error Conditions
-- =======================================================================
SELECT '=== Section 8: Edge Cases and Error Conditions ===' as section;

SELECT 'Test 8.1 - Non-existent relationship type:' as test_name;
SELECT cypher('MATCH (a)-[:NONEXISTENT]->(b) RETURN a, b') as result;

SELECT 'Test 8.2 - Relationships between non-existent nodes:' as test_name;
SELECT cypher('MATCH (a:Robot)-[:KNOWS]->(b:Alien) RETURN a, b') as result;

SELECT 'Test 8.3 - Empty relationship pattern:' as test_name;
SELECT cypher('MATCH ()-[]->() RETURN 1 LIMIT 1') as result;

-- =======================================================================
-- VERIFICATION: Database State Analysis
-- =======================================================================
SELECT '=== Verification: Database State Analysis ===' as section;

SELECT 'Total edges created:' as test_name;
SELECT COUNT(*) as total_edges FROM edges;

SELECT 'Edge types and counts:' as test_name;
.mode column
.headers on
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY count DESC, type;

SELECT 'Edge directions verification:' as test_name;
SELECT e.id, e.type, 
       n1.id as source_node_id, n2.id as target_node_id
FROM edges e 
JOIN nodes n1 ON e.source_id = n1.id 
JOIN nodes n2 ON e.target_id = n2.id 
ORDER BY e.id LIMIT 5;

SELECT 'Node degree analysis:' as test_name;
SELECT 
    'outgoing' as direction,
    COUNT(*) as total_relationships,
    COUNT(DISTINCT source_id) as nodes_with_outgoing
FROM edges
UNION ALL
SELECT 
    'incoming' as direction,
    COUNT(*) as total_relationships,
    COUNT(DISTINCT target_id) as nodes_with_incoming  
FROM edges;

SELECT 'Relationship property summary:' as test_name;
SELECT COUNT(*) as edges_with_properties FROM edges WHERE id IN (
    SELECT DISTINCT edge_id FROM edge_props_text
    UNION
    SELECT DISTINCT edge_id FROM edge_props_int
    UNION 
    SELECT DISTINCT edge_id FROM edge_props_real
    UNION
    SELECT DISTINCT edge_id FROM edge_props_bool
);

-- Cleanup note
SELECT '=== Relationship Operations Test Complete ===' as section;
SELECT 'Database contains comprehensive relationship test data' as note;