"""Tests for GraphManager multi-graph functionality."""

import os
import tempfile
import pytest

from graphqlite import graphs, GraphManager


@pytest.fixture
def temp_dir():
    """Create a temporary directory for test graphs."""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield tmpdir


class TestGraphManagerBasic:
    """Basic GraphManager operations."""

    def test_create_manager(self, temp_dir):
        """Test creating a GraphManager."""
        with graphs(temp_dir) as gm:
            assert isinstance(gm, GraphManager)
            assert len(gm) == 0

    def test_list_empty(self, temp_dir):
        """Test listing graphs in empty directory."""
        with graphs(temp_dir) as gm:
            assert gm.list() == []

    def test_create_graph(self, temp_dir):
        """Test creating a new graph."""
        with graphs(temp_dir) as gm:
            g = gm.create("social")
            assert g is not None
            assert gm.exists("social")
            assert "social" in gm.list()
            assert os.path.exists(os.path.join(temp_dir, "social.db"))

    def test_create_duplicate_fails(self, temp_dir):
        """Test that creating duplicate graph raises error."""
        with graphs(temp_dir) as gm:
            gm.create("social")
            with pytest.raises(FileExistsError):
                gm.create("social")

    def test_open_graph(self, temp_dir):
        """Test opening an existing graph."""
        with graphs(temp_dir) as gm:
            gm.create("social")

        with graphs(temp_dir) as gm:
            g = gm.open("social")
            assert g is not None

    def test_open_missing_fails(self, temp_dir):
        """Test that opening missing graph raises error."""
        with graphs(temp_dir) as gm:
            with pytest.raises(FileNotFoundError) as exc_info:
                gm.open("nonexistent")
            assert "nonexistent" in str(exc_info.value)
            assert "Available:" in str(exc_info.value)

    def test_open_or_create_new(self, temp_dir):
        """Test open_or_create creates new graph."""
        with graphs(temp_dir) as gm:
            g = gm.open_or_create("cache")
            assert g is not None
            assert gm.exists("cache")

    def test_open_or_create_existing(self, temp_dir):
        """Test open_or_create opens existing graph."""
        with graphs(temp_dir) as gm:
            gm.create("cache")
            g = gm.open_or_create("cache")
            assert g is not None

    def test_drop_graph(self, temp_dir):
        """Test dropping a graph."""
        with graphs(temp_dir) as gm:
            gm.create("social")
            assert gm.exists("social")
            gm.drop("social")
            assert not gm.exists("social")
            assert not os.path.exists(os.path.join(temp_dir, "social.db"))

    def test_drop_missing_fails(self, temp_dir):
        """Test that dropping missing graph raises error."""
        with graphs(temp_dir) as gm:
            with pytest.raises(FileNotFoundError):
                gm.drop("nonexistent")

    def test_list_multiple(self, temp_dir):
        """Test listing multiple graphs."""
        with graphs(temp_dir) as gm:
            gm.create("alpha")
            gm.create("beta")
            gm.create("gamma")
            result = gm.list()
            assert result == ["alpha", "beta", "gamma"]

    def test_contains(self, temp_dir):
        """Test 'in' operator."""
        with graphs(temp_dir) as gm:
            gm.create("social")
            assert "social" in gm
            assert "missing" not in gm

    def test_len(self, temp_dir):
        """Test len() on manager."""
        with graphs(temp_dir) as gm:
            assert len(gm) == 0
            gm.create("one")
            assert len(gm) == 1
            gm.create("two")
            assert len(gm) == 2

    def test_iter(self, temp_dir):
        """Test iterating over manager."""
        with graphs(temp_dir) as gm:
            gm.create("alpha")
            gm.create("beta")
            names = list(gm)
            assert names == ["alpha", "beta"]


class TestGraphManagerData:
    """Test data operations across graphs."""

    def test_graph_isolation(self, temp_dir):
        """Test that graphs are isolated from each other."""
        with graphs(temp_dir) as gm:
            # Create data in social graph
            social = gm.create("social")
            social.upsert_node("alice", {"name": "Alice"}, "Person")

            # Create data in products graph
            products = gm.create("products")
            products.upsert_node("phone", {"name": "Phone"}, "Product")

            # Verify isolation
            social_nodes = social.query("MATCH (n) RETURN n.name")
            assert len(social_nodes) == 1
            assert social_nodes[0]["n.name"] == "Alice"

            product_nodes = products.query("MATCH (n) RETURN n.name")
            assert len(product_nodes) == 1
            assert product_nodes[0]["n.name"] == "Phone"

    def test_cached_open(self, temp_dir):
        """Test that open returns cached graph."""
        with graphs(temp_dir) as gm:
            gm.create("social")
            g1 = gm.open("social")
            g2 = gm.open("social")
            assert g1 is g2


class TestCrossGraphQueries:
    """Test cross-graph query functionality."""

    def test_query_single_graph(self, temp_dir):
        """Test cross-graph query on single graph."""
        with graphs(temp_dir) as gm:
            social = gm.create("social")
            social.upsert_node("alice", {"name": "Alice", "age": 30}, "Person")
            social.upsert_node("bob", {"name": "Bob", "age": 25}, "Person")

            result = gm.query(
                "MATCH (n:Person) FROM social RETURN n.name ORDER BY n.name",
                graphs=["social"]
            )
            assert len(result) == 2
            assert result[0]["n.name"] == "Alice"
            assert result[1]["n.name"] == "Bob"

    def test_query_with_graph_function(self, temp_dir):
        """Test graph() function in cross-graph query."""
        with graphs(temp_dir) as gm:
            social = gm.create("social")
            social.upsert_node("alice", {"name": "Alice"}, "Person")

            result = gm.query(
                "MATCH (n:Person) FROM social RETURN n.name, graph(n) AS source",
                graphs=["social"]
            )
            assert len(result) == 1
            assert result[0]["n.name"] == "Alice"
            assert result[0]["source"] == "social"

    def test_query_missing_graph_fails(self, temp_dir):
        """Test that querying missing graph raises error."""
        with graphs(temp_dir) as gm:
            with pytest.raises(FileNotFoundError):
                gm.query(
                    "MATCH (n) FROM missing RETURN n",
                    graphs=["missing"]
                )

    def test_query_sql(self, temp_dir):
        """Test raw SQL cross-graph query."""
        with graphs(temp_dir) as gm:
            social = gm.create("social")
            social.upsert_node("alice", {"name": "Alice"}, "Person")

            result = gm.query_sql(
                "SELECT COUNT(*) FROM social.nodes",
                graphs=["social"]
            )
            assert result[0][0] == 1


class TestGraphManagerPersistence:
    """Test persistence across manager instances."""

    def test_data_persists(self, temp_dir):
        """Test that data persists across manager instances."""
        # Create and populate graph
        with graphs(temp_dir) as gm:
            social = gm.create("social")
            social.upsert_node("alice", {"name": "Alice"}, "Person")

        # Reopen and verify
        with graphs(temp_dir) as gm:
            social = gm.open("social")
            result = social.query("MATCH (n:Person) RETURN n.name")
            assert len(result) == 1
            assert result[0]["n.name"] == "Alice"

    def test_graphs_persist(self, temp_dir):
        """Test that graph list persists."""
        with graphs(temp_dir) as gm:
            gm.create("alpha")
            gm.create("beta")

        with graphs(temp_dir) as gm:
            assert gm.list() == ["alpha", "beta"]


class TestGraphManagerHardening:
    """Regression tests for GitHub #111 (path traversal) and #112 (graphs required)."""

    @pytest.mark.parametrize("bad", ["../x", "a/b", "", "a b", "1abc", "x;DROP"])
    def test_manager_rejects_traversal(self, temp_dir, bad):
        with graphs(temp_dir) as gm:
            for op in (gm.create, gm.open, gm.open_or_create, gm.exists):
                with pytest.raises(ValueError):
                    op(bad)
            with pytest.raises(ValueError):
                gm.query("MATCH (n) RETURN n", graphs=[bad])
        parent = os.path.dirname(os.path.abspath(temp_dir))
        assert not os.path.exists(os.path.join(parent, "x.db"))
        assert not os.path.exists(os.path.join(temp_dir, "a", "b.db"))
        assert os.listdir(temp_dir) == []

    def test_manager_drop_rejects_traversal(self, temp_dir):
        parent = os.path.dirname(os.path.abspath(temp_dir))
        victim = os.path.join(parent, "victim_should_survive.db")
        with open(victim, "w") as fh:
            fh.write("x")
        try:
            with graphs(temp_dir) as gm:
                with pytest.raises(ValueError):
                    gm.drop("../victim_should_survive")
            assert os.path.exists(victim)
        finally:
            os.unlink(victim)

    def test_manager_query_requires_graphs(self, temp_dir):
        with graphs(temp_dir) as gm:
            gm.create("social").upsert_node("s1", {"name": "S1"})
            with pytest.raises(ValueError, match="graphs is required"):
                gm.query("MATCH (n) FROM social RETURN n.name", graphs=[])
            with pytest.raises(ValueError, match="graphs is required"):
                gm.query("MATCH (n) FROM social RETURN n.name", None)
            with pytest.raises(TypeError):
                gm.query("MATCH (n) FROM social RETURN n.name")
            with pytest.raises(ValueError, match="graphs is required"):
                gm.query_sql("SELECT 1", [])
            rows = gm.query("MATCH (n) FROM social RETURN n.name", graphs=["social"]).to_list()
            assert rows == [{"n.name": "S1"}]
