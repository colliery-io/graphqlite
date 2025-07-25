-- Test 04: Comprehensive Relationship Operations
-- Tests all aspects of relationship creation and querying

.load ./build/graphqlite

.print "=== Test 04: Comprehensive Relationship Operations ==="

-- SECTION 1: Relationship Creation Patterns
.print "=== SECTION 1: Relationship Creation Patterns ===";

-- Test 1.1: Simple directed relationship
.print "Test 1.1 - Simple directed relationship:";
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS]->(b:Person {name: "Bob"})') as result;

-- Test 1.2: Reverse directed relationship
.print "Test 1.2 - Reverse directed relationship:";
SELECT cypher('CREATE (c:Person {name: "Charlie"})<-[:FOLLOWS]-(d:Person {name: "David"})') as result;

-- Test 1.3: Undirected relationship (should work as forward)
.print "Test 1.3 - Undirected relationship:";
SELECT cypher('CREATE (e:Person {name: "Eve"})-[:FRIENDS]-(f:Person {name: "Frank"})') as result;

-- Test 1.4: Multiple relationship types
.print "Test 1.4 - Multiple relationship types:";
SELECT cypher('CREATE (g:Person {name: "Grace"})-[:WORKS_FOR]->(h:Company {name: "TechCorp"})') as result;
SELECT cypher('CREATE (grace:Person {name: "Grace"})-[:LIVES_IN]->(i:City {name: "NYC"})') as result;

-- Test 1.5: Simple relationship creation  
.print "Test 1.5 - Simple relationship:";
SELECT cypher('CREATE (j:Person {name: "Jack"})-[:MANAGES]->(k:Person {name: "Kate"})') as result;

-- Test 1.6: Relationship with variables
.print "Test 1.6 - Relationship with variables:";
SELECT cypher('CREATE (m:Person {name: "Mike"})-[rel:MENTORS]->(n:Person {name: "Nina"})') as result;

-- SECTION 2: Relationship Matching Patterns
.print "=== SECTION 2: Relationship Matching Patterns ===";

-- Test 2.1: Match by relationship type
.print "Test 2.1 - Match by relationship type:";
SELECT cypher('MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name') as result;

-- Test 2.2: Match any relationship type
.print "Test 2.2 - Match any relationship type:";
SELECT cypher('MATCH (a)-[r]->(b) RETURN a.name, b.name ORDER BY a.name') as result;

-- Test 2.3: Match undirected (both directions)
.print "Test 2.3 - Match undirected (both directions):";
SELECT cypher('MATCH (a)-[:KNOWS]-(b) RETURN a.name, b.name') as result;

-- Test 2.4: Match reverse direction
.print "Test 2.4 - Match reverse direction:";
SELECT cypher('MATCH (a)<-[:FOLLOWS]-(b) RETURN a.name, b.name') as result;

-- Test 2.5: Match with relationship variable
.print "Test 2.5 - Match with relationship variable:";
SELECT cypher('MATCH (a)-[r:MENTORS]->(b) RETURN a.name, r, b.name') as result;

-- Test 2.6: Match relationship without type constraint
.print "Test 2.6 - Match relationship without type constraint:";
SELECT cypher('MATCH ()-[r]-() RETURN r ORDER BY r LIMIT 3') as result;

-- SECTION 3: Complex Relationship Patterns
.print "=== SECTION 3: Complex Relationship Patterns ===";

-- Test 3.1: Match relationship chains
.print "Test 3.1 - Match relationship chains:";
SELECT cypher('MATCH (a)-[:MANAGES]->(b)-[:REPORTS_TO]->(c) RETURN a.name, b.name, c.name') as result;

-- Test 3.2: Match multiple relationships from same node
.print "Test 3.2 - Match multiple relationships from same node:";
SELECT cypher('MATCH (a:Person)-[:WORKS_FOR]->(company) RETURN a.name, company.name') as result;

-- Test 3.3: Match nodes with specific relationship counts
.print "Test 3.3 - Nodes with outgoing relationships:";
SELECT cypher('MATCH (a)-[]->(b) RETURN a.name ORDER BY a.name') as result;

-- Test 3.4: Match nodes by label with relationships
.print "Test 3.4 - Match specific labels with relationships:";
SELECT cypher('MATCH (p:Person)-[r]->(target) RETURN p.name, target ORDER BY p.name') as result;

-- SECTION 4: Relationship Direction Testing
.print "=== SECTION 4: Relationship Direction Testing ===";

-- Test 4.1: Verify forward direction works
.print "Test 4.1 - Forward direction verification:";
SELECT cypher('MATCH (alice:Person {name: "Alice"})-[:KNOWS]->(bob) RETURN bob.name') as result;

-- Test 4.2: Verify reverse direction works  
.print "Test 4.2 - Reverse direction verification:";
SELECT cypher('MATCH (charlie)<-[:FOLLOWS]-(david) RETURN charlie.name, david.name') as result;

-- Test 4.3: Verify undirected matching works both ways
.print "Test 4.3 - Undirected matching both ways:";
SELECT cypher('MATCH (a)-[:FRIENDS]-(b) RETURN a.name, b.name ORDER BY a.name') as result;

-- SECTION 5: Edge Cases and Error Conditions
.print "=== SECTION 5: Edge Cases ===";

-- Test 5.1: Non-existent relationship type
.print "Test 5.1 - Non-existent relationship type:";
SELECT cypher('MATCH (a)-[:NONEXISTENT]->(b) RETURN a, b') as result;

-- Test 5.2: Relationships between non-existent nodes
.print "Test 5.2 - Match with non-matching labels:";
SELECT cypher('MATCH (a:Robot)-[:KNOWS]->(b:Alien) RETURN a, b') as result;

-- SECTION 6: Database Verification and Statistics
.print "=== SECTION 6: Database Verification ===";

-- Test 6.1: Total edge count
.print "Test 6.1 - Total edges created:";
SELECT COUNT(*) as total_edges FROM edges;

-- Test 6.2: Edge types and counts
.print "Test 6.2 - Edge types and their counts:";
.mode column
.headers on
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY count DESC, type;

-- Test 6.3: Edge directions verification
.print "Test 6.3 - Sample of actual edge data:";
SELECT e.id, e.type, 
       n1.id as source_node_id, n2.id as target_node_id
FROM edges e 
JOIN nodes n1 ON e.source_id = n1.id 
JOIN nodes n2 ON e.target_id = n2.id 
ORDER BY e.id LIMIT 5;

-- Test 6.4: Node degree analysis
.print "Test 6.4 - Node degree analysis:";
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