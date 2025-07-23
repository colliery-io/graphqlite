-- Test 01: Basic CREATE operations
.load ../build/graphqlite.so

-- Test simple node creation
SELECT cypher('CREATE (n:Person)');
SELECT cypher('CREATE (n:Person {name: "Alice"})');
SELECT cypher('CREATE (n:Product {name: "Widget", price: 100})');

-- Test multiple properties with different types
SELECT cypher('CREATE (n:Product {name: "Gadget", price: 250, rating: 4.5, inStock: true})');

-- Verify nodes were created
SELECT 'Node count:', COUNT(*) FROM nodes;
SELECT 'Label count:', COUNT(*) FROM node_labels;
SELECT 'Property count (text):', COUNT(*) FROM node_props_text;
SELECT 'Property count (int):', COUNT(*) FROM node_props_int;
SELECT 'Property count (real):', COUNT(*) FROM node_props_real;
SELECT 'Property count (bool):', COUNT(*) FROM node_props_bool;