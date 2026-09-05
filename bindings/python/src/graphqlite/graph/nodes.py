"""Node operations mixin for Graph class."""

import json
from typing import Any, Optional

from ._base import BaseMixin
from ..utils import assert_identifier


class NodesMixin(BaseMixin):
    """Mixin providing node CRUD operations."""

    def has_node(self, node_id: str) -> bool:
        """
        Check if a node exists.

        Args:
            node_id: The node's id property value

        Returns:
            True if node exists, False otherwise
        """
        result = self._conn.cypher(
            "MATCH (n {id: $id}) RETURN count(n) AS cnt",
            params={"id": node_id},
        )
        if len(result) == 0:
            return False
        cnt = result[0].get("cnt", 0)
        return int(cnt) > 0 if cnt else False

    def get_node(self, node_id: str) -> Optional[dict]:
        """
        Get a node by ID.

        Args:
            node_id: The node's id property value

        Returns:
            Node dict with 'id', 'labels', 'properties' or None if not found
        """
        result = self._conn.cypher(
            "MATCH (n {id: $id}) RETURN n",
            params={"id": node_id},
        )
        if len(result) == 0:
            return None
        return result[0].get("n")

    def upsert_node(
        self,
        node_id: str,
        node_data: dict[str, Any],
        label: str = "Entity"
    ) -> None:
        """
        Create or update a node.

        If a node with the given id exists, its properties are updated.
        Otherwise, a new node is created.

        Args:
            node_id: Unique identifier for the node (stored as 'id' property).
                An ``"id"`` key inside ``node_data`` is ignored; ``node_id``
                always determines identity.
            node_data: Dictionary of properties to set. Keys must be valid
                identifiers (``^[A-Za-z_][A-Za-z0-9_]*$``).
            label: Node label (only used on creation); must be a valid
                identifier.

        Raises:
            ValueError: If ``label`` or any property key is not a valid
                identifier. Nothing is written in that case.
        """
        assert_identifier(label, "label")
        # node_id is the identity; never let a caller-supplied "id" replace
        # it on create or rename the node on update (GitHub #109).
        data = {k: v for k, v in node_data.items() if k != "id"}
        for k in data:
            assert_identifier(k, "property key")

        if self.has_node(node_id):
            # Update existing node
            for k, v in data.items():
                self._conn.cypher(
                    f"MATCH (n {{id: $id}}) "
                    f"SET n.{k} = $val RETURN n",
                    params={"id": node_id, "val": v},
                )
        else:
            # Create new node
            prop_str = self._format_props({"id": node_id, **data})
            self._conn.cypher(f"CREATE (n:{label} {{{prop_str}}})")

    def delete_node(self, node_id: str) -> None:
        """
        Delete a node and its relationships.

        Args:
            node_id: The node's id property value
        """
        self._conn.cypher(
            "MATCH (n {id: $id}) DETACH DELETE n",
            params={"id": node_id},
        )

    def get_all_nodes(self, label: Optional[str] = None) -> list[dict]:
        """
        Get all nodes, optionally filtered by label.

        Args:
            label: Optional label to filter by

        Returns:
            List of node dicts
        """
        if label:
            assert_identifier(label, "label")
            result = self._conn.cypher(f"MATCH (n:{label}) RETURN n")
        else:
            result = self._conn.cypher("MATCH (n) RETURN n")

        nodes = []
        for row in result:
            if "result" in row and isinstance(row["result"], str):
                try:
                    parsed = json.loads(row["result"])
                    for item in parsed:
                        if "n" in item:
                            nodes.append(item["n"])
                except json.JSONDecodeError:
                    pass
            elif "n" in row and row["n"]:
                nodes.append(row["n"])
        return nodes
