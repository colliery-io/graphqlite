#!/bin/bash
# PageRank Performance Tests
# Tests PageRank algorithm performance at various graph scales

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
ITERATIONS=${1:-3}

echo "========================================"
echo "PageRank Performance Tests"
echo "Iterations per test: $ITERATIONS"
echo "========================================"
echo ""

# Helper to run timed query
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
    printf "  %-40s %s\n" "$name" "$timing"
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

# Get file size
get_file_size() {
    local file="$1"
    if [ -f "$file" ]; then
        local bytes=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
        if [ "$bytes" -gt 1048576 ]; then
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
    local count="$2"
    local label="$3"

    sqlite3 "$db" <<EOF
-- Generate node IDs
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO nodes (id) SELECT x FROM cnt;

-- Add labels
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO node_labels (node_id, label) SELECT x, '$label' FROM cnt;

-- Add 'id' property key if not exists
INSERT OR IGNORE INTO property_keys (key) VALUES ('id');

-- Add id property to each node (for MATCH (n {id: x}) queries)
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO node_props_int (node_id, key_id, value)
SELECT x, (SELECT id FROM property_keys WHERE key = 'id'), x FROM cnt;
EOF
}

# Build cyclic edges using direct SQL
build_cyclic_edges_fast() {
    local db="$1"
    local node_count="$2"
    local edges_per_node="$3"
    local edge_type="$4"

    # Build all edges in one query using recursive CTE
    sqlite3 "$db" <<EOF
WITH RECURSIVE
  nodes_cte(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM nodes_cte WHERE x < $node_count),
  offsets(k) AS (VALUES(1) UNION ALL SELECT k+1 FROM offsets WHERE k < $edges_per_node)
INSERT INTO edges (source_id, target_id, type)
SELECT n.x, ((n.x - 1 + o.k) % $node_count) + 1, '$edge_type'
FROM nodes_cte n, offsets o;
EOF
}

# ============================================
# Test: PageRank at various scales
# ============================================
test_pagerank_scale() {
    local size=$1
    local edges_per_node=$2
    local pr_iterations=${3:-20}
    local db=$(mktemp /tmp/gql_pagerank_$$.db)

    echo "=== PageRank Test (n=$size, edges/node=$edges_per_node, pr_iter=$pr_iterations) ==="

    local build_start=$(get_time_ms)

    # Initialize schema
    init_schema "$db"

    # Create nodes using direct SQL
    build_nodes_fast "$db" $size "Page"

    local nodes_done=$(get_time_ms)
    local node_time=$((nodes_done - build_start))

    # Create cyclic edges using direct SQL
    build_cyclic_edges_fast "$db" $size $edges_per_node "LINKS"

    local build_end=$(get_time_ms)
    local edge_time=$((build_end - nodes_done))
    local total_edges=$((size * edges_per_node))
    local db_size=$(get_file_size "$db")

    printf "  Build: %d nodes in %dms, %d edges in %dms\n" $size $node_time $total_edges $edge_time
    printf "  DB Size: %s\n" "$db_size"
    echo ""

    run_timed_query "$db" "pageRank(0.85, $pr_iterations)" "RETURN pageRank(0.85, $pr_iterations)" $ITERATIONS
    run_timed_query "$db" "topPageRank(10)" "RETURN topPageRank(10)" $ITERATIONS
    echo ""

    rm -f "$db"
}

# ============================================
# Test: Iteration count impact
# ============================================
test_iteration_impact() {
    local size=$1
    local db=$(mktemp /tmp/gql_pagerank_iter_$$.db)

    echo "=== Iteration Impact (n=$size) ==="

    # Build graph using fast SQL
    local build_start=$(get_time_ms)
    init_schema "$db"
    build_nodes_fast "$db" $size "Page"
    build_cyclic_edges_fast "$db" $size 3 "LINKS"
    local build_end=$(get_time_ms)

    local db_size=$(get_file_size "$db")
    printf "  Graph: %d nodes, %d edges | DB: %s | Built in %dms\n" $size $((size * 3)) "$db_size" $((build_end - build_start))
    echo ""

    run_timed_query "$db" "pageRank(0.85, 5)" "RETURN pageRank(0.85, 5)" $ITERATIONS
    run_timed_query "$db" "pageRank(0.85, 10)" "RETURN pageRank(0.85, 10)" $ITERATIONS
    run_timed_query "$db" "pageRank(0.85, 15)" "RETURN pageRank(0.85, 15)" $ITERATIONS
    run_timed_query "$db" "pageRank(0.85, 20)" "RETURN pageRank(0.85, 20)" $ITERATIONS
    echo ""

    rm -f "$db"
}

# ============================================
# Run Tests
# ============================================

echo "========================================"
echo "SMALL SCALE (1K-10K)"
echo "========================================"
echo ""
test_pagerank_scale 1000 3 10
test_pagerank_scale 5000 3 10
test_pagerank_scale 10000 3 10

echo "========================================"
echo "MEDIUM SCALE (50K-100K)"
echo "========================================"
echo ""
test_pagerank_scale 50000 3 5
test_pagerank_scale 100000 3 5

echo "========================================"
echo "ITERATION SCALING (10K nodes)"
echo "========================================"
echo ""
test_iteration_impact 10000

echo "========================================"
echo "LARGE SCALE (500K-1M)"
echo "========================================"
echo ""
test_pagerank_scale 500000 2 3
test_pagerank_scale 1000000 2 3

echo "========================================"
echo "PageRank Performance Tests Complete"
echo "========================================"
