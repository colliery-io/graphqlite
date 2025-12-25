#!/bin/bash
# GraphQLite Unified Performance Tests
#
# Usage: ./run_all_perf.sh [quick|standard|full]
#   quick:    10K nodes only (~30s)
#   standard: 100K, 500K nodes (~3min) - default
#   full:     10K, 100K, 500K, 1M nodes (~10min)

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
ITERATIONS="${PERF_ITERATIONS:-3}"

# Results storage
declare -a RESULTS

get_time_ms() { python3 -c 'import time; print(int(time.time() * 1000))'; }

fmt_num() {
    local n=$1
    if [ "$n" -ge 1000000 ]; then printf "%.1fM" $(echo "scale=1; $n/1000000" | bc)
    elif [ "$n" -ge 1000 ]; then printf "%.0fK" $(echo "scale=0; $n/1000" | bc)
    else printf "%d" "$n"; fi
}

fmt_time() {
    local ms=$1
    if [ "$ms" = "ERR" ] || [ -z "$ms" ]; then printf "-"
    elif [ "$ms" -ge 1000 ]; then printf "%.2fs" $(echo "scale=2; $ms/1000" | bc)
    else printf "%dms" "$ms"; fi
}

fmt_rate() {
    local count=$1 ms=$2
    if [ "$ms" -le 0 ] || [ "$ms" = "ERR" ]; then printf "-"
    else printf "%s/s" "$(fmt_num $(echo "$count * 1000 / $ms" | bc))"; fi
}

add_row() { RESULTS+=("$1|$2|$3|$4|$5|$6"); }

# ============================================
# Schema & Build Helpers
# ============================================

init_schema() {
    local db="$1"
    sqlite3 "$db" <<EOF
CREATE TABLE IF NOT EXISTS nodes (id INTEGER PRIMARY KEY AUTOINCREMENT);
CREATE TABLE IF NOT EXISTS node_labels (node_id INTEGER NOT NULL, label TEXT NOT NULL, PRIMARY KEY (node_id, label));
CREATE TABLE IF NOT EXISTS edges (id INTEGER PRIMARY KEY AUTOINCREMENT, source_id INTEGER NOT NULL, target_id INTEGER NOT NULL, type TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS property_keys (id INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT UNIQUE NOT NULL);
CREATE TABLE IF NOT EXISTS node_props_int (node_id INTEGER NOT NULL, key_id INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY (node_id, key_id));
CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id, type);
CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id, type);
CREATE INDEX IF NOT EXISTS idx_node_labels_label ON node_labels(label, node_id);
EOF
}

build_nodes_sql() {
    local db="$1" count="$2" label="$3"
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

build_cyclic_edges() {
    local db="$1" count="$2" per_node="$3" type="$4"
    sqlite3 "$db" <<EOF
WITH RECURSIVE
  n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < $count),
  o(k) AS (VALUES(1) UNION ALL SELECT k+1 FROM o WHERE k < $per_node)
INSERT INTO edges (source_id, target_id, type)
SELECT n.x, ((n.x - 1 + o.k) % $count) + 1, '$type' FROM n, o;
EOF
}

build_chain_edges() {
    local db="$1" count="$2" type="$3"
    sqlite3 "$db" <<EOF
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $count)
INSERT INTO edges (source_id, target_id, type) SELECT x, x+1, '$type' FROM cnt WHERE x < $count;
EOF
}

build_tree_edges() {
    local db="$1" count="$2" type="$3"
    sqlite3 "$db" <<EOF
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $((count/2)))
INSERT INTO edges (source_id, target_id, type)
SELECT x, x*2, '$type' FROM cnt WHERE x*2 <= $count
UNION ALL SELECT x, x*2+1, '$type' FROM cnt WHERE x*2+1 <= $count;
EOF
}

build_bipartite_edges() {
    local db="$1" set_a="$2" set_b="$3" per_a="$4" type="$5"
    sqlite3 "$db" <<EOF
WITH RECURSIVE
  a(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM a WHERE x < $set_a),
  o(k) AS (VALUES(0) UNION ALL SELECT k+1 FROM o WHERE k < $per_a - 1)
INSERT INTO edges (source_id, target_id, type)
SELECT a.x, $((set_a+1)) + ((a.x + o.k - 1) % $set_b), '$type' FROM a, o;
EOF
}

build_normal_edges() {
    local db="$1" count="$2" mean="$3" stddev="$4" type="$5"
    sqlite3 "$db" <<EOF
CREATE TEMP TABLE node_degrees AS
WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < $count)
SELECT x AS node_id, MAX(1, CAST($mean + $stddev * (
    (abs(random()) % 1000000 + abs(random()) % 1000000 + abs(random()) % 1000000 +
     abs(random()) % 1000000 + abs(random()) % 1000000 + abs(random()) % 1000000) / 1000000.0 - 3.0
) / 0.707 AS INTEGER)) AS degree FROM n;
WITH RECURSIVE offsets AS (SELECT 1 AS k UNION ALL SELECT k+1 FROM offsets WHERE k < (SELECT MAX(degree) FROM node_degrees))
INSERT INTO edges (source_id, target_id, type)
SELECT d.node_id, ((d.node_id - 1 + o.k) % $count) + 1, '$type' FROM node_degrees d JOIN offsets o ON o.k <= d.degree;
DROP TABLE node_degrees;
EOF
}

build_powerlaw_edges() {
    local db="$1" count="$2" min="$3" max="$4" type="$5"
    sqlite3 "$db" <<EOF
CREATE TEMP TABLE node_degrees AS
WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x < $count)
SELECT x AS node_id, MIN($max, MAX($min, CAST($min / POWER((abs(random()) % 999999 + 1) / 1000000.0, 0.67) AS INTEGER))) AS degree FROM n;
WITH RECURSIVE offsets AS (SELECT 1 AS k UNION ALL SELECT k+1 FROM offsets WHERE k < (SELECT MAX(degree) FROM node_degrees))
INSERT INTO edges (source_id, target_id, type)
SELECT d.node_id, ((d.node_id - 1 + o.k) % $count) + 1, '$type' FROM node_degrees d JOIN offsets o ON o.k <= d.degree;
DROP TABLE node_degrees;
EOF
}

run_query() {
    local db="$1" query="$2" iters="${3:-$ITERATIONS}"
    local result=$(sqlite3 "$db" 2>&1 <<EOF
.load $EXTENSION
.timer on
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $iters)
SELECT cypher('$query') FROM cnt;
EOF
)
    local time_str=$(echo "$result" | grep "Run Time:" | tail -1 | sed 's/.*real \([0-9.]*\).*/\1/')
    [ -n "$time_str" ] && [ "$time_str" != "0" ] && echo "$time_str" | awk -v n="$iters" '{printf "%.0f", ($1 * 1000) / n}' || echo "ERR"
}

# ============================================
# Test Runners
# ============================================

get_sizes() {
    case "$MODE" in
        quick)    echo "10000" ;;
        standard) echo "100000 500000" ;;
        full)     echo "10000 100000 500000 1000000" ;;
    esac
}

run_all_tests() {
    local sizes=$(get_sizes)
    local total_tests=0

    # Count tests
    for size in $sizes; do
        total_tests=$((total_tests + 1))  # insertion
        total_tests=$((total_tests + 8))  # topologies
        total_tests=$((total_tests + 3))  # algorithms
        total_tests=$((total_tests + 6))  # queries (lookup, 1-hop, 2-hop, 3-hop, filter, match all)
    done

    local current=0
    echo ""

    for size in $sizes; do
        local db edges

        # Insertion
        current=$((current + 1))
        printf "\r  Running tests... %d/%d" $current $total_tests
        db=$(mktemp /tmp/gql_XXXXXX.db)
        edges=$((size * 5))
        local start=$(get_time_ms)
        init_schema "$db"
        build_nodes_sql "$db" $size "Node"
        build_cyclic_edges "$db" $size 5 "EDGE"
        local elapsed=$(($(get_time_ms) - start))
        add_row "Insert" "Bulk load" "$(fmt_num $size)" "$(fmt_num $edges)" "$(fmt_time $elapsed)" "$(fmt_rate $((size+edges)) $elapsed)"
        rm -f "$db"

        # Topologies
        for topo in "chain" "tree" "sparse" "moderate" "dense" "bipartite" "normal" "powerlaw"; do
            current=$((current + 1))
            printf "\r  Running tests... %d/%d" $current $total_tests
            db=$(mktemp /tmp/gql_XXXXXX.db)
            init_schema "$db"
            build_nodes_sql "$db" $size "Node"

            case "$topo" in
                chain)    build_chain_edges "$db" $size "LINK"; edges=$((size-1)) ;;
                tree)     build_tree_edges "$db" $size "LINK"; edges=$((size-1)) ;;
                sparse)   build_cyclic_edges "$db" $size 5 "LINK"; edges=$((size*5)) ;;
                moderate) build_cyclic_edges "$db" $size 20 "LINK"; edges=$((size*20)) ;;
                dense)
                    if [ $size -le 100000 ]; then
                        build_cyclic_edges "$db" $size 50 "LINK"; edges=$((size*50))
                    else
                        rm -f "$db"; continue
                    fi ;;
                bipartite) build_bipartite_edges "$db" $((size/2)) $((size/2)) 10 "LINK"; edges=$((size/2*10)) ;;
                normal)   build_normal_edges "$db" $size 10 5 "LINK"; edges=$(sqlite3 "$db" "SELECT COUNT(*) FROM edges") ;;
                powerlaw) build_powerlaw_edges "$db" $size 1 100 "LINK"; edges=$(sqlite3 "$db" "SELECT COUNT(*) FROM edges") ;;
            esac

            local hop1=$(run_query "$db" "MATCH (a:Node {id: 1})-[:LINK]->(b) RETURN count(b)")
            local hop2=$(run_query "$db" "MATCH (a:Node {id: 1})-[:LINK]->()-[:LINK]->(c) RETURN count(c)")
            add_row "Topology" "$topo" "$(fmt_num $size)" "$(fmt_num $edges)" "$(fmt_time $hop1)" "$(fmt_time $hop2)"
            rm -f "$db"
        done

        # Algorithms
        db=$(mktemp /tmp/gql_XXXXXX.db)
        init_schema "$db"
        build_nodes_sql "$db" $size "Node"
        build_cyclic_edges "$db" $size 5 "EDGE"

        local pr_iters=10 lp_iters=10
        [ $size -ge 100000 ] && pr_iters=5 && lp_iters=5
        [ $size -ge 500000 ] && pr_iters=3 && lp_iters=3

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local pr=$(run_query "$db" "RETURN pageRank(0.85, $pr_iters)")
        add_row "Algorithm" "PageRank" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $pr)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local lp=$(run_query "$db" "RETURN labelPropagation($lp_iters)")
        add_row "Algorithm" "LabelProp" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $lp)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local agg=$(run_query "$db" "MATCH (n:Node) RETURN count(n), min(n.id), max(n.id)")
        add_row "Algorithm" "Aggregates" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $agg)" "-"
        rm -f "$db"

        # Queries
        db=$(mktemp /tmp/gql_XXXXXX.db)
        init_schema "$db"
        build_nodes_sql "$db" $size "Node"
        build_cyclic_edges "$db" $size 5 "EDGE"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local lookup=$(run_query "$db" "MATCH (n:Node {id: $((size/2))}) RETURN n")
        add_row "Query" "Lookup" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $lookup)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local h1=$(run_query "$db" "MATCH (a:Node {id: 1})-[:EDGE]->(b) RETURN count(b)")
        add_row "Query" "1-hop" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $h1)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local h2=$(run_query "$db" "MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->(c) RETURN count(c)")
        add_row "Query" "2-hop" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $h2)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local h3=$(run_query "$db" "MATCH (a:Node {id: 1})-[:EDGE]->()-[:EDGE]->()-[:EDGE]->(d) RETURN count(d)")
        add_row "Query" "3-hop" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $h3)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local flt=$(run_query "$db" "MATCH (n:Node) WHERE n.id > $((size/2)) RETURN count(n)")
        add_row "Query" "Filter" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $flt)" "-"

        current=$((current + 1)); printf "\r  Running tests... %d/%d" $current $total_tests
        local all=$(run_query "$db" "MATCH (n:Node) RETURN count(n)")
        add_row "Query" "MATCH all" "$(fmt_num $size)" "$(fmt_num $((size*5)))" "$(fmt_time $all)" "-"
        rm -f "$db"
    done

    printf "\r  Running tests... done!          \n"
}

# ============================================
# Output
# ============================================

print_results() {
    echo ""
    echo "┌────────────┬────────────┬─────────┬──────────┬──────────┬──────────┐"
    echo "│ Category   │ Test       │ Nodes   │ Edges    │ Time     │ Extra    │"
    echo "├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤"

    local last_cat=""
    for row in "${RESULTS[@]}"; do
        IFS='|' read -r cat test nodes edges time extra <<< "$row"

        # Print separator between categories
        if [ "$cat" != "$last_cat" ] && [ -n "$last_cat" ]; then
            echo "├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤"
        fi
        last_cat="$cat"

        printf "│ %-10s │ %-10s │ %7s │ %8s │ %8s │ %8s │\n" \
            "$cat" "$test" "$nodes" "$edges" "$time" "$extra"
    done

    echo "└────────────┴────────────┴─────────┴──────────┴──────────┴──────────┘"
    echo ""
    echo "  Mode: $MODE | Iterations per query: $ITERATIONS"
    echo "  Time column shows avg per query for Query/Algorithm tests"
    echo "  Topology tests show 1-hop time (Time) and 2-hop time (Extra)"
    echo ""
}

# ============================================
# Main
# ============================================

echo ""
echo "GraphQLite Performance Tests"
echo "============================"

case "$MODE" in
    quick)    echo "  Mode: quick (10K nodes)" ;;
    standard) echo "  Mode: standard (100K, 500K nodes)" ;;
    full)     echo "  Mode: full (10K, 100K, 500K, 1M nodes)" ;;
    *)
        echo "Unknown mode: $MODE"
        echo "Usage: $0 [quick|standard|full]"
        exit 1
        ;;
esac

run_all_tests
print_results
