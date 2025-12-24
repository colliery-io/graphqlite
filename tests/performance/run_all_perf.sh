#!/bin/bash
# GraphQLite Performance Tests
#
# Target Scale (lightweight embedded graph database):
#   Standard: 100K nodes / 1M edges
#   Stress:   500K nodes / 5M edges
#   Target:   <500ms for typical queries, <100ms for simple traversals
#
# Usage: ./run_all_perf.sh [size]
#   Sizes: 100k, 500k, all (default)
#   Example: ./run_all_perf.sh 100k

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
EXTENSION="$PROJECT_DIR/build/graphqlite.dylib"

# Size to run (default: all)
SIZE_FILTER="${1:-all}"

if [ ! -f "$EXTENSION" ]; then
    echo "Error: Extension not found at $EXTENSION"
    echo "Run 'make extension' first"
    exit 1
fi

# Results array
declare -a RESULTS

# Get current time in milliseconds
get_time_ms() {
    python3 -c 'import time; print(int(time.time() * 1000))'
}

# Initialize schema (matches cypher_schema.c)
init_schema() {
    local db="$1"
    sqlite3 "$db" <<EOF
-- Core tables
CREATE TABLE IF NOT EXISTS nodes (id INTEGER PRIMARY KEY AUTOINCREMENT);
CREATE TABLE IF NOT EXISTS node_labels (node_id INTEGER NOT NULL, label TEXT NOT NULL, PRIMARY KEY (node_id, label));
CREATE TABLE IF NOT EXISTS edges (id INTEGER PRIMARY KEY AUTOINCREMENT, source_id INTEGER NOT NULL, target_id INTEGER NOT NULL, type TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS property_keys (id INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT UNIQUE NOT NULL);
CREATE TABLE IF NOT EXISTS node_props_int (node_id INTEGER NOT NULL, key_id INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY (node_id, key_id));
CREATE TABLE IF NOT EXISTS node_props_text (node_id INTEGER NOT NULL, key_id INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (node_id, key_id));

-- Composite indexes for efficient lookups (from cypher_schema.c)
CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id, type);
CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id, type);
CREATE INDEX IF NOT EXISTS idx_edges_type ON edges(type);
CREATE INDEX IF NOT EXISTS idx_node_labels_label ON node_labels(label, node_id);
CREATE INDEX IF NOT EXISTS idx_property_keys_key ON property_keys(key);
CREATE INDEX IF NOT EXISTS idx_node_props_int_key_value ON node_props_int(key_id, value, node_id);
CREATE INDEX IF NOT EXISTS idx_node_props_text_key_value ON node_props_text(key_id, value, node_id);
EOF
}

# Build nodes with label
build_nodes() {
    local db="$1"
    local count="$2"
    local label="$3"

    sqlite3 "$db" <<EOF
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO nodes (id) SELECT x FROM cnt;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO node_labels (node_id, label) SELECT x, '$label' FROM cnt;
INSERT OR IGNORE INTO property_keys (key) VALUES ('id');
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO node_props_int (node_id, key_id, value)
SELECT x, (SELECT id FROM property_keys WHERE key = 'id'), x FROM cnt;
EOF
}

# Build cyclic edges
build_edges() {
    local db="$1"
    local node_count="$2"
    local edges_per_node="$3"
    local edge_type="$4"

    sqlite3 "$db" <<EOF
WITH RECURSIVE
  nodes_cte(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM nodes_cte WHERE x < $node_count),
  offsets(k) AS (VALUES(1) UNION ALL SELECT k+1 FROM offsets WHERE k < $edges_per_node)
INSERT INTO edges (source_id, target_id, type)
SELECT n.x, ((n.x - 1 + o.k) % $node_count) + 1, '$edge_type'
FROM nodes_cte n, offsets o;
EOF
}

# Run a timed Cypher query and return time in ms
run_query() {
    local db="$1"
    local query="$2"
    local iterations="${3:-1}"

    local result=$(sqlite3 "$db" 2>&1 <<EOF
.load $EXTENSION
.timer on
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $iterations)
SELECT cypher('$query') FROM cnt;
EOF
)
    # Extract real time (format: "Run Time: real 0.001234 user ...") and convert to ms
    local time_str=$(echo "$result" | grep "Run Time:" | tail -1 | sed 's/.*real \([0-9.]*\).*/\1/')
    if [ -n "$time_str" ] && [ "$time_str" != "0" ]; then
        echo "$time_str" | awk '{printf "%.0f", $1 * 1000}'
    else
        echo "ERR"
    fi
}

# Add result to array with pass/fail threshold
add_result() {
    local test="$1"
    local nodes="$2"
    local edges="$3"
    local build_ms="$4"
    local query_ms="$5"
    local threshold="$6"  # Optional threshold in ms
    RESULTS+=("$test|$nodes|$edges|$build_ms|$query_ms|$threshold")
}

# Format number with K/M suffix
fmt_num() {
    local n=$1
    if [ "$n" -ge 1000000 ]; then
        echo "$(echo "scale=1; $n/1000000" | bc)M"
    elif [ "$n" -ge 1000 ]; then
        echo "$(echo "scale=0; $n/1000" | bc)K"
    else
        echo "$n"
    fi
}

# Format time in ms with pass/fail indicator
fmt_time() {
    local ms=$1
    local threshold=$2
    local time_str

    if [ "$ms" = "ERR" ]; then
        echo "ERR"
        return
    fi

    if [ "$ms" -ge 1000 ]; then
        time_str="$(echo "scale=2; $ms/1000" | bc)s"
    else
        time_str="${ms}ms"
    fi

    # Add pass/fail indicator if threshold provided
    if [ -n "$threshold" ] && [ "$threshold" != "" ]; then
        if [ "$ms" -le "$threshold" ]; then
            echo "$time_str OK"
        else
            echo "$time_str SLOW"
        fi
    else
        echo "$time_str"
    fi
}

echo "========================================"
echo "GraphQLite Performance Tests"
echo "========================================"
echo "Target: Lightweight embedded graphs"
echo "  Standard: 100K nodes / 1M edges"
echo "  Stress:   500K nodes / 5M edges"
echo "========================================"
echo ""

ITERATIONS=3

# Run all tests for a given graph size
run_tests_for_size() {
    local size=$1
    local edges_per=$2
    local pr_iters=$3
    local lp_iters=$4

    local size_label=$(fmt_num $size)
    echo "Building ${size_label} node graph..."

    db=$(mktemp /tmp/gql_perf_$$.db)

    # Build graph once
    start=$(get_time_ms)
    init_schema "$db"
    build_nodes "$db" $size "Node"
    build_edges "$db" $size $edges_per "KNOWS"
    build_ms=$(($(get_time_ms) - start))

    echo "  Graph built in ${build_ms}ms"
    echo "  Running queries..."

    # MATCH count
    query_ms=$(run_query "$db" "MATCH (n:Node) RETURN count(n)" $ITERATIONS)
    add_result "MATCH count" $size $((size*edges_per)) $build_ms $query_ms 500

    # 1-hop traversal
    query_ms=$(run_query "$db" "MATCH (a:Node {id: 1})-[:KNOWS]->(b) RETURN count(b)" $ITERATIONS)
    add_result "1-hop traversal" $size $((size*edges_per)) $build_ms $query_ms 100

    # 2-hop traversal
    query_ms=$(run_query "$db" "MATCH (a:Node {id: 1})-[:KNOWS]->()-[:KNOWS]->(c) RETURN count(c)" $ITERATIONS)
    add_result "2-hop traversal" $size $((size*edges_per)) $build_ms $query_ms 500

    # count/min/max aggregation
    query_ms=$(run_query "$db" "MATCH (n:Node) RETURN count(n), min(n.id), max(n.id)" $ITERATIONS)
    add_result "count/min/max" $size $((size*edges_per)) $build_ms $query_ms 500

    # PageRank
    query_ms=$(run_query "$db" "RETURN pageRank(0.85, $pr_iters)" $ITERATIONS)
    add_result "pageRank(i=$pr_iters)" $size $((size*edges_per)) $build_ms $query_ms 2000

    # Label Propagation
    query_ms=$(run_query "$db" "RETURN labelPropagation($lp_iters)" $ITERATIONS)
    add_result "labelProp(i=$lp_iters)" $size $((size*edges_per)) $build_ms $query_ms 3000

    rm -f "$db"
    echo ""
}

# Run tests based on size filter
if [ "$SIZE_FILTER" = "all" ] || [ "$SIZE_FILTER" = "100k" ]; then
    run_tests_for_size 100000 10 5 5
fi

if [ "$SIZE_FILTER" = "all" ] || [ "$SIZE_FILTER" = "500k" ]; then
    run_tests_for_size 500000 10 3 3
fi

# ============================================
# Print Summary Table
# ============================================
echo ""
echo "========================================"
echo "PERFORMANCE SUMMARY"
echo "========================================"
echo ""
printf "%-18s %8s %8s %10s %14s\n" "Test" "Nodes" "Edges" "Build" "Query"
printf "%-18s %8s %8s %10s %14s\n" "------------------" "--------" "--------" "----------" "--------------"

for result in "${RESULTS[@]}"; do
    IFS='|' read -r test nodes edges build_ms query_ms threshold <<< "$result"
    printf "%-18s %8s %8s %10s %14s\n" \
        "$test" \
        "$(fmt_num $nodes)" \
        "$(fmt_num $edges)" \
        "$(fmt_time $build_ms "")" \
        "$(fmt_time $query_ms $threshold)"
done

echo ""
echo "========================================"
echo "Thresholds: 1-hop <100ms, 2-hop <500ms, algorithms <3s"
echo "========================================"
