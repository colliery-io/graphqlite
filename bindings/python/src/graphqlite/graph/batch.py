"""Batch operations mixin for Graph class.

These methods provide convenient batch upsert operations using Cypher MERGE semantics.
For high-performance atomic batch inserts, use the bulk insert methods instead.
"""

from typing import Any

from ._base import BaseMixin


class BatchMixin(BaseMixin):
    """Mixin providing batch operations."""

    def upsert_nodes_batch(
        self,
        nodes: list[tuple[str, dict[str, Any], str]]
    ) -> None:
        """
        Batch upsert multiple nodes.

        Convenience method that calls upsert_node for each item.
        Uses Cypher MERGE semantics (update if exists, create if not).

        Note:
            This method does NOT provide atomicity - if an operation fails
            partway through, earlier operations will have already completed.
            For atomic batch inserts, use `insert_nodes_bulk` instead.

        Args:
            nodes: List of (node_id, properties, label) tuples
        """
        for node_id, props, label in nodes:
            self.upsert_node(node_id, props, label)

    def upsert_edges_batch(
        self,
        edges: list[tuple[str, str, dict[str, Any], str]]
    ) -> None:
        """
        Batch upsert multiple edges.

        Convenience method that calls upsert_edge for each item.
        Uses Cypher MERGE semantics (update if exists, create if not).

        Note:
            This method does NOT provide atomicity - if an operation fails
            partway through, earlier operations will have already completed.
            For atomic batch inserts, use `insert_edges_bulk` instead.

        Args:
            edges: List of (source_id, target_id, properties, rel_type) tuples
        """
        for source_id, target_id, props, rel_type in edges:
            self.upsert_edge(source_id, target_id, props, rel_type)
