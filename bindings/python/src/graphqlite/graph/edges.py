"""Edge operations mixin for Graph class."""

from typing import Any, Optional

from ._base import BaseMixin
from ..utils import sanitize_rel_type


class EdgesMixin(BaseMixin):
    """Mixin providing edge CRUD operations."""

    def has_edge(self, source_id: str, target_id: str) -> bool:
        """
        Check if an edge exists between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id

        Returns:
            True if edge exists, False otherwise
        """
        result = self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r]->"
            f"(b {{id: '{self._escape(target_id)}'}}) "
            f"RETURN count(r) AS cnt"
        )
        if len(result) == 0:
            return False
        cnt = result[0].get("cnt", 0)
        return int(cnt) > 0 if cnt else False

    def get_edge(self, source_id: str, target_id: str) -> Optional[dict]:
        """
        Get edge properties between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id

        Returns:
            Edge dict or None if not found
        """
        result = self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r]->"
            f"(b {{id: '{self._escape(target_id)}'}}) RETURN r"
        )
        if len(result) == 0:
            return None
        return result[0].get("r")

    def upsert_edge(
        self,
        source_id: str,
        target_id: str,
        edge_data: dict[str, Any],
        rel_type: str = "RELATED"
    ) -> None:
        """
        Create or update an edge between two nodes.

        If an edge already exists, this is a no-op.
        Both source and target nodes must exist.

        Args:
            source_id: Source node id
            target_id: Target node id
            edge_data: Dictionary of edge properties
            rel_type: Relationship type label
        """
        safe_rel_type = sanitize_rel_type(rel_type)

        if self.has_edge(source_id, target_id):
            return

        esc_source = self._escape(source_id)
        esc_target = self._escape(target_id)

        if edge_data:
            prop_str = self._format_props(edge_data)
            query = (
                f"MATCH (a {{id: '{esc_source}'}}), (b {{id: '{esc_target}'}}) "
                f"CREATE (a)-[r:{safe_rel_type} {{{prop_str}}}]->(b)"
            )
        else:
            query = (
                f"MATCH (a {{id: '{esc_source}'}}), (b {{id: '{esc_target}'}}) "
                f"CREATE (a)-[r:{safe_rel_type}]->(b)"
            )
        self._conn.cypher(query)

    def delete_edge(self, source_id: str, target_id: str) -> None:
        """
        Delete edge between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id
        """
        self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r]->"
            f"(b {{id: '{self._escape(target_id)}'}}) DELETE r"
        )

    def get_all_edges(self) -> list[dict]:
        """
        Get all edges with source and target info.

        Returns:
            List of dicts with 'source', 'target', and edge properties
        """
        result = self._conn.cypher(
            "MATCH (a)-[r]->(b) RETURN a.id AS source, b.id AS target, r"
        )
        return result.to_list()
