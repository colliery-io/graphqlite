#!/bin/bash
#
# CLI Test Suite for gqlite binary
#
# Usage: ./tests/cli/run_cli_tests.sh [path_to_gqlite]
#
# IMPORTANT: This test suite expects a RELEASE build of gqlite.
# Debug builds will fail the "no debug output" test.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GQLITE="${1:-$PROJECT_ROOT/build/gqlite}"
TEMP_DIR=$(mktemp -d)
PASSED=0
FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

log_pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    PASSED=$((PASSED + 1))
}

log_fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    echo "  Expected: $2"
    echo "  Got: $3"
    FAILED=$((FAILED + 1))
}

# Helper to filter startup messages (but NOT debug output)
filter_startup() {
    grep -v "Opened database" | grep -v "executor initialized"
}

# Check if gqlite binary exists
if [ ! -x "$GQLITE" ]; then
    echo "Error: gqlite binary not found at $GQLITE"
    echo "Build it with: angreal build app --release"
    exit 1
fi

echo "Testing gqlite CLI at: $GQLITE"
echo "Temp directory: $TEMP_DIR"
echo ""

# ============================================================
# Test 0: No debug output in release build (CRITICAL)
# ============================================================
test_no_debug_output() {
    local db="$TEMP_DIR/test0.db"
    local result

    result=$(echo 'CREATE (n:Test {name: "Alice"});
MATCH (n:Test) RETURN n.name;' | "$GQLITE" "$db" 2>&1)

    if echo "$result" | grep -q "CYPHER_DEBUG\|DEBUG -"; then
        log_fail "No debug output in release build" "no CYPHER_DEBUG or DEBUG messages" "found debug output"
        echo "  This test requires a RELEASE build. Run: make graphqlite RELEASE=1"
    else
        log_pass "No debug output in release build"
    fi
}

# ============================================================
# Test 1: Basic single statement
# ============================================================
test_basic_single_statement() {
    local db="$TEMP_DIR/test1.db"
    local result

    result=$(echo 'CREATE (n:Person {name: "Alice"});' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Nodes created: 1"; then
        log_pass "Basic single statement"
    else
        log_fail "Basic single statement" "Nodes created: 1" "$result"
    fi
}

# ============================================================
# Test 2: Multi-line statement with semicolon
# ============================================================
test_multiline_statement() {
    local db="$TEMP_DIR/test2.db"
    local result

    result=$(echo 'CREATE (a:Person {name: "Alice"});
CREATE (b:Person {name: "Bob"});
MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b);' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Relationships created: 1"; then
        log_pass "Multi-line statement with MATCH-CREATE works"
    else
        log_fail "Multi-line statement with MATCH-CREATE works" "Relationships created: 1" "$result"
    fi
}

# ============================================================
# Test 3: Friend-of-friend query (the Issue #5 test case)
# ============================================================
test_friend_of_friend() {
    local db="$TEMP_DIR/test3.db"
    local result

    result=$(echo 'CREATE (a:Person {name: "Alice", age: 30});
CREATE (b:Person {name: "Bob", age: 25});
CREATE (c:Person {name: "Charlie", age: 35});
MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b);
MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"})
    CREATE (b)-[:KNOWS]->(c);
MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
    RETURN fof.name;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Charlie"; then
        log_pass "Friend-of-friend query returns correct result"
    else
        log_fail "Friend-of-friend query returns correct result" "Charlie" "$result"
    fi
}

# ============================================================
# Test 4: Multiple statements in sequence
# ============================================================
test_multiple_statements() {
    local db="$TEMP_DIR/test4.db"
    local result

    result=$(echo 'CREATE (n:A);
CREATE (n:B);
CREATE (n:C);
MATCH (n) RETURN count(n);' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "3"; then
        log_pass "Multiple statements counted correctly"
    else
        log_fail "Multiple statements counted correctly" "3" "$result"
    fi
}

# ============================================================
# Test 5: Properties and RETURN
# ============================================================
test_properties_return() {
    local db="$TEMP_DIR/test5.db"
    local result

    result=$(echo 'CREATE (n:Person {name: "Alice", age: 30});
MATCH (n:Person) RETURN n.name, n.age;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Alice" && echo "$result" | grep -q "30"; then
        log_pass "Properties returned correctly"
    else
        log_fail "Properties returned correctly" "Alice and 30" "$result"
    fi
}

# ============================================================
# Test 6: Dot command .tables
# ============================================================
test_dot_tables() {
    local db="$TEMP_DIR/test6.db"
    local result

    result=$(echo 'CREATE (n:Test);
.tables' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "nodes" && echo "$result" | grep -q "edges"; then
        log_pass "Dot command .tables works"
    else
        log_fail "Dot command .tables works" "nodes and edges tables" "$result"
    fi
}

# ============================================================
# Test 7: Dot command .schema
# ============================================================
test_dot_schema() {
    local db="$TEMP_DIR/test7.db"
    local result

    result=$(echo 'CREATE (n:Test);
.schema' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "CREATE TABLE" && echo "$result" | grep -q "nodes"; then
        log_pass "Dot command .schema works"
    else
        log_fail "Dot command .schema works" "CREATE TABLE nodes" "$result"
    fi
}

# ============================================================
# Test 8: Dot command .stats
# ============================================================
test_dot_stats() {
    local db="$TEMP_DIR/test8.db"
    local result

    result=$(echo 'CREATE (n:Test);
CREATE (m:Test);
.stats' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Nodes" && echo "$result" | grep -q "2"; then
        log_pass "Dot command .stats works"
    else
        log_fail "Dot command .stats works" "Nodes: 2" "$result"
    fi
}

# ============================================================
# Test 9: Dot command .help
# ============================================================
test_dot_help() {
    local db="$TEMP_DIR/test9.db"
    local result

    result=$(echo '.help' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Cypher" && echo "$result" | grep -q ".quit"; then
        log_pass "Dot command .help works"
    else
        log_fail "Dot command .help works" "help text with .quit" "$result"
    fi
}

# ============================================================
# Test 10: Error handling
# ============================================================
test_error_handling() {
    local db="$TEMP_DIR/test10.db"
    local result

    result=$(echo 'RETURN unknown_variable;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -qi "failed\|error"; then
        log_pass "Error handling works"
    else
        log_fail "Error handling works" "error message" "$result"
    fi
}

# ============================================================
# Test 11: Empty lines are skipped
# ============================================================
test_empty_lines() {
    local db="$TEMP_DIR/test11.db"
    local result

    result=$(echo '

CREATE (n:Test);

MATCH (n) RETURN count(n);

' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "1"; then
        log_pass "Empty lines handled correctly"
    else
        log_fail "Empty lines handled correctly" "count of 1" "$result"
    fi
}

# ============================================================
# Test 12: WHERE clause in multi-line
# ============================================================
test_where_multiline() {
    local db="$TEMP_DIR/test12.db"
    local result

    result=$(echo 'CREATE (a:Person {name: "Alice", age: 30});
CREATE (b:Person {name: "Bob", age: 25});
MATCH (n:Person)
    WHERE n.age > 28
    RETURN n.name;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Alice" && ! echo "$result" | grep -q "Bob"; then
        log_pass "WHERE clause in multi-line works"
    else
        log_fail "WHERE clause in multi-line works" "only Alice (age > 28)" "$result"
    fi
}

# ============================================================
# Test 13: File input (non-TTY mode)
# ============================================================
test_file_input() {
    local db="$TEMP_DIR/test13.db"
    local input_file="$TEMP_DIR/input.cypher"
    local result

    cat > "$input_file" << 'EOF'
CREATE (n:FileTest {value: 42});
MATCH (n:FileTest) RETURN n.value;
EOF

    result=$("$GQLITE" "$db" < "$input_file" 2>&1 | filter_startup)

    if echo "$result" | grep -q "42"; then
        log_pass "File input works correctly"
    else
        log_fail "File input works correctly" "42" "$result"
    fi
}

# ============================================================
# Test 14: Semicolon inside string literal
# ============================================================
test_semicolon_in_string() {
    local db="$TEMP_DIR/test14.db"
    local result

    result=$(echo 'CREATE (n:Test {text: "Hello; World"});
MATCH (n:Test) RETURN n.text;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Hello; World"; then
        log_pass "Semicolon inside string literal works"
    else
        log_fail "Semicolon inside string literal works" "Hello; World" "$result"
    fi
}

# ============================================================
# Test 15: Multiple semicolons in string
# ============================================================
test_multiple_semicolons_in_string() {
    local db="$TEMP_DIR/test15.db"
    local result

    result=$(echo 'CREATE (n:Test {sql: "SELECT * FROM foo; DELETE FROM bar;"});
MATCH (n:Test) RETURN n.sql;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "SELECT \* FROM foo; DELETE FROM bar;"; then
        log_pass "Multiple semicolons in string literal works"
    else
        log_fail "Multiple semicolons in string literal works" "SELECT * FROM foo; DELETE FROM bar;" "$result"
    fi
}

# ============================================================
# Test 16: Escaped quotes in string
# ============================================================
test_escaped_quotes() {
    local db="$TEMP_DIR/test16.db"
    local result

    result=$(echo "CREATE (n:Test {quote: 'He said \"hello\"'});
MATCH (n:Test) RETURN n.quote;" | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q 'He said "hello"'; then
        log_pass "Escaped quotes in string works"
    else
        log_fail "Escaped quotes in string works" 'He said "hello"' "$result"
    fi
}

# ============================================================
# Test 17: Long multi-line statement
# ============================================================
test_long_multiline() {
    local db="$TEMP_DIR/test17.db"
    local result

    result=$(echo 'CREATE (a:Person {name: "Alice"});
CREATE (b:Person {name: "Bob"});
CREATE (c:Person {name: "Charlie"});
CREATE (d:Person {name: "Diana"});
CREATE (e:Person {name: "Eve"});
MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b);
MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"})
    CREATE (b)-[:KNOWS]->(c);
MATCH (c:Person {name: "Charlie"}), (d:Person {name: "Diana"})
    CREATE (c)-[:KNOWS]->(d);
MATCH (d:Person {name: "Diana"}), (e:Person {name: "Eve"})
    CREATE (d)-[:KNOWS]->(e);
MATCH (p:Person)
    RETURN p.name
    ORDER BY p.name;' | "$GQLITE" "$db" 2>&1 | filter_startup)

    # Should have all people we created
    if echo "$result" | grep -q "Alice" && echo "$result" | grep -q "Eve"; then
        log_pass "Long multi-line statement works"
    else
        log_fail "Long multi-line statement works" "Alice through Eve" "$result"
    fi
}

# ============================================================
# Test 18: Unknown dot command
# ============================================================
test_unknown_dot_command() {
    local db="$TEMP_DIR/test18.db"
    local result

    result=$(echo '.unknown' | "$GQLITE" "$db" 2>&1 | filter_startup)

    if echo "$result" | grep -q "Unknown command"; then
        log_pass "Unknown dot command handled"
    else
        log_fail "Unknown dot command handled" "Unknown command" "$result"
    fi
}

# ============================================================
# Run all tests
# ============================================================
echo "Running CLI tests..."
echo "===================="
echo ""

# Critical test first - verifies release build
test_no_debug_output

# Basic functionality
test_basic_single_statement
test_multiline_statement
test_friend_of_friend
test_multiple_statements
test_properties_return

# Dot commands
test_dot_tables
test_dot_schema
test_dot_stats
test_dot_help
test_unknown_dot_command

# Error handling and edge cases
test_error_handling
test_empty_lines
test_where_multiline
test_file_input

# String handling edge cases
test_semicolon_in_string
test_multiple_semicolons_in_string
test_escaped_quotes

# Complex scenarios
test_long_multiline

echo ""
echo "===================="
echo -e "Results: ${GREEN}$PASSED passed${NC}, ${RED}$FAILED failed${NC}"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
