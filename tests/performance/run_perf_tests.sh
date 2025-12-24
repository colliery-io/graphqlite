#!/bin/bash
# Performance tests for GraphQLite
# Measures execution time for various query patterns

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
EXTENSION="$PROJECT_DIR/build/graphqlite.dylib"

if [ ! -f "$EXTENSION" ]; then
    echo "Error: Extension not found at $EXTENSION"
    echo "Run 'make extension' first"
    exit 1
fi

# Number of iterations for each test
ITERATIONS=${1:-100}

echo "========================================"
echo "GraphQLite Performance Tests"
echo "Iterations per test: $ITERATIONS"
echo "========================================"
echo ""

# Create temp database
TMPDB=$(mktemp /tmp/graphqlite_perf_XXXXXX.db)
trap "rm -f $TMPDB" EXIT

# Helper function to run timed test
run_test() {
    local name="$1"
    local setup="$2"
    local query="$3"
    local iterations="$4"

    # Run the test and capture timing
    local result=$(sqlite3 "$TMPDB" 2>/dev/null <<EOF
.load $EXTENSION
.timer on
$setup
-- Warmup
SELECT cypher('$query');
-- Timed iterations
WITH RECURSIVE cnt(x) AS (
    VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < $iterations
)
SELECT cypher('$query') FROM cnt;
EOF
)

    # Extract timing (last "Run Time:" line)
    local timing=$(echo "$result" | grep "Run Time:" | tail -1 | sed 's/Run Time: //')
    printf "%-45s %s\n" "$name" "$timing"
}

# Initialize schema once
sqlite3 "$TMPDB" 2>/dev/null <<EOF
.load $EXTENSION
SELECT cypher('CREATE (n:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (n:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (n:Person {name: "Charlie", age: 35})');
SELECT cypher('CREATE (n:Person {name: "Diana", age: 28})');
SELECT cypher('CREATE (n:Person {name: "Eve", age: 32})');
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)');
SELECT cypher('MATCH (a:Person {name: "Bob"}), (b:Person {name: "Charlie"}) CREATE (a)-[:KNOWS]->(b)');
SELECT cypher('MATCH (a:Person {name: "Charlie"}), (b:Person {name: "Diana"}) CREATE (a)-[:KNOWS]->(b)');
SELECT cypher('MATCH (a:Person {name: "Diana"}), (b:Person {name: "Eve"}) CREATE (a)-[:KNOWS]->(b)');
EOF

echo "=== Basic Query Performance ==="
echo ""

# Simple RETURN
run_test "RETURN literal" "" "RETURN 1" $ITERATIONS

# RETURN with expression
run_test "RETURN expression" "" "RETURN 1 + 2 * 3" $ITERATIONS

# Simple MATCH
run_test "MATCH all nodes" "" "MATCH (n) RETURN n" $ITERATIONS

# MATCH with label
run_test "MATCH with label" "" "MATCH (n:Person) RETURN n" $ITERATIONS

# MATCH with property filter
run_test "MATCH with WHERE" "" "MATCH (n:Person) WHERE n.age > 25 RETURN n.name" $ITERATIONS

echo ""
echo "=== List Operations Performance ==="
echo ""

# List literal
run_test "List literal [1..10]" "" "RETURN [1,2,3,4,5,6,7,8,9,10]" $ITERATIONS

# List comprehension
run_test "List comprehension" "" "RETURN [x IN [1,2,3,4,5] | x * 2]" $ITERATIONS

# List comprehension with filter
run_test "List comprehension + filter" "" "RETURN [x IN [1,2,3,4,5,6,7,8,9,10] WHERE x > 5 | x * 2]" $ITERATIONS

echo ""
echo "=== List Predicate Performance ==="
echo ""

# any()
run_test "any() predicate" "" "RETURN any(x IN [1,2,3,4,5] WHERE x > 3)" $ITERATIONS

# all()
run_test "all() predicate" "" "RETURN all(x IN [1,2,3,4,5] WHERE x > 0)" $ITERATIONS

# none()
run_test "none() predicate" "" "RETURN none(x IN [1,2,3,4,5] WHERE x > 10)" $ITERATIONS

# single()
run_test "single() predicate" "" "RETURN single(x IN [1,2,3,4,5] WHERE x = 3)" $ITERATIONS

echo ""
echo "=== reduce() Performance ==="
echo ""

# reduce sum small
run_test "reduce() sum [1..5]" "" "RETURN reduce(a = 0, x IN [1,2,3,4,5] | a + x)" $ITERATIONS

# reduce sum medium
run_test "reduce() sum [1..10]" "" "RETURN reduce(a = 0, x IN [1,2,3,4,5,6,7,8,9,10] | a + x)" $ITERATIONS

# reduce product
run_test "reduce() product [1..5]" "" "RETURN reduce(a = 1, x IN [1,2,3,4,5] | a * x)" $ITERATIONS

echo ""
echo "=== Boolean Operations Performance ==="
echo ""

# AND/OR
run_test "AND/OR expression" "" "RETURN true AND false OR true" $ITERATIONS

# XOR
run_test "XOR expression" "" "RETURN true XOR false" $ITERATIONS

# Complex boolean
run_test "Complex boolean" "" "RETURN (1 > 0) XOR (2 < 1) AND true" $ITERATIONS

echo ""
echo "=== Temporal Functions Performance ==="
echo ""

# date()
run_test "date()" "" "RETURN date()" $ITERATIONS

# time()
run_test "time()" "" "RETURN time()" $ITERATIONS

# datetime()
run_test "datetime()" "" "RETURN datetime()" $ITERATIONS

echo ""
echo "=== EXPLAIN Performance ==="
echo ""

# EXPLAIN simple
run_test "EXPLAIN simple" "" "EXPLAIN MATCH (n) RETURN n" $ITERATIONS

# EXPLAIN complex
run_test "EXPLAIN complex" "" "EXPLAIN MATCH (n:Person) WHERE n.age > 21 RETURN n.name" $ITERATIONS

echo ""
echo "=== Relationship Traversal Performance ==="
echo ""

# Single hop
run_test "Single hop traversal" "" "MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name" $ITERATIONS

# Two hops
run_test "Two hop traversal" "" "MATCH (a:Person)-[:KNOWS]->(b)-[:KNOWS]->(c) RETURN a.name, c.name" $ITERATIONS

echo ""
echo "========================================"
echo "Performance tests complete"
echo "========================================"
