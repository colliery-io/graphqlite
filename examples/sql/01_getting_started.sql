-- Getting Started with GraphQLite
-- Basic graph operations using Cypher queries
--
-- Run with: sqlite3 < examples/01_getting_started.sql

.load build/graphqlite.dylib

-- Create some nodes
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35})');

-- Create relationships
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b)
');
SELECT cypher('
    MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"})
    CREATE (b)-[:KNOWS]->(c)
');

-- Query all people
SELECT 'All people:';
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age');

-- Query relationships
SELECT '';
SELECT 'Who knows who:';
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a.name, b.name');

-- Find friends of friends
SELECT '';
SELECT 'Friends of friends:';
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
    RETURN fof.name
');
