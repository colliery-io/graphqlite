-- Building Graphs with Raw SQL
-- For bulk loading, raw INSERT is faster than Cypher CREATE
--
-- Run with: sqlite3 < examples/02_building_graphs.sql

.load build/graphqlite.dylib
.mode column
.headers on

-- Initialize the schema
SELECT cypher('RETURN 1') as init;

-- Create nodes directly
INSERT INTO nodes (id) VALUES (1), (2), (3), (4);

-- Add labels
INSERT INTO node_labels (node_id, label) VALUES
    (1, 'Person'),
    (2, 'Person'),
    (3, 'Person'),
    (4, 'Company');

-- Register property keys
INSERT INTO property_keys (id, key) VALUES (1, 'name'), (2, 'age');

-- Add properties (text)
INSERT INTO node_props_text (node_id, key_id, value) VALUES
    (1, 1, 'Alice'),
    (2, 1, 'Bob'),
    (3, 1, 'Charlie'),
    (4, 1, 'Acme Corp');

-- Add properties (integer)
INSERT INTO node_props_int (node_id, key_id, value) VALUES
    (1, 2, 30),
    (2, 2, 25),
    (3, 2, 35);

-- Create relationships
INSERT INTO edges (source_id, target_id, type) VALUES
    (1, 2, 'KNOWS'),
    (2, 3, 'KNOWS'),
    (1, 4, 'WORKS_AT'),
    (2, 4, 'WORKS_AT');

-- Verify with Cypher queries
SELECT 'People:';
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age');

SELECT '';
SELECT 'Relationships:';
SELECT cypher('MATCH (a)-[r]->(b) RETURN a.name, type(r), b.name');

SELECT '';
SELECT 'Who works at Acme:';
SELECT cypher('
    MATCH (p:Person)-[:WORKS_AT]->(c:Company {name: "Acme Corp"})
    RETURN p.name
');
