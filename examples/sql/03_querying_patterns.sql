-- Querying Patterns
-- Common MATCH patterns for traversing graphs
--
-- Run with: sqlite3 < examples/03_querying_patterns.sql

.load build/graphqlite.dylib
.mode column
.headers on

-- Build a social network
SELECT cypher('RETURN 1') as init;

INSERT INTO nodes (id) VALUES (1), (2), (3), (4), (5);
INSERT INTO node_labels (node_id, label) VALUES
    (1, 'Person'), (2, 'Person'), (3, 'Person'), (4, 'Person'), (5, 'Person');
INSERT INTO property_keys (id, key) VALUES (1, 'name');
INSERT INTO node_props_text (node_id, key_id, value) VALUES
    (1, 1, 'Alice'), (2, 1, 'Bob'), (3, 1, 'Charlie'),
    (4, 1, 'Diana'), (5, 1, 'Eve');

INSERT INTO edges (source_id, target_id, type) VALUES
    (1, 2, 'FOLLOWS'), (1, 3, 'FOLLOWS'),
    (2, 3, 'FOLLOWS'), (2, 4, 'FOLLOWS'),
    (3, 5, 'FOLLOWS'), (4, 5, 'FOLLOWS');

-- Direct connections
SELECT '=== Who does Alice follow? ===';
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:FOLLOWS]->(b)
    RETURN b.name
');

-- Reverse direction
SELECT '';
SELECT '=== Who follows Charlie? ===';
SELECT cypher('
    MATCH (a)-[:FOLLOWS]->(b:Person {name: "Charlie"})
    RETURN a.name
');

-- Two hops
SELECT '';
SELECT '=== Friends of friends (2 hops from Alice) ===';
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:FOLLOWS]->()-[:FOLLOWS]->(c)
    RETURN DISTINCT c.name
');

-- Variable length paths
SELECT '';
SELECT '=== Everyone reachable from Alice (1-3 hops) ===';
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:FOLLOWS*1..3]->(b)
    RETURN DISTINCT b.name
');

-- Count connections
SELECT '';
SELECT '=== Follower counts ===';
SELECT cypher('
    MATCH (a:Person)-[:FOLLOWS]->(b:Person)
    RETURN b.name, count(a) as followers
');
