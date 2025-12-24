-- SQL Integration
-- Using graph algorithm results in SQL queries
--
-- Run with: sqlite3 < examples/06_sql_integration.sql

.load build/graphqlite.dylib
.mode column
.headers on

-- Build a social network with activity data
SELECT cypher('RETURN 1') as init;

INSERT INTO nodes (id) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
INSERT INTO node_labels (node_id, label) VALUES
    (1, 'User'), (2, 'User'), (3, 'User'), (4, 'User'),
    (5, 'User'), (6, 'User'), (7, 'User'), (8, 'User');

INSERT INTO property_keys (id, key) VALUES (1, 'name'), (2, 'posts');
INSERT INTO node_props_text (node_id, key_id, value) VALUES
    (1, 1, 'Alice'), (2, 1, 'Bob'), (3, 1, 'Charlie'), (4, 1, 'Diana'),
    (5, 1, 'Eve'), (6, 1, 'Frank'), (7, 1, 'Grace'), (8, 1, 'Henry');
INSERT INTO node_props_int (node_id, key_id, value) VALUES
    (1, 2, 42), (2, 2, 15), (3, 2, 8), (4, 2, 23),
    (5, 2, 67), (6, 2, 5), (7, 2, 31), (8, 2, 12);

-- Two friend clusters
INSERT INTO edges (source_id, target_id, type) VALUES
    -- Cluster 1: Alice, Bob, Charlie, Diana
    (1, 2, 'FOLLOWS'), (2, 1, 'FOLLOWS'),
    (1, 3, 'FOLLOWS'), (3, 1, 'FOLLOWS'),
    (2, 3, 'FOLLOWS'), (3, 2, 'FOLLOWS'),
    (1, 4, 'FOLLOWS'), (4, 1, 'FOLLOWS'),
    -- Cluster 2: Eve, Frank, Grace, Henry
    (5, 6, 'FOLLOWS'), (6, 5, 'FOLLOWS'),
    (5, 7, 'FOLLOWS'), (7, 5, 'FOLLOWS'),
    (6, 7, 'FOLLOWS'), (7, 6, 'FOLLOWS'),
    (5, 8, 'FOLLOWS'), (8, 5, 'FOLLOWS'),
    -- Bridge: Diana follows Eve
    (4, 5, 'FOLLOWS');

-- Find influential users
SELECT '=== Top influencers by PageRank ===';
WITH rankings AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.score') as score
    FROM json_each(cypher('RETURN pageRank(0.85, 20)'))
)
SELECT
    n.value as name,
    printf('%.4f', r.score) as influence,
    p.value as posts
FROM rankings r
JOIN node_props_text n ON n.node_id = r.node_id AND n.key_id = 1
JOIN node_props_int p ON p.node_id = r.node_id AND p.key_id = 2
ORDER BY r.score DESC
LIMIT 5;

-- Detect communities
SELECT '';
SELECT '=== Communities ===';
WITH communities AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.community') as community
    FROM json_each(cypher('RETURN labelPropagation(10)'))
)
SELECT
    c.community as group_id,
    group_concat(n.value, ', ') as members,
    count(*) as size
FROM communities c
JOIN node_props_text n ON n.node_id = c.node_id AND n.key_id = 1
GROUP BY c.community
ORDER BY size DESC;

-- Combine: top influencer per community
SELECT '';
SELECT '=== Top influencer per community ===';
WITH
rankings AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.score') as score
    FROM json_each(cypher('RETURN pageRank(0.85, 20)'))
),
communities AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.community') as community
    FROM json_each(cypher('RETURN labelPropagation(10)'))
),
ranked_by_community AS (
    SELECT
        c.community,
        r.node_id,
        r.score,
        ROW_NUMBER() OVER (PARTITION BY c.community ORDER BY r.score DESC) as rank
    FROM communities c
    JOIN rankings r ON r.node_id = c.node_id
)
SELECT
    rc.community as group_id,
    n.value as top_influencer,
    printf('%.4f', rc.score) as score
FROM ranked_by_community rc
JOIN node_props_text n ON n.node_id = rc.node_id AND n.key_id = 1
WHERE rc.rank = 1;
