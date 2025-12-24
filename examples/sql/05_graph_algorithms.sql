-- Graph Algorithms
-- PageRank and community detection
--
-- Run with: sqlite3 < examples/05_graph_algorithms.sql

.load build/graphqlite.dylib
.mode column
.headers on

-- Build a citation network
SELECT cypher('RETURN 1') as init;

INSERT INTO nodes (id) VALUES (1), (2), (3), (4), (5), (6);
INSERT INTO node_labels (node_id, label) VALUES
    (1, 'Paper'), (2, 'Paper'), (3, 'Paper'),
    (4, 'Paper'), (5, 'Paper'), (6, 'Paper');

INSERT INTO property_keys (id, key) VALUES (1, 'title');
INSERT INTO node_props_text (node_id, key_id, value) VALUES
    (1, 1, 'Foundations'),
    (2, 1, 'Methods'),
    (3, 1, 'Applications'),
    (4, 1, 'Survey'),
    (5, 1, 'Analysis'),
    (6, 1, 'Review');

-- Citation links (citing -> cited)
INSERT INTO edges (source_id, target_id, type) VALUES
    (2, 1, 'CITES'),  -- Methods cites Foundations
    (3, 1, 'CITES'),  -- Applications cites Foundations
    (3, 2, 'CITES'),  -- Applications cites Methods
    (4, 1, 'CITES'),  -- Survey cites Foundations
    (4, 2, 'CITES'),  -- Survey cites Methods
    (4, 3, 'CITES'),  -- Survey cites Applications
    (5, 2, 'CITES'),  -- Analysis cites Methods
    (6, 4, 'CITES'); -- Review cites Survey

-- PageRank: find influential papers
SELECT '=== PageRank (paper influence) ===';
SELECT cypher('RETURN pageRank(0.85, 20)');

-- Unpack to table format
SELECT '';
SELECT '=== PageRank as table ===';
SELECT
    json_extract(value, '$.node_id') as id,
    printf('%.4f', json_extract(value, '$.score')) as score
FROM json_each(cypher('RETURN pageRank(0.85, 20)'));

-- Join with paper titles
SELECT '';
SELECT '=== Papers ranked by influence ===';
WITH rankings AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.score') as score
    FROM json_each(cypher('RETURN pageRank(0.85, 20)'))
)
SELECT
    p.value as paper,
    printf('%.4f', r.score) as influence
FROM rankings r
JOIN node_props_text p ON p.node_id = r.node_id
ORDER BY r.score DESC;

-- Community detection
SELECT '';
SELECT '=== Community Detection ===';
SELECT cypher('RETURN labelPropagation(10)');

-- Communities as table
SELECT '';
SELECT '=== Papers by community ===';
WITH communities AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.community') as community
    FROM json_each(cypher('RETURN labelPropagation(10)'))
)
SELECT
    c.community,
    p.value as paper
FROM communities c
JOIN node_props_text p ON p.node_id = c.node_id
ORDER BY c.community, p.value;
