#!/bin/bash
# GraphQLite Hop Depth Performance
#
# Tests traversal performance at various hop depths
# Usage: ./perf_hop_depth.sh [quick|standard|full]

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
        quick)    echo "10000" ;;
        standard) echo "10000 100000" ;;
        full)     echo "10000 100000 500000" ;;
    esac
}

echo ""
echo "GraphQLite Hop Depth Performance"
echo "================================="
echo ""

case "$MODE" in
    quick)    echo "  Mode: quick (10K nodes)" ;;
    standard) echo "  Mode: standard (10K, 100K nodes)" ;;
    full)     echo "  Mode: full (10K, 100K, 500K nodes)" ;;
esac
echo ""

declare -a RESULTS

for size in $(get_sizes); do
    edges=$((size * 5))

    echo "Testing graph: $(fmt_num $size) nodes, $(fmt_num $edges) edges..."

    db=$(mktemp /tmp/gqlhop_XXXXXX.db)

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

    # Run hop depth tests in single session
    result=$(sqlite3 "$db" 2>&1 <<EOF
.load $EXTENSION
.timer on

SELECT 'HOP1_START';
SELECT cypher('MATCH (a:Node {id: 1})-[:EDGE]->(b) RETURN count(b)');
SELECT 'HOP1_END';

SELECT 'HOP2_START';
SELECT cypher('MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->(c) RETURN count(c)');
SELECT 'HOP2_END';

SELECT 'HOP3_START';
SELECT cypher('MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->()-[:EDGE]->(d) RETURN count(d)');
SELECT 'HOP3_END';

SELECT 'HOP4_START';
SELECT cypher('MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->()-[:EDGE]->()-[:EDGE]->(e) RETURN count(e)');
SELECT 'HOP4_END';

SELECT 'HOP5_START';
SELECT cypher('MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->()-[:EDGE]->()-[:EDGE]->()-[:EDGE]->(f) RETURN count(f)');
SELECT 'HOP5_END';

SELECT 'HOP6_START';
SELECT cypher('MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->()-[:EDGE]->()-[:EDGE]->()-[:EDGE]->()-[:EDGE]->(g) RETURN count(g)');
SELECT 'HOP6_END';
EOF
)

    # Extract times and counts
    extract_data() {
        local marker="$1"
        local section=$(echo "$result" | awk "/${marker}_START/,/${marker}_END/")
        local time=$(echo "$section" | grep "Run Time:" | tail -1 | sed 's/.*real \([0-9.]*\).*/\1/' | awk '{printf "%.0f", $1 * 1000}')
        local count=$(echo "$section" | grep -o '"count([^"]*)":[0-9]*' | grep -o '[0-9]*$' | head -1)
        echo "$time|$count"
    }

    h1=$(extract_data "HOP1")
    h2=$(extract_data "HOP2")
    h3=$(extract_data "HOP3")
    h4=$(extract_data "HOP4")
    h5=$(extract_data "HOP5")
    h6=$(extract_data "HOP6")

    RESULTS+=("$size|$edges|$h1|$h2|$h3|$h4|$h5|$h6")

    rm -f "$db"
done

echo ""
echo "┌─────────┬──────────┬────────────────┬────────────────┬────────────────┬────────────────┬────────────────┬────────────────┐"
echo "│         │          │     1-hop      │     2-hop      │     3-hop      │     4-hop      │     5-hop      │     6-hop      │"
echo "│ Nodes   │ Edges    ├────────┬───────┼────────┬───────┼────────┬───────┼────────┬───────┼────────┬───────┼────────┬───────┤"
echo "│         │          │  Time  │ Count │  Time  │ Count │  Time  │ Count │  Time  │ Count │  Time  │ Count │  Time  │ Count │"
echo "├─────────┼──────────┼────────┼───────┼────────┼───────┼────────┼───────┼────────┼───────┼────────┼───────┼────────┼───────┤"

fmt_time() {
    local ms=$1
    if [ -z "$ms" ] || [ "$ms" = "0" ]; then printf "  <1ms"
    elif [ "$ms" -ge 1000 ]; then printf "%5.1fs" $(echo "scale=1; $ms/1000" | bc)
    else printf "%4dms" "$ms"; fi
}

fmt_count() {
    local c=$1
    if [ -z "$c" ]; then printf "    -"
    elif [ "$c" -ge 1000000 ]; then printf "%4.1fM" $(echo "scale=1; $c/1000000" | bc)
    elif [ "$c" -ge 1000 ]; then printf "%4.0fK" $(echo "scale=0; $c/1000" | bc)
    else printf "%5d" "$c"; fi
}

for row in "${RESULTS[@]}"; do
    IFS='|' read -r nodes edges t1 c1 t2 c2 t3 c3 t4 c4 t5 c5 t6 c6 <<< "$row"
    printf "│ %7s │ %8s │%s │%s │%s │%s │%s │%s │%s │%s │%s │%s │%s │%s │\n" \
        "$(fmt_num $nodes)" "$(fmt_num $edges)" \
        "$(fmt_time $t1)" "$(fmt_count $c1)" \
        "$(fmt_time $t2)" "$(fmt_count $c2)" \
        "$(fmt_time $t3)" "$(fmt_count $c3)" \
        "$(fmt_time $t4)" "$(fmt_count $c4)" \
        "$(fmt_time $t5)" "$(fmt_count $c5)" \
        "$(fmt_time $t6)" "$(fmt_count $c6)"
done

echo "└─────────┴──────────┴────────┴───────┴────────┴───────┴────────┴───────┴────────┴───────┴────────┴───────┴────────┴───────┘"
echo ""
echo "  Time = query execution time"
echo "  Count = number of paths found (grows as degree^hops)"
echo "  Graph has average out-degree of 5, so paths grow ~5^n"
echo ""
