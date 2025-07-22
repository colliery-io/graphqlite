-- Test 04: Error handling verification
-- Note: Parse errors cause SQLite runtime errors, so we test them separately
.load ./build/graphqlite.so

-- First, let's verify valid queries work
SELECT '=== Valid queries should succeed ===';
SELECT cypher('CREATE (n:ValidNode)');
SELECT cypher('CREATE (n:ValidNode {prop: "value"})');
SELECT cypher('CREATE (n:Product {name: "Test", price: 100, active: true})');

-- Verify the nodes were created
SELECT '=== Verifying node creation ===';
SELECT 'Node count:', COUNT(*) FROM nodes;
SELECT 'ValidNode count:', COUNT(*) 
FROM nodes n 
JOIN node_labels nl ON n.id = nl.node_id 
WHERE nl.label = 'ValidNode';

-- Test property type verification
SELECT '=== Property type verification ===';
SELECT 'Text properties:', COUNT(*) FROM node_props_text;
SELECT 'Integer properties:', COUNT(*) FROM node_props_int;
SELECT 'Boolean properties:', COUNT(*) FROM node_props_bool;