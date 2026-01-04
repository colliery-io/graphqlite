"""GraphManager for managing multiple graph databases."""

from __future__ import annotations

import os
import sqlite3
from pathlib import Path
from typing import Any, Dict, Iterator, List, Optional, Union

from .connection import Connection, CypherResult
from .graph import Graph
from ._platform import find_extension


class GraphManager:
    """
    Manager for multiple graph databases in a directory.

    Provides ergonomic multi-graph management using separate SQLite files
    per graph, with cross-graph query support via ATTACH.

    Example:
        >>> from graphqlite import graphs
        >>> with graphs("./data") as gm:
        ...     gm.create("social")
        ...     gm.create("products")
        ...     social = gm.open("social")
        ...     social.upsert_node("alice", {"name": "Alice"}, "Person")
        ...     gm.list()  # ['products', 'social']
    """

    def __init__(
        self,
        base_path: Union[str, Path],
        extension_path: Optional[str] = None
    ):
        """
        Initialize GraphManager.

        Args:
            base_path: Directory where graph .db files are stored
            extension_path: Path to graphqlite extension (auto-detected if None)
        """
        self._base_path = Path(base_path)
        self._extension_path = find_extension(extension_path)
        self._open_graphs: dict[str, Graph] = {}
        self._coordinator: Optional[sqlite3.Connection] = None

        # Ensure base directory exists
        self._base_path.mkdir(parents=True, exist_ok=True)

    def _graph_path(self, name: str) -> Path:
        """Get the file path for a graph."""
        return self._base_path / f"{name}.db"

    def _ensure_coordinator(self) -> sqlite3.Connection:
        """Get or create the coordinator connection for cross-graph queries."""
        if self._coordinator is None:
            self._coordinator = sqlite3.connect(":memory:")
            self._coordinator.enable_load_extension(True)
            # Load extension (remove file extension for SQLite)
            ext_path = Path(self._extension_path)
            load_path = str(ext_path.parent / ext_path.stem)
            self._coordinator.load_extension(load_path)
            self._coordinator.enable_load_extension(False)
        return self._coordinator

    def list(self) -> list[str]:
        """
        List all available graphs in the base directory.

        Returns:
            List of graph names (without .db extension)

        Example:
            >>> gm.list()
            ['products', 'social', 'users']
        """
        graphs = []
        for path in self._base_path.glob("*.db"):
            graphs.append(path.stem)
        return sorted(graphs)

    def exists(self, name: str) -> bool:
        """
        Check if a graph exists.

        Args:
            name: Graph name

        Returns:
            True if the graph file exists
        """
        return self._graph_path(name).exists()

    def create(self, name: str) -> Graph:
        """
        Create a new graph.

        Args:
            name: Graph name (will create {name}.db file)

        Returns:
            Graph instance for the new graph

        Raises:
            FileExistsError: If graph already exists

        Example:
            >>> g = gm.create("analytics")
            >>> g.upsert_node("n1", {"value": 100}, "Metric")
        """
        path = self._graph_path(name)
        if path.exists():
            raise FileExistsError(f"Graph '{name}' already exists at {path}")

        graph = Graph(str(path), extension_path=self._extension_path)
        self._open_graphs[name] = graph
        return graph

    def open(self, name: str) -> Graph:
        """
        Open an existing graph.

        Args:
            name: Graph name

        Returns:
            Graph instance

        Raises:
            FileNotFoundError: If graph doesn't exist

        Example:
            >>> social = gm.open("social")
            >>> results = social.query("MATCH (n:Person) RETURN n.name")
        """
        # Return cached graph if already open
        if name in self._open_graphs:
            return self._open_graphs[name]

        path = self._graph_path(name)
        if not path.exists():
            available = self.list()
            raise FileNotFoundError(
                f"Graph '{name}' not found. Available: {available}"
            )

        graph = Graph(str(path), extension_path=self._extension_path)
        self._open_graphs[name] = graph
        return graph

    def open_or_create(self, name: str) -> Graph:
        """
        Open a graph, creating it if it doesn't exist.

        Args:
            name: Graph name

        Returns:
            Graph instance

        Example:
            >>> cache = gm.open_or_create("cache")
        """
        if self.exists(name):
            return self.open(name)
        return self.create(name)

    def drop(self, name: str) -> None:
        """
        Delete a graph and its database file.

        Args:
            name: Graph name

        Raises:
            FileNotFoundError: If graph doesn't exist

        Example:
            >>> gm.drop("old_graph")
        """
        path = self._graph_path(name)
        if not path.exists():
            available = self.list()
            raise FileNotFoundError(
                f"Graph '{name}' not found. Available: {available}"
            )

        # Close if open
        if name in self._open_graphs:
            self._open_graphs[name].close()
            del self._open_graphs[name]

        # Detach from coordinator if attached
        if self._coordinator is not None:
            try:
                self._coordinator.execute(f"DETACH DATABASE {name}")
            except sqlite3.OperationalError:
                pass  # Not attached

        # Delete file
        path.unlink()

    def query(
        self,
        cypher: str,
        graphs: Optional[list[str]] = None,
        params: Optional[dict[str, Any]] = None
    ) -> CypherResult:
        """
        Execute a cross-graph Cypher query.

        Uses the FROM clause syntax to query across multiple graphs.
        Graphs are automatically attached to the coordinator connection.

        Note: Open graph connections are committed before the query runs
        to ensure their changes are visible to the coordinator.

        Args:
            cypher: Cypher query with FROM clauses specifying graphs
            graphs: List of graph names to attach (auto-detected from query if None)
            params: Optional query parameters

        Returns:
            CypherResult with query results

        Example:
            >>> # Query across social and products graphs
            >>> result = gm.query('''
            ...     MATCH (u:User) FROM social
            ...     WHERE u.user_id = "alice"
            ...     RETURN u.name, graph(u) AS source
            ... ''', graphs=["social"])
        """
        # Commit any open graph connections so their data is visible
        for graph in self._open_graphs.values():
            graph.connection.commit()

        coord = self._ensure_coordinator()

        # Attach requested graphs
        if graphs:
            for name in graphs:
                path = self._graph_path(name)
                if not path.exists():
                    available = self.list()
                    raise FileNotFoundError(
                        f"Graph '{name}' not found. Available: {available}"
                    )
                try:
                    coord.execute(f"ATTACH DATABASE '{path}' AS {name}")
                except sqlite3.OperationalError as e:
                    if "already in use" not in str(e).lower():
                        raise

        # Execute query
        import json
        if params:
            params_json = json.dumps(params)
            cursor = coord.execute("SELECT cypher(?, ?)", (cypher, params_json))
        else:
            cursor = coord.execute("SELECT cypher(?)", (cypher,))

        row = cursor.fetchone()

        if row is None or row[0] is None:
            return CypherResult([], [])

        result_str = row[0]

        # Parse JSON result
        try:
            data = json.loads(result_str)
        except json.JSONDecodeError:
            if result_str.startswith("Error"):
                raise sqlite3.Error(result_str)
            return CypherResult([{"result": result_str}], ["result"])

        if isinstance(data, list):
            if len(data) == 0:
                return CypherResult([], [])
            if isinstance(data[0], dict):
                columns = list(data[0].keys()) if data else []
                return CypherResult(data, columns)
            return CypherResult([{"result": result_str}], ["result"])
        elif isinstance(data, dict):
            return CypherResult([data], list(data.keys()))
        else:
            return CypherResult([{"result": data}], ["result"])

    def query_sql(
        self,
        sql: str,
        graphs: list[str],
        parameters: tuple = ()
    ) -> list[tuple]:
        """
        Execute a raw SQL query across attached graphs.

        For power users who need direct SQL access to cross-graph data.

        Args:
            sql: SQL query with graph-prefixed table names
            graphs: List of graph names to attach
            parameters: Query parameters

        Returns:
            List of result tuples

        Example:
            >>> gm.query_sql('''
            ...     SELECT s.value, p.value
            ...     FROM social.node_props_text s
            ...     JOIN products.node_props_text p ON s.value = p.value
            ... ''', graphs=["social", "products"])
        """
        coord = self._ensure_coordinator()

        # Attach requested graphs
        for name in graphs:
            path = self._graph_path(name)
            if not path.exists():
                available = self.list()
                raise FileNotFoundError(
                    f"Graph '{name}' not found. Available: {available}"
                )
            try:
                coord.execute(f"ATTACH DATABASE '{path}' AS {name}")
            except sqlite3.OperationalError as e:
                if "already in use" not in str(e).lower():
                    raise

        cursor = coord.execute(sql, parameters)
        return cursor.fetchall()

    def close(self) -> None:
        """Close all open graph connections and the coordinator."""
        for graph in self._open_graphs.values():
            graph.close()
        self._open_graphs.clear()

        if self._coordinator is not None:
            self._coordinator.close()
            self._coordinator = None

    def __enter__(self) -> "GraphManager":
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit - close all connections."""
        self.close()

    def __iter__(self) -> Iterator[str]:
        """Iterate over graph names."""
        return iter(self.list())

    def __contains__(self, name: str) -> bool:
        """Check if a graph exists."""
        return self.exists(name)

    def __len__(self) -> int:
        """Return number of graphs."""
        return len(self.list())


def graphs(
    base_path: Union[str, Path],
    extension_path: Optional[str] = None
) -> GraphManager:
    """
    Create a GraphManager for managing multiple graphs.

    Factory function for creating a GraphManager instance.

    Args:
        base_path: Directory where graph .db files are stored
        extension_path: Path to graphqlite extension (auto-detected if None)

    Returns:
        GraphManager instance

    Example:
        >>> from graphqlite import graphs
        >>> with graphs("./data") as gm:
        ...     gm.create("social")
        ...     social = gm.open("social")
        ...     social.upsert_node("alice", {"name": "Alice"}, "Person")
    """
    return GraphManager(base_path, extension_path)


__all__ = ["GraphManager", "graphs"]
