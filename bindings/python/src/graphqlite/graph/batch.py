"""Batch operations mixin for Graph class."""

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

        Args:
            nodes: List of (node_id, properties, label) tuples
        """
        # TODO: Use transaction for actual batching
        for node_id, props, label in nodes:
            self.upsert_node(node_id, props, label)

    def upsert_edges_batch(
        self,
        edges: list[tuple[str, str, dict[str, Any], str]]
    ) -> None:
        """
        Batch upsert multiple edges.

        Args:
            edges: List of (source_id, target_id, properties, rel_type) tuples
        """
        # TODO: Use transaction for actual batching
        for source_id, target_id, props, rel_type in edges:
            self.upsert_edge(source_id, target_id, props, rel_type)
