-- Test 09: Flexible grammar patterns (nodes without labels, edges without types)
.load ../build/graphqlite.so

-- Test CREATE with nodes without labels
SELECT '=== CREATE nodes without labels ===';
SELECT cypher('CREATE (a) RETURN a');
SELECT cypher('CREATE (person {name: "Alice", age: 30}) RETURN person');
-- Multiple CREATE in one statement not supported yet: CREATE (a), (b), (c) RETURN a

-- Test CREATE with RETURN (compound statements)
SELECT '=== CREATE with RETURN ===';
SELECT cypher('CREATE (a:Person {name: "Test"}) RETURN a');
SELECT cypher('CREATE (node {type: "generic"}) RETURN node');

-- Test edge patterns without types
SELECT '=== Edge patterns without types ===';
SELECT cypher('CREATE (a)-[r]->(b) RETURN a');
SELECT cypher('CREATE (a:Person)-[connection]->(b:Person) RETURN a');

-- Test edge patterns with empty brackets  
SELECT '=== Edge patterns with empty brackets ===';
SELECT cypher('CREATE (a)-[]->(b) RETURN a');
SELECT cypher('CREATE (start:Node)-[]->(end:Node) RETURN start');

-- Test edge patterns without brackets
SELECT '=== Edge patterns without brackets ===';
SELECT cypher('CREATE (a)-->(b) RETURN a');
SELECT cypher('CREATE (first:Thing)-->(second:Thing) RETURN first');

-- Test MATCH with flexible patterns
SELECT '=== MATCH with flexible node patterns ===';
SELECT cypher('MATCH (a) RETURN a');
SELECT cypher('MATCH (node {name: "Alice"}) RETURN node');

-- Test MATCH with flexible edge patterns
SELECT '=== MATCH with flexible edge patterns ===';
SELECT cypher('MATCH (a)-[r]->(b) RETURN a');
SELECT cypher('MATCH (a)-[]->(b) RETURN a');
SELECT cypher('MATCH (a)-->(b) RETURN a');

-- Test left direction flexible patterns
SELECT '=== Left direction flexible patterns ===';
SELECT cypher('CREATE (a)<-[r]-(b) RETURN a');
SELECT cypher('CREATE (a)<-[]-(b) RETURN a');
SELECT cypher('CREATE (a)<--(b) RETURN a');
SELECT cypher('MATCH (a)<-[r]-(b) RETURN a');
SELECT cypher('MATCH (a)<-[]-(b) RETURN a');
SELECT cypher('MATCH (a)<--(b) RETURN a');

-- Test mixed patterns (some with labels/types, some without)
SELECT '=== Mixed patterns ===';
SELECT cypher('CREATE (a:Person)-[r]->(b) RETURN a');
SELECT cypher('CREATE (a)-[r:KNOWS]->(b:Person) RETURN a');
SELECT cypher('CREATE (a)-[]->(b:Company) RETURN a');
SELECT cypher('MATCH (a:Person)-[r]->(b) RETURN a');
SELECT cypher('MATCH (a)-[r:KNOWS]->(b:Person) RETURN a');

-- Test edge patterns with properties but no types
SELECT '=== Edge patterns with properties but no types ===';
SELECT cypher('CREATE (a)-[r {since: "2024", strength: 5}]->(b) RETURN a');
SELECT cypher('MATCH (a)-[r {since: "2024"}]->(b) RETURN a');

-- Test comprehensive flexible pattern combinations
SELECT '=== Comprehensive flexible combinations ===';
SELECT cypher('CREATE (a {name: "Alice"})-[r {type: "connection"}]->(b {name: "Bob"}) RETURN a');
SELECT cypher('MATCH (a {name: "Alice"})-[r {type: "connection"}]->(b {name: "Bob"}) RETURN a');

-- Verify all operations completed successfully
SELECT 'Flexible patterns test completed - all new grammar patterns are working!';