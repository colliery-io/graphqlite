"""Graph traversal algorithms mixin."""

from ..graph._base import BaseMixin
from ._parsing import extract_algo_array, parse_traversal_result


class TraversalMixin(BaseMixin):
    """Mixin providing graph traversal algorithm methods."""

    def bfs(
        self,
        start_id: str,
        max_depth: int = -1
    ) -> list[dict]:
        """
        Perform breadth-first search traversal from a starting node.

        BFS explores nodes level by level, visiting all neighbors at depth d
        before any nodes at depth d+1. Useful for finding shortest paths in
        unweighted graphs.

        Args:
            start_id: Starting node's id property value
            max_depth: Maximum depth to traverse (-1 for unlimited)

        Returns:
            List of dicts with 'user_id', 'depth', 'order' sorted by traversal order
        """
        if max_depth < 0:
            query = f"RETURN bfs('{self._escape(start_id)}')"
        else:
            query = f"RETURN bfs('{self._escape(start_id)}', {max_depth})"

        result = self._conn.cypher(query)
        rows = extract_algo_array(result.to_list())

        nodes = []
        for row in rows:
            parsed = parse_traversal_result(row)
            if parsed is not None:
                nodes.append(parsed)

        return nodes

    # Alias
    breadth_first_search = bfs

    def dfs(
        self,
        start_id: str,
        max_depth: int = -1
    ) -> list[dict]:
        """
        Perform depth-first search traversal from a starting node.

        DFS explores as far as possible along each branch before backtracking.
        Useful for topological sorting, detecting cycles, and exploring paths.

        Args:
            start_id: Starting node's id property value
            max_depth: Maximum depth to traverse (-1 for unlimited)

        Returns:
            List of dicts with 'user_id', 'depth', 'order' sorted by traversal order
        """
        if max_depth < 0:
            query = f"RETURN dfs('{self._escape(start_id)}')"
        else:
            query = f"RETURN dfs('{self._escape(start_id)}', {max_depth})"

        result = self._conn.cypher(query)
        rows = extract_algo_array(result.to_list())

        nodes = []
        for row in rows:
            parsed = parse_traversal_result(row)
            if parsed is not None:
                nodes.append(parsed)

        return nodes

    # Alias
    depth_first_search = dfs
