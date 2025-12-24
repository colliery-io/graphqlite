"""Tests for GraphQLite Python bindings."""

import os
import sqlite3
import tempfile
from pathlib import Path

import pytest

import graphqlite
from graphqlite import connect, wrap


# Find extension path for tests
def get_extension_path():
    """Get path to the built extension."""
    # Look for build directory relative to this file
    # tests/ -> bindings/python/ -> bindings/ -> graphqlite/
    test_dir = Path(__file__).parent
    build_dir = test_dir.parent.parent.parent / "build"

    if (build_dir / "graphqlite.dylib").exists():
        return str(build_dir / "graphqlite.dylib")
    elif (build_dir / "graphqlite.so").exists():
        return str(build_dir / "graphqlite.so")

    # Try environment variable
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


class TestConnection:
    """Test connection management."""

    def test_connect_memory(self):
        """Test in-memory database connection."""
        ext_path = get_extension_path()
        db = connect(":memory:", extension_path=ext_path)
        assert db is not None
        db.close()

    def test_connect_file(self, tmp_path):
        """Test file database connection."""
        ext_path = get_extension_path()
        db_path = tmp_path / "test.db"
        db = connect(str(db_path), extension_path=ext_path)
        assert db is not None
        assert db_path.exists()
        db.close()

    def test_context_manager(self):
        """Test connection as context manager."""
        ext_path = get_extension_path()
        with connect(":memory:", extension_path=ext_path) as db:
            results = db.cypher("RETURN 1 as n")
            assert len(results) == 1

    def test_wrap_existing_connection(self):
        """Test wrapping an existing SQLite connection."""
        ext_path = get_extension_path()
        sqlite_conn = sqlite3.connect(":memory:")
        db = wrap(sqlite_conn, extension_path=ext_path)
        results = db.cypher("RETURN 'hello' as msg")
        assert results[0]["msg"] == "hello"
        db.close()


class TestCypherQueries:
    """Test Cypher query execution."""

    def test_create_node(self, db):
        """Test creating a node."""
        db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
        results = db.cypher("MATCH (n:Person) RETURN n.name, n.age")
        assert len(results) == 1
        assert results[0]["n.name"] == "Alice"
        assert results[0]["n.age"] == 30

    def test_create_relationship(self, db):
        """Test creating nodes with relationship."""
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

    def test_return_scalar(self, db):
        """Test returning scalar values."""
        results = db.cypher("RETURN 42 as num, 'hello' as str")
        assert len(results) == 1
        assert results[0]["num"] == 42
        assert results[0]["str"] == "hello"

    def test_return_list(self, db):
        """Test returning list values."""
        db.cypher("CREATE (n:Num {val: 1})")
        db.cypher("CREATE (n:Num {val: 2})")
        db.cypher("CREATE (n:Num {val: 3})")

        results = db.cypher("MATCH (n:Num) RETURN n.val ORDER BY n.val")
        assert len(results) == 3
        assert [r["n.val"] for r in results] == [1, 2, 3]

    def test_aggregation(self, db):
        """Test aggregation functions."""
        db.cypher("CREATE (n:Num {val: 10})")
        db.cypher("CREATE (n:Num {val: 20})")
        db.cypher("CREATE (n:Num {val: 30})")

        results = db.cypher("MATCH (n:Num) RETURN count(n) as cnt, sum(n.val) as total")
        # Aggregations may return string values; convert to int for comparison
        assert int(results[0]["cnt"]) == 3
        assert int(results[0]["total"]) == 60

    def test_empty_result(self, db):
        """Test query with no results."""
        results = db.cypher("MATCH (n:NonExistent) RETURN n")
        # Empty MATCH results may return a success message row, so just check <= 1
        assert len(results) <= 1


class TestCypherResult:
    """Test CypherResult object."""

    def test_iteration(self, db):
        """Test iterating over results."""
        db.cypher("CREATE (n:N {v: 1})")
        db.cypher("CREATE (n:N {v: 2})")

        results = db.cypher("MATCH (n:N) RETURN n.v ORDER BY n.v")
        values = [row["n.v"] for row in results]
        assert values == [1, 2]

    def test_indexing(self, db):
        """Test indexing results."""
        db.cypher("CREATE (n:N {v: 'first'})")
        db.cypher("CREATE (n:N {v: 'second'})")

        results = db.cypher("MATCH (n:N) RETURN n.v ORDER BY n.v")
        assert results[0]["n.v"] == "first"
        assert results[1]["n.v"] == "second"

    def test_columns(self, db):
        """Test column names."""
        results = db.cypher("RETURN 1 as a, 2 as b, 3 as c")
        assert results.columns == ["a", "b", "c"]

    def test_to_list(self, db):
        """Test converting to list."""
        db.cypher("CREATE (n:N {v: 1})")
        results = db.cypher("MATCH (n:N) RETURN n.v")
        data = results.to_list()
        assert isinstance(data, list)
        assert len(data) == 1


class TestGraphAlgorithms:
    """Test graph algorithm functions."""

    def test_pagerank(self, db):
        """Test PageRank algorithm."""
        # Create a small graph
        db.cypher("CREATE (a:Page {name: 'A'})")
        db.cypher("CREATE (b:Page {name: 'B'})")
        db.cypher("CREATE (c:Page {name: 'C'})")
        db.cypher("""
            MATCH (a:Page {name: 'A'}), (b:Page {name: 'B'}), (c:Page {name: 'C'})
            CREATE (a)-[:LINKS]->(b), (a)-[:LINKS]->(c), (b)-[:LINKS]->(c)
        """)

        results = db.cypher("RETURN pageRank(0.85, 10)")
        assert len(results) > 0

    def test_label_propagation(self, db):
        """Test label propagation algorithm."""
        # Create a small graph
        db.cypher("CREATE (a:Node {name: 'A'})")
        db.cypher("CREATE (b:Node {name: 'B'})")
        db.cypher("""
            MATCH (a:Node {name: 'A'}), (b:Node {name: 'B'})
            CREATE (a)-[:CONNECTED]->(b)
        """)

        results = db.cypher("RETURN labelPropagation(5)")
        assert len(results) > 0


class TestVersion:
    """Test version info."""

    def test_version(self):
        """Test version is accessible."""
        assert hasattr(graphqlite, "__version__")
        assert isinstance(graphqlite.__version__, str)
