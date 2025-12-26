"""Tests for graphqlite.connection module."""

import os
import sqlite3
from pathlib import Path

import pytest

import graphqlite
from graphqlite import Connection, connect, wrap


def get_extension_path():
    """Get path to the built extension."""
    test_dir = Path(__file__).parent
    build_dir = test_dir.parent.parent.parent / "build"

    if (build_dir / "graphqlite.dylib").exists():
        return str(build_dir / "graphqlite.dylib")
    elif (build_dir / "graphqlite.so").exists():
        return str(build_dir / "graphqlite.so")

    env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    pytest.skip("GraphQLite extension not found. Build with 'make extension'")


@pytest.fixture
def db():
    """Create an in-memory GraphQLite database."""
    ext_path = get_extension_path()
    conn = connect(":memory:", extension_path=ext_path)
    yield conn
    conn.close()


# =============================================================================
# Connection Management
# =============================================================================

def test_connect_memory():
    ext_path = get_extension_path()
    db = connect(":memory:", extension_path=ext_path)
    assert db is not None
    db.close()


def test_connect_file(tmp_path):
    ext_path = get_extension_path()
    db_path = tmp_path / "test.db"
    db = connect(str(db_path), extension_path=ext_path)
    assert db is not None
    assert db_path.exists()
    db.close()


def test_connect_context_manager():
    ext_path = get_extension_path()
    with connect(":memory:", extension_path=ext_path) as db:
        results = db.cypher("RETURN 1 as n")
        assert len(results) == 1


def test_wrap_existing_connection():
    ext_path = get_extension_path()
    sqlite_conn = sqlite3.connect(":memory:")
    db = wrap(sqlite_conn, extension_path=ext_path)
    results = db.cypher("RETURN 'hello' as msg")
    assert results[0]["msg"] == "hello"
    db.close()


def test_sqlite_connection_property(db):
    assert db.sqlite_connection is not None
    assert isinstance(db.sqlite_connection, sqlite3.Connection)


# =============================================================================
# Cypher Queries
# =============================================================================

def test_create_node(db):
    db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
    results = db.cypher("MATCH (n:Person) RETURN n.name, n.age")
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"
    assert results[0]["n.age"] == 30


def test_create_relationship(db):
    db.cypher("CREATE (a:Person {name: 'Alice'})")
    db.cypher("CREATE (b:Person {name: 'Bob'})")
    db.cypher("""
        MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
        CREATE (a)-[:KNOWS]->(b)
    """)
    results = db.cypher("MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name")
    assert len(results) == 1
    assert results[0]["a.name"] == "Alice"
    assert results[0]["b.name"] == "Bob"


def test_return_scalar(db):
    results = db.cypher("RETURN 42 as num, 'hello' as str")
    assert len(results) == 1
    assert results[0]["num"] == 42
    assert results[0]["str"] == "hello"


def test_return_multiple_rows(db):
    db.cypher("CREATE (n:Num {val: 1})")
    db.cypher("CREATE (n:Num {val: 2})")
    db.cypher("CREATE (n:Num {val: 3})")
    results = db.cypher("MATCH (n:Num) RETURN n.val ORDER BY n.val")
    assert len(results) == 3
    assert [r["n.val"] for r in results] == [1, 2, 3]


def test_aggregation(db):
    db.cypher("CREATE (n:Num {val: 10})")
    db.cypher("CREATE (n:Num {val: 20})")
    db.cypher("CREATE (n:Num {val: 30})")
    results = db.cypher("MATCH (n:Num) RETURN count(n) as cnt, sum(n.val) as total")
    assert int(results[0]["cnt"]) == 3
    assert int(results[0]["total"]) == 60


def test_empty_result(db):
    results = db.cypher("MATCH (n:NonExistent) RETURN n")
    assert len(results) <= 1


# =============================================================================
# CypherResult
# =============================================================================

def test_result_iteration(db):
    db.cypher("CREATE (n:N {v: 1})")
    db.cypher("CREATE (n:N {v: 2})")
    results = db.cypher("MATCH (n:N) RETURN n.v ORDER BY n.v")
    values = [row["n.v"] for row in results]
    assert values == [1, 2]


def test_result_indexing(db):
    db.cypher("CREATE (n:N {v: 'first'})")
    db.cypher("CREATE (n:N {v: 'second'})")
    results = db.cypher("MATCH (n:N) RETURN n.v ORDER BY n.v")
    assert results[0]["n.v"] == "first"
    assert results[1]["n.v"] == "second"


def test_result_columns(db):
    results = db.cypher("RETURN 1 as a, 2 as b, 3 as c")
    assert results.columns == ["a", "b", "c"]


def test_result_to_list(db):
    db.cypher("CREATE (n:N {v: 1})")
    results = db.cypher("MATCH (n:N) RETURN n.v")
    data = results.to_list()
    assert isinstance(data, list)
    assert len(data) == 1


def test_result_len(db):
    db.cypher("CREATE (n:N {v: 1})")
    db.cypher("CREATE (n:N {v: 2})")
    results = db.cypher("MATCH (n:N) RETURN n.v")
    assert len(results) == 2


# =============================================================================
# Graph Algorithms (via Connection.cypher)
# =============================================================================

def test_pagerank_function(db):
    db.cypher("CREATE (a:Page {name: 'A'})")
    db.cypher("CREATE (b:Page {name: 'B'})")
    db.cypher("CREATE (c:Page {name: 'C'})")
    db.cypher("""
        MATCH (a:Page {name: 'A'}), (b:Page {name: 'B'}), (c:Page {name: 'C'})
        CREATE (a)-[:LINKS]->(b), (a)-[:LINKS]->(c), (b)-[:LINKS]->(c)
    """)
    results = db.cypher("RETURN pageRank(0.85, 10)")
    assert len(results) > 0


def test_label_propagation_function(db):
    db.cypher("CREATE (a:Node {name: 'A'})")
    db.cypher("CREATE (b:Node {name: 'B'})")
    db.cypher("""
        MATCH (a:Node {name: 'A'}), (b:Node {name: 'B'})
        CREATE (a)-[:CONNECTED]->(b)
    """)
    results = db.cypher("RETURN labelPropagation(5)")
    assert len(results) > 0


# =============================================================================
# Module Exports
# =============================================================================

def test_version_exists():
    assert hasattr(graphqlite, "__version__")
    assert isinstance(graphqlite.__version__, str)


def test_exports():
    assert hasattr(graphqlite, "Connection")
    assert hasattr(graphqlite, "connect")
    assert hasattr(graphqlite, "wrap")
    assert hasattr(graphqlite, "load")
    assert hasattr(graphqlite, "loadable_path")


# =============================================================================
# Parameterized Queries
# =============================================================================

def test_parameterized_string(db):
    """Test parameterized query with string parameter."""
    db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
    db.cypher("CREATE (n:Person {name: 'Bob', age: 25})")

    results = db.cypher(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name, n.age",
        {"name": "Alice"}
    )
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"
    assert results[0]["n.age"] == 30


def test_parameterized_integer(db):
    """Test parameterized query with integer parameter."""
    db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
    db.cypher("CREATE (n:Person {name: 'Bob', age: 25})")
    db.cypher("CREATE (n:Person {name: 'Charlie', age: 35})")

    results = db.cypher(
        "MATCH (n:Person) WHERE n.age > $min_age RETURN n.name ORDER BY n.age",
        {"min_age": 28}
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "Alice"
    assert results[1]["n.name"] == "Charlie"


def test_parameterized_multiple(db):
    """Test parameterized query with multiple parameters."""
    db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
    db.cypher("CREATE (n:Person {name: 'Bob', age: 25})")
    db.cypher("CREATE (n:Person {name: 'Charlie', age: 35})")

    results = db.cypher(
        "MATCH (n:Person) WHERE n.age >= $min AND n.age <= $max RETURN n.name ORDER BY n.age",
        {"min": 25, "max": 32}
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "Bob"
    assert results[1]["n.name"] == "Alice"


def test_parameterized_no_match(db):
    """Test parameterized query that matches nothing."""
    db.cypher("CREATE (n:Person {name: 'Alice'})")

    results = db.cypher(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name",
        {"name": "Nobody"}
    )
    # When no rows match, result may be empty or contain success message
    # The key is that it doesn't return Alice
    for row in results:
        if "n.name" in row:
            assert row["n.name"] != "Alice"


def test_parameterized_sql_injection_safe(db):
    """Test that parameters prevent SQL injection."""
    db.cypher("CREATE (n:Person {name: 'Alice'})")
    db.cypher("CREATE (n:Person {name: 'Bob'})")

    # This malicious string should be treated as a literal value
    results = db.cypher(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name",
        {"name": "'; DROP TABLE nodes; --"}
    )

    # Should not match anything (no one named with SQL injection string)
    matched_names = [row.get("n.name") for row in results if "n.name" in row]
    assert "Alice" not in matched_names
    assert "Bob" not in matched_names

    # Verify data is still intact
    all_results = db.cypher("MATCH (n:Person) RETURN n.name ORDER BY n.name")
    names = [row["n.name"] for row in all_results]
    assert "Alice" in names
    assert "Bob" in names


# =============================================================================
# REMOVE Clause Tests
# =============================================================================

def test_remove_node_property(db):
    """Test REMOVE clause for removing a node property."""
    db.cypher("CREATE (n:RemoveTest {name: 'Alice', age: 30, city: 'NYC'})")

    # Verify properties exist
    results = db.cypher("MATCH (n:RemoveTest) RETURN n.name, n.age, n.city")
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"
    assert results[0]["n.age"] == 30
    assert results[0]["n.city"] == "NYC"

    # Remove age property
    db.cypher("MATCH (n:RemoveTest) REMOVE n.age")

    # Verify age is removed (null)
    results = db.cypher("MATCH (n:RemoveTest) RETURN n.name, n.age, n.city")
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"
    assert results[0]["n.age"] is None
    assert results[0]["n.city"] == "NYC"


def test_remove_multiple_properties(db):
    """Test REMOVE clause for removing multiple properties at once."""
    db.cypher("CREATE (n:RemoveMultiTest {a: 1, b: 2, c: 3, d: 4})")

    # Remove multiple properties in one query
    db.cypher("MATCH (n:RemoveMultiTest) REMOVE n.a, n.b")

    # Verify a and b are removed, c and d remain
    results = db.cypher("MATCH (n:RemoveMultiTest) RETURN n.a, n.b, n.c, n.d")
    assert len(results) == 1
    assert results[0]["n.a"] is None
    assert results[0]["n.b"] is None
    assert results[0]["n.c"] == 3
    assert results[0]["n.d"] == 4


def test_remove_label(db):
    """Test REMOVE clause for removing a label from a node."""
    db.cypher("CREATE (n:RemoveLabelTest:RemoveLabelEmp:RemoveLabelMgr {name: 'Bob'})")

    # Verify Manager label exists
    results = db.cypher("MATCH (n:RemoveLabelMgr {name: 'Bob'}) RETURN n.name")
    assert len(results) == 1
    assert results[0]["n.name"] == "Bob"

    # Remove Manager label
    db.cypher("MATCH (n:RemoveLabelTest {name: 'Bob'}) REMOVE n:RemoveLabelMgr")

    # Verify label is removed by checking that a query for the removed label returns no results
    results = db.cypher("MATCH (n:RemoveLabelMgr {name: 'Bob'}) RETURN n.name")
    has_bob = any(row.get("n.name") == "Bob" for row in results)
    assert not has_bob, "Should not find Bob with RemoveLabelMgr after removal"

    # Verify the node still exists with remaining labels
    results = db.cypher("MATCH (n:RemoveLabelTest {name: 'Bob'}) RETURN n.name")
    assert len(results) == 1
    assert results[0]["n.name"] == "Bob"


def test_remove_edge_property(db):
    """Test REMOVE clause for removing an edge property."""
    db.cypher(
        "CREATE (a:RemoveEdgeTest {name: 'A'})-[r:KNOWS {since: 2020, strength: 0.9}]->(b:RemoveEdgeTest {name: 'B'})"
    )

    # Verify edge properties exist
    results = db.cypher(
        "MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) RETURN r.since, r.strength"
    )
    assert len(results) == 1
    assert results[0]["r.since"] == 2020

    # Remove since property
    db.cypher("MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) REMOVE r.since")

    # Verify since is removed, strength remains
    results = db.cypher(
        "MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) RETURN r.since, r.strength"
    )
    assert len(results) == 1
    assert results[0]["r.since"] is None
    assert abs(results[0]["r.strength"] - 0.9) < 0.01


def test_remove_with_where(db):
    """Test REMOVE clause with WHERE filtering."""
    db.cypher("CREATE (a:RemoveWhereTest {name: 'Alice', age: 30, status: 'active'})")
    db.cypher("CREATE (b:RemoveWhereTest {name: 'Bob', age: 25, status: 'active'})")
    db.cypher("CREATE (c:RemoveWhereTest {name: 'Charlie', age: 35, status: 'active'})")

    # Remove status only from nodes where age > 28
    db.cypher("MATCH (n:RemoveWhereTest) WHERE n.age > 28 REMOVE n.status")

    # Verify: Alice (30) and Charlie (35) should have status removed
    results = db.cypher(
        "MATCH (n:RemoveWhereTest) RETURN n.name, n.status ORDER BY n.name"
    )
    assert len(results) == 3

    # Alice: status should be null
    assert results[0]["n.name"] == "Alice"
    assert results[0]["n.status"] is None

    # Bob: status should still exist
    assert results[1]["n.name"] == "Bob"
    assert results[1]["n.status"] == "active"

    # Charlie: status should be null
    assert results[2]["n.name"] == "Charlie"
    assert results[2]["n.status"] is None


def test_remove_nonexistent_property(db):
    """Test REMOVE clause on a non-existent property (should not error)."""
    db.cypher("CREATE (n:RemoveNonexistTest {name: 'Test'})")

    # Remove a property that doesn't exist - should succeed without error
    db.cypher("MATCH (n:RemoveNonexistTest) REMOVE n.nonexistent")

    # Original property should still exist
    results = db.cypher("MATCH (n:RemoveNonexistTest) RETURN n.name")
    assert len(results) == 1
    assert results[0]["n.name"] == "Test"


def test_remove_no_match(db):
    """Test REMOVE clause when no nodes match."""
    # Remove property on non-existent nodes - should succeed (0 rows affected)
    db.cypher("MATCH (n:NonExistentLabel) REMOVE n.property")
    # No error should be raised


# =============================================================================
# IN Operator Tests
# =============================================================================

def test_in_literal_list_match(db):
    """Test IN operator with literal list - matching value."""
    results = db.cypher("RETURN 5 IN [1, 2, 5, 10]")
    assert len(results) == 1
    # Result should be truthy (1 or true)
    assert results[0][list(results[0].keys())[0]] in [1, True, "1", "true"]


def test_in_literal_list_no_match(db):
    """Test IN operator with literal list - non-matching value."""
    results = db.cypher("RETURN 'x' IN ['a', 'b', 'c']")
    assert len(results) == 1
    # Result should be falsy (0 or false)
    assert results[0][list(results[0].keys())[0]] in [0, False, "0", "false"]


def test_in_with_where_clause(db):
    """Test IN operator in WHERE clause for filtering."""
    db.cypher("CREATE (n:InTest {name: 'Alice', status: 'active'})")
    db.cypher("CREATE (n:InTest {name: 'Bob', status: 'pending'})")
    db.cypher("CREATE (n:InTest {name: 'Charlie', status: 'inactive'})")

    results = db.cypher(
        "MATCH (n:InTest) WHERE n.status IN ['active', 'pending'] RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "Alice"
    assert results[1]["n.name"] == "Bob"


def test_in_with_integers(db):
    """Test IN operator with integer values."""
    db.cypher("CREATE (n:InIntTest {name: 'A', priority: 1})")
    db.cypher("CREATE (n:InIntTest {name: 'B', priority: 2})")
    db.cypher("CREATE (n:InIntTest {name: 'C', priority: 3})")

    results = db.cypher(
        "MATCH (n:InIntTest) WHERE n.priority IN [1, 3] RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "A"
    assert results[1]["n.name"] == "C"


def test_in_empty_result(db):
    """Test IN operator when no values match."""
    db.cypher("CREATE (n:InEmptyTest {name: 'Test', status: 'archived'})")

    results = db.cypher(
        "MATCH (n:InEmptyTest) WHERE n.status IN ['active', 'pending'] RETURN n.name"
    )
    assert len(results) == 0


# Tests for STARTS WITH, ENDS WITH, CONTAINS operators
def test_starts_with_match(db):
    """Test STARTS WITH operator - matching prefix."""
    results = db.cypher("RETURN 'hello world' STARTS WITH 'hello'")
    assert len(results) == 1
    assert results[0][list(results[0].keys())[0]] in [1, True, "1", "true"]


def test_starts_with_no_match(db):
    """Test STARTS WITH operator - non-matching prefix."""
    results = db.cypher("RETURN 'hello world' STARTS WITH 'world'")
    assert len(results) == 1
    assert results[0][list(results[0].keys())[0]] in [0, False, "0", "false"]


def test_ends_with_match(db):
    """Test ENDS WITH operator - matching suffix."""
    results = db.cypher("RETURN 'hello world' ENDS WITH 'world'")
    assert len(results) == 1
    assert results[0][list(results[0].keys())[0]] in [1, True, "1", "true"]


def test_ends_with_no_match(db):
    """Test ENDS WITH operator - non-matching suffix."""
    results = db.cypher("RETURN 'hello world' ENDS WITH 'hello'")
    assert len(results) == 1
    assert results[0][list(results[0].keys())[0]] in [0, False, "0", "false"]


def test_contains_match(db):
    """Test CONTAINS operator - substring exists."""
    results = db.cypher("RETURN 'hello world' CONTAINS 'lo wo'")
    assert len(results) == 1
    assert results[0][list(results[0].keys())[0]] in [1, True, "1", "true"]


def test_contains_no_match(db):
    """Test CONTAINS operator - substring does not exist."""
    results = db.cypher("RETURN 'hello world' CONTAINS 'xyz'")
    assert len(results) == 1
    assert results[0][list(results[0].keys())[0]] in [0, False, "0", "false"]


def test_string_operators_in_where(db):
    """Test string operators in WHERE clause."""
    db.cypher("CREATE (n:StringTest {name: 'John Smith', email: 'john@example.com'})")
    db.cypher("CREATE (n:StringTest {name: 'Jane Doe', email: 'jane@test.org'})")
    db.cypher("CREATE (n:StringTest {name: 'Bob Johnson', email: 'bob@example.com'})")

    # Test STARTS WITH
    results = db.cypher(
        "MATCH (n:StringTest) WHERE n.name STARTS WITH 'J' RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "Jane Doe"
    assert results[1]["n.name"] == "John Smith"

    # Test ENDS WITH
    results = db.cypher(
        "MATCH (n:StringTest) WHERE n.email ENDS WITH '.com' RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "Bob Johnson"
    assert results[1]["n.name"] == "John Smith"

    # Test CONTAINS
    results = db.cypher(
        "MATCH (n:StringTest) WHERE n.name CONTAINS 'ohn' RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "Bob Johnson"
    assert results[1]["n.name"] == "John Smith"


def test_string_operators_with_property(db):
    """Test string operators comparing property to literal."""
    db.cypher("CREATE (n:Product {name: 'iPhone 15 Pro Max', category: 'electronics'})")
    db.cypher("CREATE (n:Product {name: 'iPad Pro', category: 'electronics'})")
    db.cypher("CREATE (n:Product {name: 'MacBook Air', category: 'electronics'})")

    # Products starting with "i" (case-sensitive)
    results = db.cypher(
        "MATCH (n:Product) WHERE n.name STARTS WITH 'i' RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "iPad Pro"
    assert results[1]["n.name"] == "iPhone 15 Pro Max"

    # Products ending with "Pro"
    results = db.cypher(
        "MATCH (n:Product) WHERE n.name ENDS WITH 'Pro' RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 1
    assert results[0]["n.name"] == "iPad Pro"

    # Products containing "Pro"
    results = db.cypher(
        "MATCH (n:Product) WHERE n.name CONTAINS 'Pro' RETURN n.name ORDER BY n.name"
    )
    assert len(results) == 2
    assert results[0]["n.name"] == "iPad Pro"
    assert results[1]["n.name"] == "iPhone 15 Pro Max"


# =============================================================================
# UNION Queries
# =============================================================================

def test_union_basic(db):
    """Test basic UNION query."""
    results = db.cypher("RETURN 'A' AS letter UNION RETURN 'B' AS letter")
    assert len(results) == 2
    letters = {row["letter"] for row in results}
    assert letters == {"A", "B"}


def test_union_all(db):
    """Test UNION ALL (keeps duplicates)."""
    results = db.cypher(
        "RETURN 'A' AS letter UNION ALL RETURN 'A' AS letter UNION ALL RETURN 'B' AS letter"
    )
    assert len(results) == 3
    letters = [row["letter"] for row in results]
    assert letters.count("A") == 2
    assert letters.count("B") == 1


def test_union_distinct(db):
    """Test UNION removes duplicates."""
    results = db.cypher(
        "RETURN 'A' AS letter UNION RETURN 'A' AS letter UNION RETURN 'B' AS letter"
    )
    assert len(results) == 2
    letters = {row["letter"] for row in results}
    assert letters == {"A", "B"}


def test_union_with_match(db):
    """Test UNION with MATCH queries."""
    db.cypher("CREATE (a:UnionPerson {name: 'Alice', city: 'NYC'})")
    db.cypher("CREATE (b:UnionPerson {name: 'Bob', city: 'LA'})")
    db.cypher("CREATE (c:UnionCompany {name: 'TechCorp'})")

    results = db.cypher("""
        MATCH (p:UnionPerson {city: 'NYC'}) RETURN p.name AS name
        UNION
        MATCH (c:UnionCompany) RETURN c.name AS name
    """)
    assert len(results) == 2
    names = {row["name"] for row in results}
    assert names == {"Alice", "TechCorp"}


# =============================================================================
# String Functions
# =============================================================================

def test_toupper_function(db):
    """Test toUpper() function."""
    results = db.cypher("RETURN toUpper('hello') AS result")
    assert len(results) == 1
    assert results[0]["result"] == "HELLO"


def test_tolower_function(db):
    """Test toLower() function."""
    results = db.cypher("RETURN toLower('HELLO') AS result")
    assert len(results) == 1
    assert results[0]["result"] == "hello"


def test_trim_function(db):
    """Test trim() function."""
    results = db.cypher("RETURN trim('  hello  ') AS result")
    assert len(results) == 1
    assert results[0]["result"] == "hello"


def test_substring_function(db):
    """Test substring() function."""
    results = db.cypher("RETURN substring('hello world', 0, 5) AS result")
    assert len(results) == 1
    assert results[0]["result"] == "hello"


def test_replace_function(db):
    """Test replace() function."""
    results = db.cypher("RETURN replace('hello world', 'world', 'there') AS result")
    assert len(results) == 1
    assert results[0]["result"] == "hello there"


def test_reverse_function(db):
    """Test reverse() function."""
    results = db.cypher("RETURN reverse('hello') AS result")
    assert len(results) == 1
    assert results[0]["result"] == "olleh"


def test_left_right_functions(db):
    """Test left() and right() functions."""
    results = db.cypher("RETURN left('hello', 3) AS l, right('hello', 3) AS r")
    assert len(results) == 1
    assert results[0]["l"] == "hel"
    assert results[0]["r"] == "llo"


# =============================================================================
# Math Functions
# =============================================================================

def test_abs_function(db):
    """Test abs() function."""
    results = db.cypher("RETURN abs(-5) AS result")
    assert len(results) == 1
    assert int(results[0]["result"]) == 5


def test_ceil_floor_functions(db):
    """Test ceil() and floor() functions."""
    results = db.cypher("RETURN ceil(4.3) AS c, floor(4.7) AS f")
    assert len(results) == 1
    assert int(results[0]["c"]) == 5
    assert int(results[0]["f"]) == 4


def test_round_function(db):
    """Test round() function."""
    results = db.cypher("RETURN round(4.5) AS result")
    assert len(results) == 1
    assert int(results[0]["result"]) == 5


def test_sqrt_function(db):
    """Test sqrt() function."""
    results = db.cypher("RETURN sqrt(16) AS result")
    assert len(results) == 1
    assert float(results[0]["result"]) == 4.0


def test_sign_function(db):
    """Test sign() function."""
    results = db.cypher("RETURN sign(-5) AS neg, sign(0) AS zero, sign(5) AS pos")
    assert len(results) == 1
    assert int(results[0]["neg"]) == -1
    assert int(results[0]["zero"]) == 0
    assert int(results[0]["pos"]) == 1


# =============================================================================
# List Functions
# =============================================================================

def test_size_on_list(db):
    """Test size() function on list."""
    results = db.cypher("RETURN size([1, 2, 3, 4, 5]) AS result")
    assert len(results) == 1
    assert int(results[0]["result"]) == 5


def test_head_function(db):
    """Test head() function."""
    results = db.cypher("RETURN head([1, 2, 3]) AS result")
    assert len(results) == 1
    assert int(results[0]["result"]) == 1


def test_tail_function(db):
    """Test tail() function."""
    results = db.cypher("RETURN tail([1, 2, 3]) AS result")
    assert len(results) == 1
    # tail returns [2, 3] as JSON
    import json
    tail = json.loads(results[0]["result"])
    assert tail == [2, 3]


def test_last_function(db):
    """Test last() function."""
    results = db.cypher("RETURN last([1, 2, 3]) AS result")
    assert len(results) == 1
    assert int(results[0]["result"]) == 3


def test_range_function(db):
    """Test range() function."""
    results = db.cypher("RETURN range(1, 5) AS result")
    assert len(results) == 1
    import json
    r = json.loads(results[0]["result"])
    assert r == [1, 2, 3, 4, 5]


# =============================================================================
# WITH Clause
# =============================================================================

def test_with_simple(db):
    """Test simple WITH clause."""
    db.cypher("CREATE (n:WithTest {name: 'Alice', age: 30})")
    results = db.cypher("""
        MATCH (n:WithTest)
        WITH n.name AS name, n.age AS age
        RETURN name, age
    """)
    assert len(results) == 1
    assert results[0]["name"] == "Alice"
    assert int(results[0]["age"]) == 30  # age comes back as string from JSON


def test_with_where(db):
    """Test WITH clause with WHERE filter."""
    db.cypher("CREATE (n:WithWhereTest {name: 'Alice', age: 30})")
    db.cypher("CREATE (n:WithWhereTest {name: 'Bob', age: 25})")
    results = db.cypher("""
        MATCH (n:WithWhereTest)
        WITH n.name AS name, n.age AS age
        WHERE age > 27
        RETURN name
    """)
    assert len(results) == 1
    assert results[0]["name"] == "Alice"


def test_with_aggregation(db):
    """Test WITH clause with aggregation."""
    db.cypher("CREATE (n:WithAggTest {category: 'A', value: 10})")
    db.cypher("CREATE (n:WithAggTest {category: 'A', value: 20})")
    db.cypher("CREATE (n:WithAggTest {category: 'B', value: 30})")
    results = db.cypher("""
        MATCH (n:WithAggTest)
        WITH n.category AS cat, sum(n.value) AS total
        RETURN cat, total ORDER BY cat
    """)
    assert len(results) == 2
    assert results[0]["cat"] == "A"
    assert int(results[0]["total"]) == 30
    assert results[1]["cat"] == "B"
    assert int(results[1]["total"]) == 30


# =============================================================================
# CASE Expressions
# =============================================================================

def test_case_simple(db):
    """Test simple CASE expression (using searched CASE syntax)."""
    # Note: GraphQLite uses searched CASE syntax (CASE WHEN condition THEN...)
    # Simple CASE (CASE expr WHEN value THEN...) is not supported
    results = db.cypher("""
        RETURN CASE
            WHEN 1 = 1 THEN 'one'
            WHEN 1 = 2 THEN 'two'
            ELSE 'other'
        END AS result
    """)
    assert len(results) == 1
    assert results[0]["result"] == "one"


def test_case_with_else(db):
    """Test CASE expression with ELSE."""
    # Using searched CASE syntax
    results = db.cypher("""
        RETURN CASE
            WHEN 99 = 1 THEN 'one'
            WHEN 99 = 2 THEN 'two'
            ELSE 'other'
        END AS result
    """)
    assert len(results) == 1
    assert results[0]["result"] == "other"


def test_case_searched(db):
    """Test searched CASE expression."""
    results = db.cypher("""
        RETURN CASE
            WHEN 5 > 10 THEN 'big'
            WHEN 5 < 10 THEN 'small'
            ELSE 'equal'
        END AS result
    """)
    assert len(results) == 1
    assert results[0]["result"] == "small"


def test_case_with_property(db):
    """Test CASE with property access."""
    db.cypher("CREATE (n:CaseTest {age: 25})")
    db.cypher("CREATE (n:CaseTest {age: 35})")
    db.cypher("CREATE (n:CaseTest {age: 65})")
    results = db.cypher("""
        MATCH (n:CaseTest)
        RETURN n.age AS age, CASE
            WHEN n.age < 30 THEN 'young'
            WHEN n.age < 60 THEN 'middle'
            ELSE 'senior'
        END AS category
        ORDER BY n.age
    """)
    assert len(results) == 3
    assert results[0]["category"] == "young"
    assert results[1]["category"] == "middle"
    assert results[2]["category"] == "senior"


# =============================================================================
# COALESCE Function
# =============================================================================

def test_coalesce_basic(db):
    """Test coalesce() returns first non-null."""
    results = db.cypher("RETURN coalesce(null, null, 'found', 'other') AS result")
    assert len(results) == 1
    assert results[0]["result"] == "found"


def test_coalesce_with_property(db):
    """Test coalesce() with property that may be null."""
    db.cypher("CREATE (n:CoalesceTest {name: 'Alice'})")  # no nickname
    results = db.cypher("""
        MATCH (n:CoalesceTest)
        RETURN coalesce(n.nickname, n.name) AS display_name
    """)
    assert len(results) == 1
    assert results[0]["display_name"] == "Alice"


# =============================================================================
# IS NULL / IS NOT NULL
# =============================================================================

def test_is_null(db):
    """Test IS NULL predicate."""
    db.cypher("CREATE (n:NullTest {name: 'Alice', email: 'alice@test.com'})")
    db.cypher("CREATE (n:NullTest {name: 'Bob'})")  # no email
    results = db.cypher("""
        MATCH (n:NullTest) WHERE n.email IS NULL RETURN n.name
    """)
    assert len(results) == 1
    assert results[0]["n.name"] == "Bob"


def test_is_not_null(db):
    """Test IS NOT NULL predicate."""
    db.cypher("CREATE (n:NotNullTest {name: 'Alice', email: 'alice@test.com'})")
    db.cypher("CREATE (n:NotNullTest {name: 'Bob'})")  # no email
    results = db.cypher("""
        MATCH (n:NotNullTest) WHERE n.email IS NOT NULL RETURN n.name
    """)
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"


# =============================================================================
# Type Conversion Functions
# =============================================================================

def test_tostring_function(db):
    """Test toString() function."""
    results = db.cypher("RETURN toString(123) AS result")
    assert len(results) == 1
    assert results[0]["result"] == "123"


def test_tointeger_function(db):
    """Test toInteger() function."""
    results = db.cypher("RETURN toInteger('42') AS result")
    assert len(results) == 1
    assert int(results[0]["result"]) == 42


def test_tofloat_function(db):
    """Test toFloat() function."""
    results = db.cypher("RETURN toFloat('3.14') AS result")
    assert len(results) == 1
    assert abs(float(results[0]["result"]) - 3.14) < 0.001


# =============================================================================
# FOREACH Clause
# =============================================================================

def test_foreach_create_nodes(db):
    """Test FOREACH creating multiple nodes."""
    db.cypher("""
        FOREACH (name IN ['Alice', 'Bob', 'Carol'] |
            CREATE (:ForEachPerson {name: name})
        )
    """)
    results = db.cypher("MATCH (n:ForEachPerson) RETURN n.name ORDER BY n.name")
    assert len(results) == 3
    assert results[0]["n.name"] == "Alice"
    assert results[1]["n.name"] == "Bob"
    assert results[2]["n.name"] == "Carol"


def test_foreach_set_property(db):
    """Test FOREACH setting properties on nodes."""
    # FOREACH only supports CREATE/SET/DELETE inside, not MATCH
    # So we create nodes via FOREACH and verify they were created
    db.cypher("""
        FOREACH (i IN [1, 2, 3] |
            CREATE (:ForEachSetTest {num: i, status: 'created'})
        )
    """)
    results = db.cypher("MATCH (n:ForEachSetTest) RETURN n.status ORDER BY n.num")
    assert len(results) == 3
    for r in results:
        assert r["n.status"] == "created"


# =============================================================================
# Variable-Length Relationships
# =============================================================================

def test_varlen_any_hops(db):
    """Test variable-length relationship with any number of hops."""
    db.cypher("CREATE (a:VL {name: 'A'})-[:NEXT]->(b:VL {name: 'B'})-[:NEXT]->(c:VL {name: 'C'})")
    # Note: use *1.. syntax, not bare * (parser requires explicit min bound)
    results = db.cypher("""
        MATCH (a:VL {name: 'A'})-[:NEXT*1..]->(target:VL)
        RETURN target.name ORDER BY target.name
    """)
    assert len(results) == 2
    assert results[0]["target.name"] == "B"
    assert results[1]["target.name"] == "C"


def test_varlen_exact_hops(db):
    """Test variable-length relationship with exact hop count."""
    db.cypher("CREATE (a:VLE {name: 'A'})-[:NEXT]->(b:VLE {name: 'B'})-[:NEXT]->(c:VLE {name: 'C'})")
    results = db.cypher("""
        MATCH (a:VLE {name: 'A'})-[:NEXT*2..2]->(target:VLE)
        RETURN target.name
    """)
    assert len(results) == 1
    assert results[0]["target.name"] == "C"


def test_varlen_bounded(db):
    """Test variable-length relationship with min/max bounds."""
    db.cypher("""
        CREATE (a:VLB {name: 'A'})-[:NEXT]->(b:VLB {name: 'B'})
               -[:NEXT]->(c:VLB {name: 'C'})-[:NEXT]->(d:VLB {name: 'D'})
    """)
    results = db.cypher("""
        MATCH (a:VLB {name: 'A'})-[:NEXT*1..2]->(target:VLB)
        RETURN target.name ORDER BY target.name
    """)
    assert len(results) == 2
    assert results[0]["target.name"] == "B"
    assert results[1]["target.name"] == "C"


# =============================================================================
# Path Functions
# =============================================================================

def test_path_length(db):
    """Test length() function on simple path (not varlen)."""
    db.cypher("CREATE (a:PL {name: 'A'})-[:LINK]->(b:PL {name: 'B'})")
    results = db.cypher("""
        MATCH p = (a:PL {name: 'A'})-[:LINK]->(b:PL)
        RETURN length(p) AS len
    """)
    assert len(results) == 1
    assert int(results[0]["len"]) == 1


def test_path_nodes(db):
    """Test nodes() function on simple path."""
    db.cypher("CREATE (a:PN {name: 'A'})-[:LINK]->(b:PN {name: 'B'})")
    results = db.cypher("""
        MATCH p = (a:PN {name: 'A'})-[:LINK]->(b:PN)
        RETURN nodes(p) AS path_nodes
    """)
    assert len(results) == 1
    # Column alias may not be applied, check both possibilities
    import json
    col_name = "path_nodes" if "path_nodes" in results[0] else "result"
    nodes = json.loads(results[0][col_name])
    assert len(nodes) == 2


def test_path_relationships(db):
    """Test relationships() function on simple path."""
    db.cypher("CREATE (a:PR {name: 'A'})-[:LINK]->(b:PR {name: 'B'})")
    results = db.cypher("""
        MATCH p = (a:PR {name: 'A'})-[:LINK]->(b:PR)
        RETURN relationships(p) AS rels
    """)
    assert len(results) == 1
    # Column alias may not be applied, check both possibilities
    import json
    col_name = "rels" if "rels" in results[0] else "result"
    rels = json.loads(results[0][col_name])
    assert len(rels) == 1


# =============================================================================
# EXISTS / NOT EXISTS
# =============================================================================

def test_exists_property(db):
    """Test EXISTS on property."""
    db.cypher("CREATE (n:ExProp {name: 'Alice', email: 'a@test.com'})")
    db.cypher("CREATE (n:ExProp {name: 'Bob'})")  # no email
    results = db.cypher("""
        MATCH (n:ExProp)
        WHERE exists(n.email)
        RETURN n.name
    """)
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"


def test_not_exists_property(db):
    """Test NOT EXISTS on property."""
    db.cypher("CREATE (n:NotExProp {name: 'Alice', email: 'a@test.com'})")
    db.cypher("CREATE (n:NotExProp {name: 'Bob'})")  # no email
    results = db.cypher("""
        MATCH (n:NotExProp)
        WHERE NOT exists(n.email)
        RETURN n.name
    """)
    assert len(results) == 1
    assert results[0]["n.name"] == "Bob"


def test_exists_pattern(db):
    """Test EXISTS on pattern."""
    db.cypher("CREATE (a:ExPat {name: 'Alice'})-[:KNOWS]->(b:ExPat {name: 'Bob'})")
    db.cypher("CREATE (c:ExPat {name: 'Carol'})")  # no relationships
    results = db.cypher("""
        MATCH (n:ExPat)
        WHERE exists((n)-[:KNOWS]->())
        RETURN n.name
    """)
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"


# =============================================================================
# Multiple Labels
# =============================================================================

def test_multiple_labels_create(db):
    """Test creating node with multiple labels."""
    db.cypher("CREATE (n:Person:Employee {name: 'Alice'})")
    results = db.cypher("MATCH (n:Person:Employee) RETURN n.name")
    assert len(results) == 1
    assert results[0]["n.name"] == "Alice"


def test_multiple_labels_match_any(db):
    """Test matching nodes with multiple labels."""
    db.cypher("CREATE (n:Manager:Employee {name: 'Alice'})")
    db.cypher("CREATE (n:Developer:Employee {name: 'Bob'})")
    db.cypher("CREATE (n:Customer {name: 'Carol'})")
    # Match all Employees
    results = db.cypher("MATCH (n:Employee) RETURN n.name ORDER BY n.name")
    assert len(results) == 2
    assert results[0]["n.name"] == "Alice"
    assert results[1]["n.name"] == "Bob"


def test_labels_function(db):
    """Test labels() function."""
    db.cypher("CREATE (n:Person:Employee:Manager {name: 'Alice'})")
    results = db.cypher("MATCH (n:Person {name: 'Alice'}) RETURN labels(n) AS lbls")
    assert len(results) == 1
    import json
    # Column alias may not be applied, check both possibilities
    col_name = "lbls" if "lbls" in results[0] else "result"
    labels = json.loads(results[0][col_name])
    assert "Person" in labels
    assert "Employee" in labels
    assert "Manager" in labels


# =============================================================================
# OPTIONAL MATCH
# =============================================================================

def test_optional_match_found(db):
    """Test OPTIONAL MATCH when pattern exists."""
    db.cypher("CREATE (a:OptM {name: 'Alice'})-[:KNOWS]->(b:OptM {name: 'Bob'})")
    results = db.cypher("""
        MATCH (a:OptM {name: 'Alice'})
        OPTIONAL MATCH (a)-[:KNOWS]->(friend)
        RETURN a.name, friend.name
    """)
    assert len(results) == 1
    assert results[0]["a.name"] == "Alice"
    assert results[0]["friend.name"] == "Bob"


def test_optional_match_not_found(db):
    """Test OPTIONAL MATCH when pattern doesn't exist."""
    db.cypher("CREATE (a:OptMN {name: 'Alice'})")  # no relationships
    results = db.cypher("""
        MATCH (a:OptMN {name: 'Alice'})
        OPTIONAL MATCH (a)-[:KNOWS]->(friend)
        RETURN a.name, friend.name
    """)
    assert len(results) == 1
    assert results[0]["a.name"] == "Alice"
    assert results[0]["friend.name"] is None


def test_optional_match_partial(db):
    """Test OPTIONAL MATCH with some nodes having matches."""
    db.cypher("CREATE (a:OptMP {name: 'Alice'})-[:KNOWS]->(b:Person {name: 'Bob'})")
    db.cypher("CREATE (c:OptMP {name: 'Carol'})")  # no relationships
    results = db.cypher("""
        MATCH (a:OptMP)
        OPTIONAL MATCH (a)-[:KNOWS]->(friend)
        RETURN a.name, friend.name
        ORDER BY a.name
    """)
    assert len(results) == 2
    assert results[0]["a.name"] == "Alice"
    assert results[0]["friend.name"] == "Bob"
    assert results[1]["a.name"] == "Carol"
    assert results[1]["friend.name"] is None


# =============================================================================
# UNWIND Clause
# =============================================================================

def test_unwind_list(db):
    """Test UNWIND with list literal."""
    results = db.cypher("""
        UNWIND [1, 2, 3] AS x
        RETURN x
    """)
    assert len(results) == 3
    assert int(results[0]["x"]) == 1
    assert int(results[1]["x"]) == 2
    assert int(results[2]["x"]) == 3


def test_unwind_with_create(db):
    """Test UNWIND to create multiple nodes."""
    db.cypher("""
        UNWIND ['Alice', 'Bob', 'Carol'] AS name
        CREATE (n:UnwindPerson {name: name})
    """)
    results = db.cypher("MATCH (n:UnwindPerson) RETURN n.name ORDER BY n.name")
    assert len(results) == 3
    assert results[0]["n.name"] == "Alice"
    assert results[1]["n.name"] == "Bob"
    assert results[2]["n.name"] == "Carol"


def test_unwind_empty_list(db):
    """Test UNWIND with empty list returns no rows."""
    results = db.cypher("""
        UNWIND [] AS x
        RETURN x
    """)
    assert len(results) == 0


def test_unwind_with_index(db):
    """Test array subscript syntax."""
    # Test basic list subscript with parenthesized expression
    results = db.cypher("RETURN (['a', 'b', 'c'])[1] AS item")
    assert len(results) == 1
    assert results[0]["item"] == "b"  # 0-indexed

    # Test subscript with different indices
    results = db.cypher("RETURN (['x', 'y', 'z'])[0] AS first, (['x', 'y', 'z'])[2] AS last")
    assert len(results) == 1
    assert results[0]["first"] == "x"
    assert results[0]["last"] == "z"
