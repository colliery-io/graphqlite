"""Bulk insert operations for high-performance graph construction.

These methods bypass Cypher query parsing and use direct SQL for maximum throughput.
They are designed for building graphs from external data sources where you have
full control over node IDs and don't need upsert semantics.

Example:
    >>> from graphqlite import graph
    >>> g = graph(":memory:")
    >>>
    >>> # Bulk insert nodes - returns mapping of external ID -> internal rowid
    >>> id_map = g.insert_nodes_bulk([
    ...     ("alice", {"name": "Alice", "age": 30}, "Person"),
    ...     ("bob", {"name": "Bob", "age": 25}, "Person"),
    ... ])
    >>>
    >>> # Bulk insert edges using the ID map - no MATCH queries needed
    >>> g.insert_edges_bulk([
    ...     ("alice", "bob", {"since": 2020}, "KNOWS"),
    ... ], id_map)
"""

from dataclasses import dataclass
from typing import Any, Optional

from ._base import BaseMixin


@dataclass
class BulkInsertResult:
    """Result of a bulk insert operation."""

    nodes_inserted: int
    """Number of nodes inserted."""

    edges_inserted: int
    """Number of edges inserted."""

    id_map: dict[str, int]
    """Mapping from external node IDs to internal SQLite rowids."""


class BulkMixin(BaseMixin):
    """Mixin providing bulk insert operations."""

    def insert_nodes_bulk(
        self,
        nodes: list[tuple[str, dict[str, Any], str]],
    ) -> dict[str, int]:
        """
        Insert multiple nodes in a single transaction with minimal overhead.

        Returns a map of external_id -> internal_rowid for subsequent edge insertion.
        This bypasses Cypher parsing entirely for maximum performance.

        Args:
            nodes: List of (external_id, properties, label) tuples

        Returns:
            Dictionary mapping external IDs to internal SQLite rowids.

        Example:
            >>> g = graph(":memory:")
            >>> id_map = g.insert_nodes_bulk([
            ...     ("node1", {"name": "Node 1"}, "Label"),
            ...     ("node2", {"name": "Node 2"}, "Label"),
            ... ])
            >>> assert "node1" in id_map
            >>> assert "node2" in id_map
        """
        if not nodes:
            return {}

        conn = self.connection.sqlite_connection
        id_map: dict[str, int] = {}

        # Begin transaction
        conn.execute("BEGIN IMMEDIATE")

        try:
            # Get or create property key for 'id'
            id_key_id = self._ensure_property_key(conn, "id")

            # Property key cache within this transaction
            prop_key_cache: dict[str, int] = {"id": id_key_id}

            for external_id, props, label in nodes:
                # Insert node row
                cursor = conn.execute("INSERT INTO nodes DEFAULT VALUES")
                node_id = cursor.lastrowid

                # Store mapping
                id_map[external_id] = node_id

                # Insert label
                conn.execute(
                    "INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)",
                    (node_id, label),
                )

                # Insert 'id' property (the external ID)
                conn.execute(
                    "INSERT OR REPLACE INTO node_props_text (node_id, key_id, value) VALUES (?, ?, ?)",
                    (node_id, id_key_id, external_id),
                )

                # Insert other properties
                for key, value in props.items():
                    # Get or create property key ID
                    if key in prop_key_cache:
                        key_id = prop_key_cache[key]
                    else:
                        key_id = self._ensure_property_key(conn, key)
                        prop_key_cache[key] = key_id

                    # Determine value type and insert
                    self._insert_property(conn, "node", node_id, key_id, value)

            # Commit transaction
            conn.execute("COMMIT")

        except Exception:
            conn.execute("ROLLBACK")
            raise

        return id_map

    def insert_edges_bulk(
        self,
        edges: list[tuple[str, str, dict[str, Any], str]],
        id_map: Optional[dict[str, int]] = None,
    ) -> int:
        """
        Insert multiple edges using pre-resolved internal IDs.

        Uses the mapping returned from `insert_nodes_bulk` to resolve external IDs
        to internal rowids without any database queries. For nodes not in the map,
        falls back to a database lookup.

        Args:
            edges: List of (source_external_id, target_external_id, properties, rel_type) tuples
            id_map: Optional mapping from external IDs to internal rowids (from `insert_nodes_bulk`)

        Returns:
            Number of edges inserted.

        Example:
            >>> g = graph(":memory:")
            >>> id_map = g.insert_nodes_bulk([
            ...     ("a", {}, "Node"),
            ...     ("b", {}, "Node"),
            ... ])
            >>> edges_inserted = g.insert_edges_bulk([
            ...     ("a", "b", {"weight": 1.0}, "CONNECTS"),
            ... ], id_map)
            >>> assert edges_inserted == 1
        """
        if not edges:
            return 0

        if id_map is None:
            id_map = {}

        conn = self.connection.sqlite_connection

        # Begin transaction
        conn.execute("BEGIN IMMEDIATE")

        try:
            # Property key cache
            prop_key_cache: dict[str, int] = {}

            # Cache for looking up node IDs not in the provided map
            fallback_cache: dict[str, int] = {}

            edges_inserted = 0

            for source, target, props, rel_type in edges:
                # Sanitize relationship type
                safe_rel_type = self._sanitize_rel_type(rel_type)

                # Resolve source ID
                if source in id_map:
                    source_id = id_map[source]
                elif source in fallback_cache:
                    source_id = fallback_cache[source]
                else:
                    source_id = self._lookup_node_id(conn, source)
                    fallback_cache[source] = source_id

                # Resolve target ID
                if target in id_map:
                    target_id = id_map[target]
                elif target in fallback_cache:
                    target_id = fallback_cache[target]
                else:
                    target_id = self._lookup_node_id(conn, target)
                    fallback_cache[target] = target_id

                # Insert edge
                cursor = conn.execute(
                    "INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)",
                    (source_id, target_id, safe_rel_type),
                )
                edge_id = cursor.lastrowid
                edges_inserted += 1

                # Insert edge properties
                for key, value in props.items():
                    # Get or create property key ID
                    if key in prop_key_cache:
                        key_id = prop_key_cache[key]
                    else:
                        key_id = self._ensure_property_key(conn, key)
                        prop_key_cache[key] = key_id

                    # Determine value type and insert
                    self._insert_property(conn, "edge", edge_id, key_id, value)

            # Commit transaction
            conn.execute("COMMIT")

        except Exception:
            conn.execute("ROLLBACK")
            raise

        return edges_inserted

    def insert_graph_bulk(
        self,
        nodes: list[tuple[str, dict[str, Any], str]],
        edges: list[tuple[str, str, dict[str, Any], str]],
    ) -> BulkInsertResult:
        """
        Bulk insert both nodes and edges in a single operation.

        This is a convenience method that combines `insert_nodes_bulk` and `insert_edges_bulk`.

        Args:
            nodes: List of (external_id, properties, label) tuples
            edges: List of (source_external_id, target_external_id, properties, rel_type) tuples

        Returns:
            BulkInsertResult with counts and the ID mapping.

        Example:
            >>> g = graph(":memory:")
            >>> result = g.insert_graph_bulk(
            ...     nodes=[
            ...         ("x", {"name": "X"}, "Node"),
            ...         ("y", {"name": "Y"}, "Node"),
            ...     ],
            ...     edges=[
            ...         ("x", "y", {}, "LINKS"),
            ...     ],
            ... )
            >>> assert result.nodes_inserted == 2
            >>> assert result.edges_inserted == 1
        """
        id_map = self.insert_nodes_bulk(nodes)
        edges_inserted = self.insert_edges_bulk(edges, id_map)

        return BulkInsertResult(
            nodes_inserted=len(id_map),
            edges_inserted=edges_inserted,
            id_map=id_map,
        )

    def resolve_node_ids(
        self,
        external_ids: list[str],
    ) -> dict[str, int]:
        """
        Resolve multiple external node IDs to internal rowids.

        This is useful when you need to insert edges between nodes that were
        inserted in previous sessions or via Cypher.

        Args:
            external_ids: List of external node IDs to resolve

        Returns:
            Dictionary mapping external IDs to internal rowids.
            IDs that don't exist in the database will be missing from the map.

        Example:
            >>> g = graph(":memory:")
            >>> g.cypher("CREATE (:Person {id: 'alice'})")
            >>> g.cypher("CREATE (:Person {id: 'bob'})")
            >>> resolved = g.resolve_node_ids(["alice", "bob", "unknown"])
            >>> assert "alice" in resolved
            >>> assert "bob" in resolved
            >>> assert "unknown" not in resolved
        """
        if not external_ids:
            return {}

        conn = self.connection.sqlite_connection
        result: dict[str, int] = {}

        # Get the 'id' property key
        cursor = conn.execute("SELECT id FROM property_keys WHERE key = 'id'")
        row = cursor.fetchone()
        if row is None:
            return result  # No 'id' property key means no nodes

        id_key_id = row[0]

        # Look up each external ID
        for external_id in external_ids:
            cursor = conn.execute(
                "SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?",
                (id_key_id, external_id),
            )
            row = cursor.fetchone()
            if row:
                result[external_id] = row[0]

        return result

    # Helper methods

    def _ensure_property_key(self, conn, key: str) -> int:
        """Ensure a property key exists and return its ID."""
        # Try to find existing key
        cursor = conn.execute("SELECT id FROM property_keys WHERE key = ?", (key,))
        row = cursor.fetchone()
        if row:
            return row[0]

        # Insert new key
        cursor = conn.execute("INSERT INTO property_keys (key) VALUES (?)", (key,))
        return cursor.lastrowid

    def _lookup_node_id(self, conn, external_id: str) -> int:
        """Look up a node's internal ID by external ID."""
        # Get the 'id' property key
        cursor = conn.execute("SELECT id FROM property_keys WHERE key = 'id'")
        row = cursor.fetchone()
        if row is None:
            raise ValueError(f"Node with id '{external_id}' not found (no 'id' property key)")

        id_key_id = row[0]

        # Look up the node
        cursor = conn.execute(
            "SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?",
            (id_key_id, external_id),
        )
        row = cursor.fetchone()
        if row is None:
            raise ValueError(f"Node with id '{external_id}' not found")

        return row[0]

    def _insert_property(self, conn, entity_type: str, entity_id: int, key_id: int, value: Any) -> None:
        """Insert a property value into the appropriate typed table."""
        table_prefix = "node_props" if entity_type == "node" else "edge_props"
        id_column = "node_id" if entity_type == "node" else "edge_id"

        if isinstance(value, bool):
            conn.execute(
                f"INSERT OR REPLACE INTO {table_prefix}_bool ({id_column}, key_id, value) VALUES (?, ?, ?)",
                (entity_id, key_id, 1 if value else 0),
            )
        elif isinstance(value, int):
            conn.execute(
                f"INSERT OR REPLACE INTO {table_prefix}_int ({id_column}, key_id, value) VALUES (?, ?, ?)",
                (entity_id, key_id, value),
            )
        elif isinstance(value, float):
            conn.execute(
                f"INSERT OR REPLACE INTO {table_prefix}_real ({id_column}, key_id, value) VALUES (?, ?, ?)",
                (entity_id, key_id, value),
            )
        else:
            # Convert to string
            conn.execute(
                f"INSERT OR REPLACE INTO {table_prefix}_text ({id_column}, key_id, value) VALUES (?, ?, ?)",
                (entity_id, key_id, str(value)),
            )

    def _sanitize_rel_type(self, rel_type: str) -> str:
        """Sanitize a relationship type for use in the database."""
        # Replace non-alphanumeric characters with underscores
        safe = "".join(c if c.isalnum() or c == "_" else "_" for c in rel_type)

        # Ensure it doesn't start with a number
        if safe and safe[0].isdigit():
            safe = "REL_" + safe

        # Handle empty string
        if not safe:
            safe = "REL"

        return safe
