"""Graph class package.

This package provides the main Graph class composed from focused mixins:
- NodesMixin: Node CRUD operations
- EdgesMixin: Edge CRUD operations
- QueriesMixin: Graph query operations
- BatchMixin: Batch operations

The Graph class also incorporates algorithm mixins from the algorithms package.
"""

from pathlib import Path
from typing import Any, Optional, Union

from .._platform import find_extension
from ..connection import connect
from ..algorithms import (
    CentralityMixin,
    CommunityMixin,
    ComponentsMixin,
    ExportMixin,
    PathsMixin,
    SimilarityMixin,
    TraversalMixin,
)
from ._base import BaseMixin
from .batch import BatchMixin
from .bulk import BulkInsertResult, BulkMixin
from .edges import EdgesMixin
from .nodes import NodesMixin
from .queries import QueriesMixin


class Graph(
    NodesMixin,
    EdgesMixin,
    QueriesMixin,
    BatchMixin,
    BulkMixin,
    CentralityMixin,
    CommunityMixin,
    ComponentsMixin,
    PathsMixin,
    TraversalMixin,
    SimilarityMixin,
    ExportMixin,
):
    """
    High-level graph interface for GraphQLite.

    Provides an intuitive API for working with graphs, including:
    - Node and edge CRUD operations
    - Graph algorithms (PageRank, community detection, shortest paths, etc.)
    - Query operations

    Example:
        >>> from graphqlite import graph
        >>> g = graph(":memory:")
        >>> g.upsert_node("alice", {"name": "Alice", "age": 30}, "Person")
        >>> g.upsert_node("bob", {"name": "Bob", "age": 25}, "Person")
        >>> g.upsert_edge("alice", "bob", {"since": 2020}, "KNOWS")
        >>> g.pagerank()
    """

    def __init__(
        self,
        db_path: Union[str, Path] = ":memory:",
        namespace: str = "default",
        extension_path: Optional[str] = None
    ):
        """
        Initialize a Graph instance.

        Args:
            db_path: Path to database file or ":memory:" for in-memory
            namespace: Optional namespace for isolating graphs
            extension_path: Path to graphqlite extension (auto-detected if None)
        """
        ext_path = find_extension(extension_path)
        self._conn = connect(str(db_path), ext_path)
        self.namespace = namespace

    @property
    def connection(self):
        """Return the underlying Connection object."""
        return self._conn

    def close(self) -> None:
        """Close the database connection."""
        self._conn.close()

    # Cache management methods for algorithm acceleration
    def load_graph(self) -> dict:
        """
        Load the graph into an in-memory CSR cache for fast algorithm execution.

        When the cache is loaded, graph algorithms run ~28x faster by avoiding
        repeated SQLite I/O. The cache persists until explicitly unloaded or
        the connection is closed.

        Returns:
            dict with 'status', 'nodes', and 'edges' keys

        Example:
            >>> g = graph(":memory:")
            >>> g.upsert_node("alice", {}, "Person")
            >>> g.upsert_node("bob", {}, "Person")
            >>> g.upsert_edge("alice", "bob", {}, "KNOWS")
            >>> g.load_graph()
            {'status': 'loaded', 'nodes': 2, 'edges': 1}
            >>> g.pagerank()  # Now runs ~28x faster
        """
        import json
        cursor = self._conn.execute("SELECT gql_load_graph()")
        row = cursor.fetchone()
        return json.loads(row[0]) if row else {}

    def unload_graph(self) -> dict:
        """
        Free the cached graph from memory.

        Call this after algorithm execution to reclaim memory, or when the
        graph has been modified and you want to invalidate the cache.

        Returns:
            dict with 'status' key

        Example:
            >>> g.load_graph()
            >>> g.pagerank()
            >>> g.unload_graph()
            {'status': 'unloaded'}
        """
        import json
        cursor = self._conn.execute("SELECT gql_unload_graph()")
        row = cursor.fetchone()
        return json.loads(row[0]) if row else {}

    def reload_graph(self) -> dict:
        """
        Reload the graph cache with the latest data.

        Use this after modifying the graph (adding/removing nodes/edges)
        to refresh the cache with the current state.

        Returns:
            dict with 'status', 'nodes', and 'edges' keys

        Example:
            >>> g.load_graph()
            >>> g.upsert_node("charlie", {}, "Person")  # Graph modified
            >>> g.reload_graph()  # Refresh cache with new node
            {'status': 'reloaded', 'nodes': 3, 'edges': 1}
        """
        import json
        cursor = self._conn.execute("SELECT gql_reload_graph()")
        row = cursor.fetchone()
        return json.loads(row[0]) if row else {}

    def graph_loaded(self) -> bool:
        """
        Check if the graph cache is currently loaded.

        Returns:
            True if cached, False otherwise

        Example:
            >>> g.graph_loaded()
            False
            >>> g.load_graph()
            >>> g.graph_loaded()
            True
        """
        import json
        cursor = self._conn.execute("SELECT gql_graph_loaded()")
        row = cursor.fetchone()
        result = json.loads(row[0]) if row else {}
        return result.get("loaded", False)

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - close connection."""
        self.close()
        return False


def graph(
    db_path: Union[str, Path] = ":memory:",
    namespace: str = "default",
    extension_path: Optional[str] = None
) -> Graph:
    """
    Create a new Graph instance.

    Factory function matching the style of graphqlite.connect().

    Args:
        db_path: Path to database file or ":memory:" for in-memory
        namespace: Optional namespace for isolating graphs
        extension_path: Path to graphqlite extension (auto-detected if None)

    Returns:
        Graph instance

    Example:
        >>> g = graphqlite.graph(":memory:")
        >>> g.upsert_node("n1", {"name": "Test"})
    """
    return Graph(db_path, namespace, extension_path)


__all__ = ["BulkInsertResult", "Graph", "graph"]
