"""Query operations mixin for Graph class."""

from ._base import BaseMixin


class QueriesMixin(BaseMixin):
    """Mixin providing graph query operations."""

    def node_degree(self, node_id: str) -> int:
        """
        Get the degree (number of connections) of a node.

        Counts both incoming and outgoing edges.

        Args:
            node_id: The node's id property value

        Returns:
            Number of edges connected to the node
        """
        result = self._conn.cypher(
            f"MATCH (n {{id: '{self._escape(node_id)}'}})-[r]-() "
            f"RETURN count(r) AS degree"
        )
        if len(result) == 0:
            return 0
        deg = result[0].get("degree", 0)
        return int(deg) if deg else 0

    def get_neighbors(self, node_id: str) -> list[dict]:
        """
        Get all neighboring nodes.

        Returns nodes connected by edges in either direction.

        Args:
            node_id: The node's id property value

        Returns:
            List of neighbor node dicts
        """
        result = self._conn.cypher(
            f"MATCH (n {{id: '{self._escape(node_id)}'}})-[]-(m) "
            f"RETURN DISTINCT m"
        )
        return [row.get("m") for row in result if row.get("m")]

    def get_node_edges(self, node_id: str) -> list[tuple[str, str, dict]]:
        """
        Get all edges connected to a node.

        Args:
            node_id: The node's id property value

        Returns:
            List of (source_id, target_id, properties) tuples
        """
        result = self._conn.cypher(
            f"MATCH (n {{id: '{self._escape(node_id)}'}})-[r]-(m) "
            f"RETURN n.id AS source, m.id AS target, r"
        )
        return [(row["source"], row["target"], row.get("r", {})) for row in result]

    def stats(self) -> dict[str, int]:
        """
        Get graph statistics.

        Returns:
            Dict with 'nodes' and 'edges' counts
        """
        nodes = self._conn.cypher("MATCH (n) RETURN count(n) AS cnt")
        edges = self._conn.cypher("MATCH ()-[r]->() RETURN count(r) AS cnt")

        node_cnt = nodes[0].get("cnt", 0) if len(nodes) > 0 else 0
        edge_cnt = edges[0].get("cnt", 0) if len(edges) > 0 else 0

        return {
            "nodes": int(node_cnt) if node_cnt else 0,
            "edges": int(edge_cnt) if edge_cnt else 0,
        }

    def query(self, cypher: str) -> list[dict]:
        """
        Execute a raw Cypher query.

        Args:
            cypher: Cypher query string

        Returns:
            List of result dictionaries
        """
        result = self._conn.cypher(cypher)
        return result.to_list()
