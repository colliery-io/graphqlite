"""Edge operations mixin for Graph class."""

from typing import Any, Optional

from ._base import BaseMixin
from ..utils import sanitize_rel_type


class EdgesMixin(BaseMixin):
    """Mixin providing edge CRUD operations."""

    def has_edge(self, source_id: str, target_id: str, rel_type: Optional[str] = None) -> bool:
        """
        Check if an edge exists between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id
            rel_type: Optional relationship type to check for

        Returns:
            True if edge exists, False otherwise
        """
        rel_pattern = f":{sanitize_rel_type(rel_type)}" if rel_type else ""
        result = self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r{rel_pattern}]->"
            f"(b {{id: '{self._escape(target_id)}'}}) "
            f"RETURN count(r) AS cnt"
        )
        if len(result) == 0:
            return False
        cnt = result[0].get("cnt", 0)
        return int(cnt) > 0 if cnt else False

    def get_edge(self, source_id: str, target_id: str, rel_type: Optional[str] = None) -> Optional[dict]:
        """
        Get edge properties between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id
            rel_type: Optional relationship type to retrieve

        Returns:
            Edge dict or None if not found
        """
        rel_pattern = f":{sanitize_rel_type(rel_type)}" if rel_type else ""
        result = self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r{rel_pattern}]->"
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

        If an edge of the same type already exists, its properties are updated
        (merge semantics -- existing properties not in edge_data are preserved).
        If no edge of that type exists, a new one is created.
        Both source and target nodes must exist.

        Args:
            source_id: Source node id
            target_id: Target node id
            edge_data: Dictionary of edge properties
            rel_type: Relationship type label
        """
        safe_rel_type = sanitize_rel_type(rel_type)
        esc_source = self._escape(source_id)
        esc_target = self._escape(target_id)

        self._conn.cypher(
            f"MATCH (a {{id: '{esc_source}'}}), (b {{id: '{esc_target}'}}) "
            f"MERGE (a)-[r:{safe_rel_type}]->(b)"
        )

        if edge_data:
            set_parts = []
            for k, v in edge_data.items():
                if isinstance(v, str):
                    set_parts.append(f"r.{k} = '{self._escape(v)}'")
                elif isinstance(v, bool):
                    set_parts.append(f"r.{k} = {str(v).lower()}")
                elif v is None:
                    set_parts.append(f"r.{k} = null")
                else:
                    set_parts.append(f"r.{k} = {v}")
            set_str = ", ".join(set_parts)
            self._conn.cypher(
                f"MATCH (a {{id: '{esc_source}'}})-[r:{safe_rel_type}]->"
                f"(b {{id: '{esc_target}'}}) SET {set_str}"
            )

    def delete_edge(self, source_id: str, target_id: str, rel_type: Optional[str] = None) -> None:
        """
        Delete edge between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id
            rel_type: Optional relationship type to delete
        """
        rel_pattern = f":{sanitize_rel_type(rel_type)}" if rel_type else ""
        self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r{rel_pattern}]->"
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
