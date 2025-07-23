-- ============================================================================
-- Variable-Length Pattern Tests
-- Tests grammar parsing and AST construction for variable-length paths
-- ============================================================================

-- Load the GraphQLite extension
.load ../build/graphqlite.so

-- Create test data with connected relationships for traversal
-- Alice -> Bob -> Charlie -> Dave
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS]->(b:Person {name: "Bob"})-[:KNOWS]->(c:Person {name: "Charlie"})-[:KNOWS]->(d:Person {name: "Dave"})');

-- Test 1: Basic unlimited hops pattern
-- Expected: Parse successfully and find traversal paths
SELECT cypher('MATCH (a:Person)-[*]->(b) RETURN b');

-- Test 2: Bounded hops pattern  
-- Expected: Parse successfully with min=1, max=3
SELECT cypher('MATCH (a:Person)-[*1..3]->(b) RETURN b');

-- Test 3: Exact hop count
-- Expected: Parse successfully with min=2, max=2
SELECT cypher('MATCH (a:Person)-[*2]->(b) RETURN b');

-- Test 4: Zero minimum hops
-- Expected: Parse successfully with min=0, max=2
SELECT cypher('MATCH (a:Person)-[*0..2]->(b) RETURN b');

-- Test 5: Variable binding with unlimited hops
-- Expected: Parse successfully with variable "path"
SELECT cypher('MATCH (a)-[path*]->(b) RETURN b');

-- Test 6: Variable binding with bounded hops
-- Expected: Parse successfully with variable "r", min=2, max=4
SELECT cypher('MATCH (a)-[r*2..4]->(b) RETURN b');

-- Test 7: Variable binding with exact hops
-- Expected: Parse successfully with variable "edges", min=3, max=3
SELECT cypher('MATCH (a)-[edges*3]->(b) RETURN b');

-- Test 8: Relationship type constraint unlimited
-- Expected: Parse successfully with type "KNOWS", unlimited hops
SELECT cypher('MATCH (a)-[:KNOWS*]->(b) RETURN b');

-- Test 9: Relationship type constraint bounded
-- Expected: Parse successfully with type "FOLLOWS", min=1, max=3
SELECT cypher('MATCH (a)-[:FOLLOWS*1..3]->(b) RETURN b');

-- Test 10: Relationship type constraint exact
-- Expected: Parse successfully with type "WORKS_WITH", min=2, max=2
SELECT cypher('MATCH (a)-[:WORKS_WITH*2]->(b) RETURN b');

-- Test 11: Variable and type combined unlimited
-- Expected: Parse successfully with variable "rel", type "CONNECTED", unlimited hops
SELECT cypher('MATCH (a)-[rel:CONNECTED*]->(b) RETURN b');

-- Test 12: Variable and type combined bounded
-- Expected: Parse successfully with variable "path", type "SIMILAR_TO", min=1, max=4
SELECT cypher('MATCH (a)-[path:SIMILAR_TO*1..4]->(b) RETURN b');

-- Test 13: Variable and type combined exact
-- Expected: Parse successfully with variable "chain", type "NEXT", min=3, max=3
SELECT cypher('MATCH (a)-[chain:NEXT*3]->(b) RETURN b');

-- Test 14: Left direction pattern
-- Expected: Parse successfully with left direction
SELECT cypher('MATCH (a)<-[*1..2]-(b) RETURN b');

-- Test 15: Large hop counts
-- Expected: Parse successfully with min=1, max=100
SELECT cypher('MATCH (a)-[*1..100]->(b) RETURN b');

-- Test 16: Single hop range (equivalent to exact)
-- Expected: Parse successfully with min=1, max=1
SELECT cypher('MATCH (a)-[*1..1]->(b) RETURN b');

-- Test 17: Complex relationship type name
-- Expected: Parse successfully with long type name
SELECT cypher('MATCH (a)-[:VERY_LONG_RELATIONSHIP_TYPE_NAME*2..5]->(b) RETURN b');

-- Test 18: Variable-length with WHERE clause
-- Expected: Parse successfully with both pattern and WHERE clause
SELECT cypher('MATCH (a)-[r*1..3]->(b) WHERE a.name = "Alice" RETURN b');

-- Test 19: Variable-length in compound statement 
-- Expected: Parse successfully as compound statement
-- Note: Multiple return variables not yet supported in grammar
-- SELECT cypher('MATCH (a)-[*2..4]->(b) RETURN a, b');

-- Test 20: Multiple variable-length patterns (future feature)
-- Note: This may not be supported initially, but should not crash parser
-- MATCH (a)-[*1..2]->(b)-[*1..3]->(c) RETURN c;

-- ============================================================================
-- Expected Error Cases (commented out for now, uncomment when error handling ready)
-- ============================================================================

-- Missing minimum bound: MATCH (a)-[*..3]->(b) RETURN b;
-- Missing maximum bound: MATCH (a)-[*1..]->(b) RETURN b;  
-- Negative minimum: MATCH (a)-[*-1..3]->(b) RETURN b;
-- Max less than min: MATCH (a)-[*3..1]->(b) RETURN b;
-- Invalid range syntax: MATCH (a)-[*1...3]->(b) RETURN b;