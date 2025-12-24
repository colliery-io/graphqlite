#!/bin/bash
# Scaled Performance Tests for GraphQLite
# Tests different graph sizes and connectivity patterns

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Detect extension based on platform
case "$(uname -s)" in
    Darwin) EXTENSION="$PROJECT_DIR/build/graphqlite.dylib" ;;
    MINGW*|MSYS*) EXTENSION="$PROJECT_DIR/build/graphqlite.dll" ;;
    *) EXTENSION="$PROJECT_DIR/build/graphqlite.so" ;;
esac

if [ ! -f "$EXTENSION" ]; then
    echo "Error: Extension not found at $EXTENSION"
    echo "Run 'make extension' first"
    exit 1
fi

# Test iterations
ITERATIONS=${1:-10}

echo "========================================"
echo "GraphQLite Scaled Performance Tests"
echo "Iterations per test: $ITERATIONS"
echo "========================================"
echo ""

# Helper to run timed query (suppresses debug output)
run_timed_query() {
    local db="$1"
    local name="$2"
    local query="$3"
    local iters="$4"

    local result=$(sqlite3 "$db" 2>&1 <<EOF
.load $EXTENSION
.timer on
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $iters)
SELECT cypher('$query') FROM cnt;
EOF
)
    local timing=$(echo "$result" | grep "Run Time:" | tail -1 | sed 's/Run Time: //')
    printf "  %-50s %s\n" "$name" "$timing"
}

# Helper to build graph silently
build_graph() {
    local db="$1"
    shift
    sqlite3 "$db" "$@" >/dev/null 2>&1
}

# Get current time in milliseconds
get_time_ms() {
    python3 -c 'import time; print(int(time.time() * 1000))'
}

# Get file size in human readable format
get_file_size() {
    local file="$1"
    if [ -f "$file" ]; then
        local bytes=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
        if [ "$bytes" -gt 1073741824 ]; then
            echo "$(echo "scale=2; $bytes/1073741824" | bc)GB"
        elif [ "$bytes" -gt 1048576 ]; then
            echo "$(echo "scale=2; $bytes/1048576" | bc)MB"
        elif [ "$bytes" -gt 1024 ]; then
            echo "$(echo "scale=2; $bytes/1024" | bc)KB"
        else
            echo "${bytes}B"
        fi
    else
        echo "0B"
    fi
}

# ============================================
# Fast graph building using direct SQL
# ============================================

# Initialize schema (must be called once per database)
init_schema() {
    local db="$1"
    sqlite3 "$db" <<EOF
CREATE TABLE IF NOT EXISTS nodes (id INTEGER PRIMARY KEY AUTOINCREMENT);
CREATE TABLE IF NOT EXISTS node_labels (node_id INTEGER NOT NULL, label TEXT NOT NULL, PRIMARY KEY (node_id, label));
CREATE TABLE IF NOT EXISTS edges (id INTEGER PRIMARY KEY AUTOINCREMENT, source_id INTEGER NOT NULL, target_id INTEGER NOT NULL, type TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS property_keys (id INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT UNIQUE NOT NULL);
CREATE TABLE IF NOT EXISTS node_props_int (node_id INTEGER NOT NULL, key_id INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY (node_id, key_id));
CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id);
CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id);
CREATE INDEX IF NOT EXISTS idx_node_labels ON node_labels(label);
EOF
}

# Build nodes using direct SQL (much faster than Cypher)
# Also adds 'id' property so MATCH (n {id: x}) works
build_nodes_fast() {
    local db="$1"
    local start_id="$2"
    local count="$3"
    local label="$4"
    local end_id=$((start_id + count - 1))

    sqlite3 "$db" <<EOF
-- Insert nodes
WITH RECURSIVE cnt(x) AS (VALUES($start_id) UNION ALL SELECT x+1 FROM cnt WHERE x < $end_id)
INSERT INTO nodes (id) SELECT x FROM cnt;

-- Add labels
WITH RECURSIVE cnt(x) AS (VALUES($start_id) UNION ALL SELECT x+1 FROM cnt WHERE x < $end_id)
INSERT INTO node_labels (node_id, label) SELECT x, '$label' FROM cnt;

-- Add 'id' property key if not exists
INSERT OR IGNORE INTO property_keys (key) VALUES ('id');

-- Add id property to each node (for MATCH (n {id: x}) queries)
WITH RECURSIVE cnt(x) AS (VALUES($start_id) UNION ALL SELECT x+1 FROM cnt WHERE x < $end_id)
INSERT INTO node_props_int (node_id, key_id, value)
SELECT x, (SELECT id FROM property_keys WHERE key = 'id'), x FROM cnt;
EOF
}

# Build linear chain edges using direct SQL
build_chain_edges_fast() {
    local db="$1"
    local node_count="$2"
    local edge_type="$3"

    sqlite3 "$db" <<EOF
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $node_count)
INSERT INTO edges (source_id, target_id, type)
SELECT x, x+1, '$edge_type' FROM cnt WHERE x < $node_count;
EOF
}

# Build hub-to-spokes edges using direct SQL
build_hub_edges_fast() {
    local db="$1"
    local hub_id="$2"
    local spoke_start="$3"
    local spoke_count="$4"
    local edge_type="$5"
    local spoke_end=$((spoke_start + spoke_count - 1))

    sqlite3 "$db" <<EOF
WITH RECURSIVE cnt(x) AS (VALUES($spoke_start) UNION ALL SELECT x+1 FROM cnt WHERE x < $spoke_end)
INSERT INTO edges (source_id, target_id, type)
SELECT $hub_id, x, '$edge_type' FROM cnt;
EOF
}

# Build cyclic mesh edges using direct SQL
build_cyclic_edges_fast() {
    local db="$1"
    local node_count="$2"
    local edges_per_node="$3"
    local edge_type="$4"
    local start_id="${5:-0}"

    sqlite3 "$db" <<EOF
WITH RECURSIVE
  nodes_cte(x) AS (VALUES($start_id) UNION ALL SELECT x+1 FROM nodes_cte WHERE x < $((start_id + node_count - 1))),
  offsets(k) AS (VALUES(1) UNION ALL SELECT k+1 FROM offsets WHERE k < $edges_per_node)
INSERT INTO edges (source_id, target_id, type)
SELECT n.x, $start_id + ((n.x - $start_id + o.k) % $node_count), '$edge_type'
FROM nodes_cte n, offsets o;
EOF
}

# Build binary tree edges using direct SQL
build_tree_edges_fast() {
    local db="$1"
    local max_parent="$2"
    local edge_type="$3"

    sqlite3 "$db" <<EOF
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $max_parent)
INSERT INTO edges (source_id, target_id, type)
SELECT x, x*2, '$edge_type' FROM cnt
UNION ALL
SELECT x, x*2+1, '$edge_type' FROM cnt;
EOF
}

# ============================================
# Test 1: Linear Chain Graph
# ============================================
test_linear_chain() {
    local size=$1
    local db=$(mktemp /tmp/gql_linear_XXXXXX.db)

    echo "=== Linear Chain Graph (n=$size) ==="
    echo "Structure: 1->2->3->...->$size"
    echo ""

    local build_start=$(get_time_ms)

    # Initialize and build using fast SQL
    init_schema "$db"
    build_nodes_fast "$db" 1 $size "Node"

    local nodes_done=$(get_time_ms)
    local node_time=$((nodes_done - build_start))

    build_chain_edges_fast "$db" $size "NEXT"

    local build_end=$(get_time_ms)
    local edge_time=$((build_end - nodes_done))
    local total_time=$((build_end - build_start))
    local db_size=$(get_file_size "$db")

    echo ""
    printf "  Build: %d nodes in %dms, %d edges in %dms (total: %.2fs)\n" \
        $size $node_time $((size-1)) $edge_time $(echo "scale=2; $total_time/1000" | bc)
    printf "  DB Size: %s\n" "$db_size"

    run_timed_query "$db" "Count all nodes" "MATCH (n:Node) RETURN count(n)" $ITERATIONS
    run_timed_query "$db" "Find node by id (middle)" "MATCH (n:Node {id: $((size/2))}) RETURN n" $ITERATIONS
    run_timed_query "$db" "Single hop from start" "MATCH (a:Node {id: 1})-[:NEXT]->(b) RETURN b.id" $ITERATIONS
    run_timed_query "$db" "Two hops from start" "MATCH (a:Node {id: 1})-[:NEXT]->()-[:NEXT]->(c) RETURN c.id" $ITERATIONS
    run_timed_query "$db" "Filter nodes (id > half)" "MATCH (n:Node) WHERE n.id > $((size/2)) RETURN count(n)" $ITERATIONS
    echo ""

    rm -f "$db"
}

# ============================================
# Test 2: Hub-and-Spoke Graph
# ============================================
test_hub_spoke() {
    local size=$1
    local db=$(mktemp /tmp/gql_hub_XXXXXX.db)

    echo "=== Hub-and-Spoke Graph (n=$size) ==="
    echo "Structure: Hub connected to $((size-1)) spokes"
    echo ""

    local build_start=$(get_time_ms)

    # Initialize schema and build using fast SQL
    init_schema "$db"

    # Create hub (node 0) and spokes (nodes 1 to size-1)
    sqlite3 "$db" <<EOF
-- Create hub node
INSERT INTO nodes (id) VALUES (0);
INSERT INTO node_labels (node_id, label) VALUES (0, 'Hub');

-- Create spoke nodes
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $size)
INSERT INTO nodes (id) SELECT x FROM cnt;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $size)
INSERT INTO node_labels (node_id, label) SELECT x, 'Spoke' FROM cnt;

-- Add 'id' property key
INSERT OR IGNORE INTO property_keys (key) VALUES ('id');

-- Add id properties to all nodes
INSERT INTO node_props_int (node_id, key_id, value)
SELECT 0, (SELECT id FROM property_keys WHERE key = 'id'), 0;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $size)
INSERT INTO node_props_int (node_id, key_id, value)
SELECT x, (SELECT id FROM property_keys WHERE key = 'id'), x FROM cnt;
EOF

    local nodes_done=$(get_time_ms)
    local node_time=$((nodes_done - build_start))

    # Create edges from hub to all spokes
    build_hub_edges_fast "$db" 0 1 $((size-1)) "CONNECTS"

    local build_end=$(get_time_ms)
    local edge_time=$((build_end - nodes_done))
    local total_time=$((build_end - build_start))
    local db_size=$(get_file_size "$db")

    echo ""
    printf "  Build: %d nodes in %dms, %d edges in %dms (total: %.2fs)\n" \
        $size $node_time $((size-1)) $edge_time $(echo "scale=2; $total_time/1000" | bc)
    printf "  DB Size: %s\n" "$db_size"

    run_timed_query "$db" "Count all nodes" "MATCH (n) RETURN count(n)" $ITERATIONS
    run_timed_query "$db" "Find hub" "MATCH (h:Hub) RETURN h" $ITERATIONS
    run_timed_query "$db" "Count hub connections" "MATCH (h:Hub)-[:CONNECTS]->(s) RETURN count(s)" $ITERATIONS
    run_timed_query "$db" "Filter spokes (id > half)" "MATCH (h:Hub)-[:CONNECTS]->(s) WHERE s.id > $((size/2)) RETURN count(s)" $ITERATIONS
    echo ""

    rm -f "$db"
}

# ============================================
# Test 3: Dense Mesh Graph
# ============================================
test_dense_mesh() {
    local size=$1
    local conn=$2
    local db=$(mktemp /tmp/gql_mesh_XXXXXX.db)

    echo "=== Dense Mesh Graph (n=$size, connectivity=$conn) ==="
    echo "Structure: Each node connects to next $conn nodes"
    echo ""

    local build_start=$(get_time_ms)

    # Initialize and build using fast SQL
    init_schema "$db"
    build_nodes_fast "$db" 0 $size "Mesh"

    local nodes_done=$(get_time_ms)
    local node_time=$((nodes_done - build_start))

    local total_edges=$((size * conn))
    build_cyclic_edges_fast "$db" $size $conn "LINK" 0

    local build_end=$(get_time_ms)
    local edge_time=$((build_end - nodes_done))
    local total_time=$((build_end - build_start))
    local db_size=$(get_file_size "$db")

    echo ""
    printf "  Build: %d nodes in %dms, %d edges in %dms (total: %.2fs)\n" \
        $size $node_time $total_edges $edge_time $(echo "scale=2; $total_time/1000" | bc)
    printf "  DB Size: %s\n" "$db_size"

    run_timed_query "$db" "Count all nodes" "MATCH (n:Mesh) RETURN count(n)" $ITERATIONS
    run_timed_query "$db" "Count all edges" "MATCH ()-[r:LINK]->() RETURN count(r)" $ITERATIONS
    run_timed_query "$db" "Outgoing from node 0" "MATCH (a:Mesh {id: 0})-[:LINK]->(b) RETURN count(b)" $ITERATIONS
    run_timed_query "$db" "Two-hop neighbors" "MATCH (a:Mesh {id: 0})-[:LINK]->()-[:LINK]->(c) RETURN count(c)" $ITERATIONS
    echo ""

    rm -f "$db"
}

# ============================================
# Test 4: Binary Tree Graph
# ============================================
test_tree() {
    local depth=$1
    local db=$(mktemp /tmp/gql_tree_XXXXXX.db)
    local total=$(( (1 << (depth + 1)) - 1 ))

    echo "=== Binary Tree Graph (depth=$depth) ==="
    echo "Structure: Complete binary tree, $total nodes"
    echo ""

    local build_start=$(get_time_ms)

    # Initialize and build using fast SQL
    init_schema "$db"
    build_nodes_fast "$db" 1 $total "TreeNode"

    local nodes_done=$(get_time_ms)
    local node_time=$((nodes_done - build_start))

    local max_parent=$(( (1 << depth) - 1 ))
    local total_edges=$((total - 1))
    build_tree_edges_fast "$db" $max_parent "CHILD"

    local build_end=$(get_time_ms)
    local edge_time=$((build_end - nodes_done))
    local total_time=$((build_end - build_start))
    local db_size=$(get_file_size "$db")

    echo ""
    printf "  Build: %d nodes in %dms, %d edges in %dms (total: %.2fs)\n" \
        $total $node_time $total_edges $edge_time $(echo "scale=2; $total_time/1000" | bc)
    printf "  DB Size: %s\n" "$db_size"

    run_timed_query "$db" "Count all nodes" "MATCH (n:TreeNode) RETURN count(n)" $ITERATIONS
    run_timed_query "$db" "Find root" "MATCH (n:TreeNode {id: 1}) RETURN n" $ITERATIONS
    run_timed_query "$db" "Root children" "MATCH (r:TreeNode {id: 1})-[:CHILD]->(c) RETURN c.id" $ITERATIONS
    run_timed_query "$db" "Two levels down" "MATCH (r:TreeNode {id: 1})-[:CHILD]->()-[:CHILD]->(gc) RETURN count(gc)" $ITERATIONS
    echo ""

    rm -f "$db"
}

# ============================================
# Run All Tests
# ============================================

echo "========================================"
echo "1K SCALE"
echo "========================================"
echo ""
test_linear_chain 1000
test_hub_spoke 1000
test_dense_mesh 500 3
test_tree 9

echo "========================================"
echo "5K SCALE"
echo "========================================"
echo ""
test_linear_chain 5000
test_hub_spoke 5000
test_dense_mesh 1000 4
test_tree 12

echo "========================================"
echo "10K SCALE"
echo "========================================"
echo ""
test_linear_chain 10000
test_hub_spoke 10000
test_dense_mesh 1000 5
test_tree 13

echo "========================================"
echo "100K SCALE"
echo "========================================"
echo ""
test_linear_chain 100000
test_hub_spoke 100000
test_dense_mesh 5000 5
test_tree 16

echo "========================================"
echo "500K SCALE"
echo "========================================"
echo ""
test_linear_chain 500000
test_hub_spoke 500000
test_dense_mesh 50000 3
test_tree 18

echo "========================================"
echo "1M SCALE"
echo "========================================"
echo ""
test_linear_chain 1000000
test_hub_spoke 1000000
test_dense_mesh 100000 3
test_tree 19

echo "========================================"
echo "Scaled Performance Tests Complete"
echo "========================================"
