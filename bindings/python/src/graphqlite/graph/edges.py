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
            f"MATCH (a {{id: $src}})-[r{rel_pattern}]->"
            f"(b {{id: $tgt}}) "
            f"RETURN count(r) AS cnt",
            params={"src": source_id, "tgt": target_id},
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
            f"MATCH (a {{id: $src}})-[r{rel_pattern}]->"
            f"(b {{id: $tgt}}) RETURN r",
            params={"src": source_id, "tgt": target_id},
        )
        if len(result) == 0:
            return None
        return result[0].get("r")

    def upsert_edge(
        self,
        source_id: str,
        target_id: str,
        edge_data: dict[str, Any],
        rel_type: str = "RELATED",
        edge_id: Optional[str] = None,
    ) -> None:
        """
        Create or update an edge between two nodes.

        Without edge_id: if an edge of the same type already exists, its
        properties are updated (merge semantics -- existing properties not in
        edge_data are preserved). If no edge of that type exists, a new one is
        created.

        With edge_id: the edge is matched/merged on that caller-assigned id
        (stored as an ``id`` property on the relationship) instead of on the
        (source, target, rel_type) triple. Repeated calls with the same
        edge_id update that edge in place; different edge_ids create distinct
        parallel edges between the same two nodes with the same type.

        Both source and target nodes must exist.

        Args:
            source_id: Source node id
            target_id: Target node id
            edge_data: Dictionary of edge properties
            rel_type: Relationship type label
            edge_id: Optional caller-assigned edge identifier
        """
        safe_rel_type = sanitize_rel_type(rel_type)

        if edge_id is None:
            self._conn.cypher(
                f"MATCH (a {{id: $src}}), (b {{id: $tgt}}) "
                f"MERGE (a)-[r:{safe_rel_type}]->(b)",
                params={"src": source_id, "tgt": target_id},
            )
            rel_match = f"[r:{safe_rel_type}]"
            base_params = {"src": source_id, "tgt": target_id}
        else:
            self._conn.cypher(
                f"MATCH (a {{id: $src}}), (b {{id: $tgt}}) "
                f"MERGE (a)-[r:{safe_rel_type} {{id: $eid}}]->(b)",
                params={"src": source_id, "tgt": target_id, "eid": edge_id},
            )
            rel_match = f"[r:{safe_rel_type} {{id: $eid}}]"
            base_params = {"src": source_id, "tgt": target_id, "eid": edge_id}

        if edge_data:
            params = dict(base_params)
            set_parts = []
            for i, (k, v) in enumerate(edge_data.items()):
                param_name = f"v{i}"
                set_parts.append(f"r.{k} = ${param_name}")
                params[param_name] = v
            set_str = ", ".join(set_parts)
            self._conn.cypher(
                f"MATCH (a {{id: $src}})-{rel_match}->"
                f"(b {{id: $tgt}}) SET {set_str}",
                params=params,
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
            f"MATCH (a {{id: $src}})-[r{rel_pattern}]->"
            f"(b {{id: $tgt}}) DELETE r",
            params={"src": source_id, "tgt": target_id},
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
