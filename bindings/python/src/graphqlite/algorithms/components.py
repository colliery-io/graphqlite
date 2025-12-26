"""Connected components algorithms mixin."""

from ..graph._base import BaseMixin
from ._parsing import safe_int


class ComponentsMixin(BaseMixin):
    """Mixin providing connected components algorithm methods."""

    def weakly_connected_components(self) -> list[dict]:
        """
        Find weakly connected components in the graph.

        Treats the graph as undirected and finds connected components.
        Uses Union-Find algorithm for O(V + E * α(V)) complexity.

        Returns:
            List of dicts with 'node_id', 'user_id', 'component'
            where nodes in the same component share the same component number
        """
        result = self._conn.cypher("RETURN wcc()")

        components = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            component = row.get("component")

            if node_id is not None:
                components.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "component": safe_int(component)
                })

        return components

    # Alias for weakly_connected_components
    connected_components = weakly_connected_components
    wcc = weakly_connected_components

    def strongly_connected_components(self) -> list[dict]:
        """
        Find strongly connected components in the graph.

        Finds maximal subgraphs where every node is reachable from every
        other node following edge directions. Uses Tarjan's algorithm
        for O(V + E) complexity.

        Returns:
            List of dicts with 'node_id', 'user_id', 'component'
            where nodes in the same SCC share the same component number
        """
        result = self._conn.cypher("RETURN scc()")

        components = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            component = row.get("component")

            if node_id is not None:
                components.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "component": safe_int(component)
                })

        return components

    # Alias
    scc = strongly_connected_components
