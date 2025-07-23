-- ============================================================================
-- Variable-Length Pattern Tests with Relationship Data
-- Tests execution of variable-length patterns with actual traversable data
-- ============================================================================

-- Load the GraphQLite extension
.load ../build/graphqlite.so

-- Create a chain of relationships for traversal testing
-- Alice -> Bob -> Charlie -> Dave
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS]->(b:Person {name: "Bob"})');
SELECT cypher('CREATE (b:Person {name: "Bob"})-[:KNOWS]->(c:Person {name: "Charlie"})');
SELECT cypher('CREATE (c:Person {name: "Charlie"})-[:KNOWS]->(d:Person {name: "Dave"})');

-- Test 1: Single hop variable-length pattern
SELECT cypher('MATCH (a:Person)-[*1]->(b) RETURN b');

-- Test 2: Two hop variable-length pattern  
SELECT cypher('MATCH (a:Person)-[*2]->(b) RETURN b');

-- Test 3: One to two hops pattern
SELECT cypher('MATCH (a:Person)-[*1..2]->(b) RETURN b');

-- Test 4: Unlimited hops pattern
SELECT cypher('MATCH (a:Person)-[*]->(b) RETURN b');