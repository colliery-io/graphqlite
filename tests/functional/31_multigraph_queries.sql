-- ========================================================================
-- Test 31: Multi-Graph Queries with FROM Clause
-- ========================================================================
-- PURPOSE: Verifies that MATCH ... FROM graph_name queries work correctly
--          for querying attached databases as separate graphs
-- COVERS:  FROM clause, cross-database queries, table prefixing,
--          property access, functions, aggregations, relationships
-- ========================================================================

.load ./build/graphqlite

-- ========================================================================
-- SETUP: Create main graph and attached graph
-- ========================================================================
SELECT '=== SETUP: Creating Multi-Graph Environment ===' as test_section;

-- Create a separate database file for the "other" graph
-- First, connect to temp file and initialize it
.shell rm -f test_other_graph.db

-- Create the other graph database with schema
SELECT 'Creating other_graph database...' as status;

-- We need to attach, then manually create schema in it
ATTACH DATABASE 'test_other_graph.db' AS other_graph;

-- Initialize schema in the attached database
CREATE TABLE IF NOT EXISTS other_graph.nodes (id INTEGER PRIMARY KEY);
CREATE TABLE IF NOT EXISTS other_graph.edges (
    id INTEGER PRIMARY KEY,
    source_id INTEGER NOT NULL,
    target_id INTEGER NOT NULL,
    type TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS other_graph.node_labels (
    node_id INTEGER NOT NULL,
    label TEXT NOT NULL,
    PRIMARY KEY (node_id, label)
);
CREATE TABLE IF NOT EXISTS other_graph.property_keys (
    id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS other_graph.node_props_text (
    node_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value TEXT,
    PRIMARY KEY (node_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.node_props_int (
    node_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value INTEGER,
    PRIMARY KEY (node_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.node_props_real (
    node_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value REAL,
    PRIMARY KEY (node_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.node_props_bool (
    node_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value INTEGER,
    PRIMARY KEY (node_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.edge_props_text (
    edge_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value TEXT,
    PRIMARY KEY (edge_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.edge_props_int (
    edge_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value INTEGER,
    PRIMARY KEY (edge_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.edge_props_real (
    edge_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value REAL,
    PRIMARY KEY (edge_id, key_id)
);
CREATE TABLE IF NOT EXISTS other_graph.edge_props_bool (
    edge_id INTEGER NOT NULL,
    key_id INTEGER NOT NULL,
    value INTEGER,
    PRIMARY KEY (edge_id, key_id)
);

SELECT 'Schema created in other_graph' as status;

-- ========================================================================
-- SETUP: Populate main graph with test data
-- ========================================================================
SELECT '=== Populating Main Graph ===' as test_section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, city: "NYC"})');
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25, city: "NYC"})');
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35, city: "LA"})');
SELECT cypher('CREATE (d:Company {name: "TechCorp", industry: "Technology"})');
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS {since: 2020}]->(b)');
SELECT cypher('MATCH (a:Person {name: "Alice"}), (c:Company {name: "TechCorp"}) CREATE (a)-[:WORKS_AT {role: "Engineer"}]->(c)');

SELECT 'Main graph populated' as status;

-- ========================================================================
-- SETUP: Populate other_graph with different test data
-- ========================================================================
SELECT '=== Populating Other Graph ===' as test_section;

-- Insert nodes
INSERT INTO other_graph.nodes (id) VALUES (1), (2), (3), (4);

-- Insert labels
INSERT INTO other_graph.node_labels (node_id, label) VALUES
    (1, 'Person'), (2, 'Person'), (3, 'Person'), (4, 'Company');

-- Insert property keys
INSERT INTO other_graph.property_keys (id, key) VALUES
    (1, 'name'), (2, 'age'), (3, 'city'), (4, 'industry'), (5, 'since'), (6, 'role');

-- Insert node properties
INSERT INTO other_graph.node_props_text (node_id, key_id, value) VALUES
    (1, 1, 'David'),    -- name
    (1, 3, 'Chicago'),  -- city
    (2, 1, 'Eve'),      -- name
    (2, 3, 'Chicago'),  -- city
    (3, 1, 'Frank'),    -- name
    (3, 3, 'Boston'),   -- city
    (4, 1, 'StartupInc'), -- name
    (4, 4, 'Finance');    -- industry

INSERT INTO other_graph.node_props_int (node_id, key_id, value) VALUES
    (1, 2, 28),  -- age
    (2, 2, 32),  -- age
    (3, 2, 45);  -- age

-- Insert edges
INSERT INTO other_graph.edges (id, source_id, target_id, type) VALUES
    (1, 1, 2, 'KNOWS'),
    (2, 2, 3, 'KNOWS'),
    (3, 1, 4, 'WORKS_AT');

-- Insert edge properties
INSERT INTO other_graph.edge_props_int (edge_id, key_id, value) VALUES
    (1, 5, 2019),  -- since
    (2, 5, 2021);  -- since

INSERT INTO other_graph.edge_props_text (edge_id, key_id, value) VALUES
    (3, 6, 'Manager');  -- role

SELECT 'Other graph populated' as status;

-- ========================================================================
-- TEST 1: Basic MATCH FROM - Simple node query
-- ========================================================================
SELECT '=== TEST 1: Basic MATCH FROM ===' as test_section;

SELECT 'Test 1.1 - Query main graph (no FROM):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name');

SELECT 'Test 1.2 - Query other_graph with FROM:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name ORDER BY n.name');

-- ========================================================================
-- TEST 2: MATCH FROM with WHERE clause
-- ========================================================================
SELECT '=== TEST 2: MATCH FROM with WHERE ===' as test_section;

SELECT 'Test 2.1 - WHERE with string comparison:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.name = "David" RETURN n.name, n.city');

SELECT 'Test 2.2 - WHERE with numeric comparison:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.age > 30 RETURN n.name, n.age ORDER BY n.age');

SELECT 'Test 2.3 - WHERE with city filter:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.city = "Chicago" RETURN n.name ORDER BY n.name');

-- ========================================================================
-- TEST 3: MATCH FROM with multiple properties
-- ========================================================================
SELECT '=== TEST 3: Multiple Property Access ===' as test_section;

SELECT 'Test 3.1 - Return multiple properties:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name, n.age, n.city ORDER BY n.name');

-- ========================================================================
-- TEST 4: MATCH FROM with labels() function
-- ========================================================================
SELECT '=== TEST 4: labels() Function ===' as test_section;

SELECT 'Test 4.1 - labels() on Person nodes:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name, labels(n) ORDER BY n.name');

SELECT 'Test 4.2 - labels() on Company nodes:' as test_name;
SELECT cypher('MATCH (n:Company) FROM other_graph RETURN n.name, labels(n)');

-- ========================================================================
-- TEST 5: MATCH FROM with properties() function
-- ========================================================================
SELECT '=== TEST 5: properties() Function ===' as test_section;

SELECT 'Test 5.1 - properties() returns all properties:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.name = "David" RETURN properties(n)');

-- ========================================================================
-- TEST 6: MATCH FROM with keys() function
-- ========================================================================
SELECT '=== TEST 6: keys() Function ===' as test_section;

SELECT 'Test 6.1 - keys() returns property keys:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.name = "David" RETURN keys(n)');

-- ========================================================================
-- TEST 7: MATCH FROM with relationship patterns
-- ========================================================================
SELECT '=== TEST 7: Relationship Patterns ===' as test_section;

SELECT 'Test 7.1 - Simple relationship pattern:' as test_name;
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) FROM other_graph RETURN a.name, b.name');

SELECT 'Test 7.2 - Relationship with properties:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) FROM other_graph RETURN a.name, b.name, r.since');

SELECT 'Test 7.3 - Outgoing relationship to Company:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:WORKS_AT]->(c:Company) FROM other_graph RETURN a.name, c.name, r.role');

-- ========================================================================
-- TEST 8: MATCH FROM with type() function
-- ========================================================================
SELECT '=== TEST 8: type() Function ===' as test_section;

SELECT 'Test 8.1 - type() returns relationship type:' as test_name;
SELECT cypher('MATCH (a:Person)-[r]->(b) FROM other_graph RETURN a.name, type(r), b.name ORDER BY a.name, type(r)');

-- ========================================================================
-- TEST 9: MATCH FROM with aggregations
-- ========================================================================
SELECT '=== TEST 9: Aggregation Functions ===' as test_section;

SELECT 'Test 9.1 - count() nodes:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN count(n) as person_count');

SELECT 'Test 9.2 - avg() of ages:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN avg(n.age) as avg_age');

SELECT 'Test 9.3 - min/max ages:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN min(n.age) as youngest, max(n.age) as oldest');

SELECT 'Test 9.4 - collect() names:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN collect(n.name) as all_names');

-- ========================================================================
-- TEST 10: MATCH FROM with ORDER BY, LIMIT, SKIP
-- ========================================================================
SELECT '=== TEST 10: ORDER BY, LIMIT, SKIP ===' as test_section;

SELECT 'Test 10.1 - ORDER BY DESC:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name, n.age ORDER BY n.age DESC');

SELECT 'Test 10.2 - LIMIT:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name ORDER BY n.name LIMIT 2');

SELECT 'Test 10.3 - SKIP and LIMIT:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name ORDER BY n.name SKIP 1 LIMIT 1');

-- ========================================================================
-- TEST 11: MATCH FROM with DISTINCT
-- ========================================================================
SELECT '=== TEST 11: DISTINCT ===' as test_section;

SELECT 'Test 11.1 - DISTINCT cities:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN DISTINCT n.city ORDER BY n.city');

-- ========================================================================
-- TEST 12: MATCH FROM with aliases
-- ========================================================================
SELECT '=== TEST 12: Column Aliases ===' as test_section;

SELECT 'Test 12.1 - AS aliases:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.name AS person_name, n.age AS person_age ORDER BY person_name');

-- ========================================================================
-- TEST 13: Verify graph isolation - main graph unchanged
-- ========================================================================
SELECT '=== TEST 13: Graph Isolation Verification ===' as test_section;

SELECT 'Test 13.1 - Main graph still has original data:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name');

SELECT 'Test 13.2 - Main graph person count:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN count(n) as main_count');

SELECT 'Test 13.3 - Other graph person count:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN count(n) as other_count');

-- ========================================================================
-- TEST 14: Complex WHERE conditions
-- ========================================================================
SELECT '=== TEST 14: Complex WHERE Conditions ===' as test_section;

SELECT 'Test 14.1 - AND conditions:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.city = "Chicago" AND n.age > 30 RETURN n.name, n.age');

SELECT 'Test 14.2 - OR conditions:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.city = "Chicago" OR n.city = "Boston" RETURN n.name, n.city ORDER BY n.name');

SELECT 'Test 14.3 - NOT condition:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE NOT n.city = "Chicago" RETURN n.name, n.city');

-- ========================================================================
-- TEST 15: Relationship direction patterns
-- ========================================================================
SELECT '=== TEST 15: Relationship Directions ===' as test_section;

SELECT 'Test 15.1 - Outgoing relationships:' as test_name;
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) FROM other_graph RETURN a.name AS from_person, b.name AS to_person');

SELECT 'Test 15.2 - Incoming relationships:' as test_name;
SELECT cypher('MATCH (a:Person)<-[:KNOWS]-(b:Person) FROM other_graph RETURN a.name AS to_person, b.name AS from_person');

-- ========================================================================
-- TEST 16: Group by with aggregation
-- ========================================================================
SELECT '=== TEST 16: Group By Aggregation ===' as test_section;

SELECT 'Test 16.1 - Count by city:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN n.city, count(n) as count ORDER BY n.city');

-- ========================================================================
-- TEST 17: EXISTS predicate
-- ========================================================================
SELECT '=== TEST 17: EXISTS Predicate ===' as test_section;

SELECT 'Test 17.1 - EXISTS property check:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE EXISTS(n.age) RETURN n.name ORDER BY n.name');

-- ========================================================================
-- TEST 18: IS NULL / IS NOT NULL
-- ========================================================================
SELECT '=== TEST 18: NULL Checks ===' as test_section;

SELECT 'Test 18.1 - IS NOT NULL:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph WHERE n.age IS NOT NULL RETURN n.name ORDER BY n.name');

-- ========================================================================
-- TEST 19: graph() Function for Provenance Tracking
-- ========================================================================
SELECT '=== TEST 19: graph() Function ===' as test_section;

SELECT 'Test 19.1 - graph() on main graph nodes:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN graph(n) AS source_graph, n.name ORDER BY n.name');

SELECT 'Test 19.2 - graph() on other_graph nodes:' as test_name;
SELECT cypher('MATCH (n:Person) FROM other_graph RETURN graph(n) AS source_graph, n.name ORDER BY n.name');

SELECT 'Test 19.3 - graph() on relationships:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) FROM other_graph RETURN graph(r) AS rel_graph, a.name, b.name');

SELECT 'Test 19.4 - graph() with Company nodes:' as test_name;
SELECT cypher('MATCH (c:Company) FROM other_graph RETURN graph(c) AS source, c.name');

-- ========================================================================
-- CLEANUP
-- ========================================================================
SELECT '=== CLEANUP ===' as test_section;

DETACH DATABASE other_graph;
.shell rm -f test_other_graph.db

SELECT '=== Multi-Graph Tests Complete ===' as test_section;
SELECT 'All multi-graph query patterns tested successfully' as status;
