-- Aggregations
-- Using count, sum, min, max, avg, and collect
--
-- Run with: sqlite3 < examples/04_aggregations.sql

.load build/graphqlite.dylib
.mode column
.headers on

-- Build a product review graph
SELECT cypher('RETURN 1') as init;

INSERT INTO nodes (id) VALUES (1), (2), (3), (4), (5), (6), (7);
INSERT INTO node_labels (node_id, label) VALUES
    (1, 'User'), (2, 'User'), (3, 'User'),
    (4, 'Product'), (5, 'Product'), (6, 'Product'), (7, 'Product');

INSERT INTO property_keys (id, key) VALUES (1, 'name'), (2, 'rating'), (3, 'price');
INSERT INTO node_props_text (node_id, key_id, value) VALUES
    (1, 1, 'Alice'), (2, 1, 'Bob'), (3, 1, 'Charlie'),
    (4, 1, 'Laptop'), (5, 1, 'Phone'), (6, 1, 'Tablet'), (7, 1, 'Monitor');
INSERT INTO node_props_int (node_id, key_id, value) VALUES
    (4, 3, 999), (5, 3, 699), (6, 3, 449), (7, 3, 299);

-- Reviews with ratings
INSERT INTO edges (id, source_id, target_id, type) VALUES
    (1, 1, 4, 'REVIEWED'), (2, 1, 5, 'REVIEWED'), (3, 1, 6, 'REVIEWED'),
    (4, 2, 4, 'REVIEWED'), (5, 2, 5, 'REVIEWED'),
    (6, 3, 4, 'REVIEWED'), (7, 3, 7, 'REVIEWED');

INSERT INTO edge_props_int (edge_id, key_id, value) VALUES
    (1, 2, 5), (2, 2, 4), (3, 2, 3),
    (4, 2, 4), (5, 2, 5),
    (6, 2, 5), (7, 2, 4);

-- Count reviews per product
SELECT '=== Review counts ===';
SELECT cypher('
    MATCH (u:User)-[:REVIEWED]->(p:Product)
    RETURN p.name, count(u) as reviews
');

-- Basic stats
SELECT '';
SELECT '=== Product stats ===';
SELECT cypher('
    MATCH (p:Product)
    RETURN count(p) as total, min(p.price) as cheapest, max(p.price) as priciest
');

-- Collect names into a list
SELECT '';
SELECT '=== All product names ===';
SELECT cypher('
    MATCH (p:Product)
    RETURN collect(p.name) as products
');

-- Group by and aggregate
SELECT '';
SELECT '=== Users and their reviewed products ===';
SELECT cypher('
    MATCH (u:User)-[:REVIEWED]->(p:Product)
    RETURN u.name, collect(p.name) as reviewed
');
