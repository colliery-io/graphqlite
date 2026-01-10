#!/bin/bash
# GraphQLite Cache Performance Comparison
#
# Compares uncached vs cached algorithm performance across graph sizes
# Usage: ./perf_cache_comparison.sh [quick|standard|full]

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
ITERATIONS=3

fmt_num() {
    local n=$1
    if [ "$n" -ge 1000000 ]; then printf "%.1fM" $(echo "scale=1; $n/1000000" | bc)
    elif [ "$n" -ge 1000 ]; then printf "%.0fK" $(echo "scale=0; $n/1000" | bc)
    else printf "%d" "$n"; fi
}

get_sizes() {
    case "$MODE" in
        quick)    echo "1000 10000" ;;
        standard) echo "1000 10000 100000" ;;
        full)     echo "1000 10000 100000 500000" ;;
    esac
}

echo ""
echo "GraphQLite Cache Performance Comparison"
echo "========================================"
echo ""

case "$MODE" in
    quick)    echo "  Mode: quick (1K, 10K nodes)" ;;
    standard) echo "  Mode: standard (1K, 10K, 100K nodes)" ;;
    full)     echo "  Mode: full (1K, 10K, 100K, 500K nodes)" ;;
esac
echo ""

# Results arrays
declare -a RESULTS

for size in $(get_sizes); do
    edges=$((size * 5))

    echo "Testing graph: $(fmt_num $size) nodes, $(fmt_num $edges) edges..."

    # Create temp database
    db=$(mktemp /tmp/gqlcache_XXXXXX.db)

    # Build graph using raw SQL (fast)
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

    # Run all benchmarks in a SINGLE sqlite3 session so cache persists
    result=$(sqlite3 "$db" 2>&1 <<EOF
.load $EXTENSION
.timer on

-- UNCACHED PageRank
SELECT 'PR_UNCACHED_START';
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $ITERATIONS)
SELECT cypher('RETURN pageRank(0.85, 10)') FROM cnt;
SELECT 'PR_UNCACHED_END';

-- UNCACHED Label Propagation
SELECT 'LP_UNCACHED_START';
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $ITERATIONS)
SELECT cypher('RETURN labelPropagation(10)') FROM cnt;
SELECT 'LP_UNCACHED_END';

-- UNCACHED Degree Centrality
SELECT 'DC_UNCACHED_START';
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $ITERATIONS)
SELECT cypher('RETURN degreeCentrality()') FROM cnt;
SELECT 'DC_UNCACHED_END';

-- Load cache
SELECT gql_load_graph();

-- CACHED PageRank
SELECT 'PR_CACHED_START';
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $ITERATIONS)
SELECT cypher('RETURN pageRank(0.85, 10)') FROM cnt;
SELECT 'PR_CACHED_END';

-- CACHED Label Propagation
SELECT 'LP_CACHED_START';
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $ITERATIONS)
SELECT cypher('RETURN labelPropagation(10)') FROM cnt;
SELECT 'LP_CACHED_END';

-- CACHED Degree Centrality
SELECT 'DC_CACHED_START';
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $ITERATIONS)
SELECT cypher('RETURN degreeCentrality()') FROM cnt;
SELECT 'DC_CACHED_END';
EOF
)

    # Extract times from output
    extract_time() {
        local start_marker="$1"
        local end_marker="$2"
        echo "$result" | awk "/$start_marker/,/$end_marker/" | grep "Run Time:" | tail -1 | sed 's/.*real \([0-9.]*\).*/\1/' | awk -v n="$ITERATIONS" '{printf "%.0f", ($1 * 1000) / n}'
    }

    pr_uncached=$(extract_time "PR_UNCACHED_START" "PR_UNCACHED_END")
    lp_uncached=$(extract_time "LP_UNCACHED_START" "LP_UNCACHED_END")
    dc_uncached=$(extract_time "DC_UNCACHED_START" "DC_UNCACHED_END")
    pr_cached=$(extract_time "PR_CACHED_START" "PR_CACHED_END")
    lp_cached=$(extract_time "LP_CACHED_START" "LP_CACHED_END")
    dc_cached=$(extract_time "DC_CACHED_START" "DC_CACHED_END")

    # Calculate speedups
    pr_speedup="N/A"
    lp_speedup="N/A"
    dc_speedup="N/A"

    if [ -n "$pr_cached" ] && [ "$pr_cached" -gt 0 ] && [ -n "$pr_uncached" ]; then
        pr_speedup=$(echo "scale=1; $pr_uncached / $pr_cached" | bc)x
    fi
    if [ -n "$lp_cached" ] && [ "$lp_cached" -gt 0 ] && [ -n "$lp_uncached" ]; then
        lp_speedup=$(echo "scale=1; $lp_uncached / $lp_cached" | bc)x
    fi
    if [ -n "$dc_cached" ] && [ "$dc_cached" -gt 0 ] && [ -n "$dc_uncached" ]; then
        dc_speedup=$(echo "scale=1; $dc_uncached / $dc_cached" | bc)x
    fi

    RESULTS+=("$size|$edges|$pr_uncached|$pr_cached|$pr_speedup|$lp_uncached|$lp_cached|$lp_speedup|$dc_uncached|$dc_cached|$dc_speedup")

    rm -f "$db"
done

echo ""
echo "┌─────────┬──────────┬───────────────────────────┬───────────────────────────┬───────────────────────────┐"
echo "│         │          │        PageRank           │     Label Propagation     │    Degree Centrality      │"
echo "│ Nodes   │ Edges    ├─────────┬─────────┬───────┼─────────┬─────────┬───────┼─────────┬─────────┬───────┤"
echo "│         │          │ Uncached│ Cached  │Speedup│ Uncached│ Cached  │Speedup│ Uncached│ Cached  │Speedup│"
echo "├─────────┼──────────┼─────────┼─────────┼───────┼─────────┼─────────┼───────┼─────────┼─────────┼───────┤"

fmt_time() {
    local ms=$1
    if [ -z "$ms" ] || [ "$ms" = "0" ]; then printf "    <1ms"
    elif [ "$ms" -ge 1000 ]; then printf " %6.2fs" $(echo "scale=2; $ms/1000" | bc)
    else printf " %5dms" "$ms"; fi
}

for row in "${RESULTS[@]}"; do
    IFS='|' read -r nodes edges pr_u pr_c pr_s lp_u lp_c lp_s dc_u dc_c dc_s <<< "$row"
    printf "│ %7s │ %8s │%s │%s │ %5s │%s │%s │ %5s │%s │%s │ %5s │\n" \
        "$(fmt_num $nodes)" "$(fmt_num $edges)" \
        "$(fmt_time $pr_u)" "$(fmt_time $pr_c)" "$pr_s" \
        "$(fmt_time $lp_u)" "$(fmt_time $lp_c)" "$lp_s" \
        "$(fmt_time $dc_u)" "$(fmt_time $dc_c)" "$dc_s"
done

echo "└─────────┴──────────┴─────────┴─────────┴───────┴─────────┴─────────┴───────┴─────────┴─────────┴───────┘"
echo ""
echo "  Iterations per measurement: $ITERATIONS"
echo "  Speedup = Uncached Time / Cached Time"
echo ""
