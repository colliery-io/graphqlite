#!/bin/bash
# GraphQLite Cache Performance - Repeated Queries
#
# Shows the cache benefit for repeated algorithm calls (typical use case:
# running multiple algorithms on the same graph, or re-running analysis)
#
# Usage: ./perf_cache_repeated.sh [quick|standard|full]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

case "$(uname -s)" in
    Darwin) EXTENSION="$PROJECT_DIR/build/graphqlite.dylib" ;;
    *) EXTENSION="$PROJECT_DIR/build/graphqlite.so" ;;
esac

if [ ! -f "$EXTENSION" ]; then
    echo "Error: Extension not found at $EXTENSION"
    echo "Run 'make extension' first"
    exit 1
fi

MODE="${1:-standard}"

fmt_num() {
    local n=$1
    if [ "$n" -ge 1000000 ]; then printf "%.1fM" $(echo "scale=1; $n/1000000" | bc)
    elif [ "$n" -ge 1000 ]; then printf "%.0fK" $(echo "scale=0; $n/1000" | bc)
    else printf "%d" "$n"; fi
}

get_sizes() {
    case "$MODE" in
        quick)    echo "1000 5000" ;;
        standard) echo "1000 5000 10000" ;;
        full)     echo "1000 5000 10000 50000" ;;
    esac
}

echo ""
echo "GraphQLite Cache Performance - Repeated Queries"
echo "================================================"
echo ""
echo "This benchmark shows cache benefit for running MULTIPLE different"
echo "algorithms on the same graph (typical analytics workflow)."
echo ""

case "$MODE" in
    quick)    echo "  Mode: quick (1K, 5K nodes)" ;;
    standard) echo "  Mode: standard (1K, 5K, 10K nodes)" ;;
    full)     echo "  Mode: full (1K, 5K, 10K, 50K nodes)" ;;
esac
echo ""

declare -a RESULTS

for size in $(get_sizes); do
    edges=$((size * 5))

    echo "Testing graph: $(fmt_num $size) nodes, $(fmt_num $edges) edges..."

    db=$(mktemp /tmp/gqlcache_XXXXXX.db)

    # Build graph
    sqlite3 "$db" <<EOF
CREATE TABLE IF NOT EXISTS nodes (id INTEGER PRIMARY KEY AUTOINCREMENT);
CREATE TABLE IF NOT EXISTS node_labels (node_id INTEGER NOT NULL, label TEXT NOT NULL, PRIMARY KEY (node_id, label));
CREATE TABLE IF NOT EXISTS edges (id INTEGER PRIMARY KEY AUTOINCREMENT, source_id INTEGER NOT NULL, target_id INTEGER NOT NULL, type TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS property_keys (id INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT UNIQUE NOT NULL);
CREATE TABLE IF NOT EXISTS node_props_int (node_id INTEGER NOT NULL, key_id INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY (node_id, key_id));
CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id, type);
CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id, type);
CREATE INDEX IF NOT EXISTS idx_node_labels_label ON node_labels(label, node_id);

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $size)
INSERT INTO nodes (id) SELECT x FROM cnt;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $size)
INSERT INTO node_labels (node_id, label) SELECT x, 'Node' FROM cnt;

INSERT OR IGNORE INTO property_keys (key) VALUES ('id');

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $size)
INSERT INTO node_props_int (node_id, key_id, value) SELECT x, 1, x FROM cnt;

WITH RECURSIVE
  n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < $size),
  o(k) AS (VALUES(1) UNION ALL SELECT k+1 FROM o WHERE k < 5)
INSERT INTO edges (source_id, target_id, type)
SELECT n.x, ((n.x - 1 + o.k) % $size) + 1, 'EDGE' FROM n, o;
EOF

    # Run benchmark - multiple algorithms in sequence (typical workflow)
    result=$(sqlite3 "$db" 2>&1 <<EOF
.load $EXTENSION
.timer on

-- UNCACHED: Run 5 different algorithms (each loads graph from SQLite)
SELECT 'UNCACHED_START';
SELECT length(cypher('RETURN topPageRank(10)'));
SELECT length(cypher('RETURN labelPropagation(5)'));
SELECT length(cypher('RETURN degreeCentrality()'));
SELECT length(cypher('RETURN connectedComponents()'));
SELECT length(cypher('RETURN louvain()'));
SELECT 'UNCACHED_END';

-- Load cache once
SELECT gql_load_graph();

-- CACHED: Run same 5 algorithms (all use cached graph)
SELECT 'CACHED_START';
SELECT length(cypher('RETURN topPageRank(10)'));
SELECT length(cypher('RETURN labelPropagation(5)'));
SELECT length(cypher('RETURN degreeCentrality()'));
SELECT length(cypher('RETURN connectedComponents()'));
SELECT length(cypher('RETURN louvain()'));
SELECT 'CACHED_END';
EOF
)

    # Extract total times for each section
    uncached_time=$(echo "$result" | awk '/UNCACHED_START/,/UNCACHED_END/' | grep "Run Time:" | awk '{sum += $3} END {printf "%.0f", sum * 1000}')
    cached_time=$(echo "$result" | awk '/CACHED_START/,/CACHED_END/' | grep "Run Time:" | awk '{sum += $3} END {printf "%.0f", sum * 1000}')

    speedup="N/A"
    if [ -n "$cached_time" ] && [ "$cached_time" -gt 0 ] && [ -n "$uncached_time" ]; then
        speedup=$(echo "scale=1; $uncached_time / $cached_time" | bc)x
    fi

    RESULTS+=("$size|$edges|$uncached_time|$cached_time|$speedup")

    rm -f "$db"
done

echo ""
echo "┌─────────┬──────────┬────────────────────────────────────────┐"
echo "│         │          │   5 Algorithms Total (PR+LP+DC+WCC+LV) │"
echo "│ Nodes   │ Edges    ├──────────────┬─────────────┬───────────┤"
echo "│         │          │   Uncached   │   Cached    │  Speedup  │"
echo "├─────────┼──────────┼──────────────┼─────────────┼───────────┤"

fmt_time() {
    local ms=$1
    if [ -z "$ms" ] || [ "$ms" = "0" ]; then printf "      <1ms"
    elif [ "$ms" -ge 1000 ]; then printf "   %6.2fs" $(echo "scale=2; $ms/1000" | bc)
    else printf "   %5dms" "$ms"; fi
}

for row in "${RESULTS[@]}"; do
    IFS='|' read -r nodes edges uncached cached speedup <<< "$row"
    printf "│ %7s │ %8s │%s │%s │   %5s   │\n" \
        "$(fmt_num $nodes)" "$(fmt_num $edges)" \
        "$(fmt_time $uncached)" "$(fmt_time $cached)" "$speedup"
done

echo "└─────────┴──────────┴──────────────┴─────────────┴───────────┘"
echo ""
echo "  Algorithms run: PageRank, LabelProp, DegreeCentrality, WCC, Louvain"
echo "  Speedup = Total uncached time / Total cached time"
echo "  Cache benefit: Eliminates 5x graph loading overhead"
echo ""
