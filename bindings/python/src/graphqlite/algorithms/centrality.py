"""Centrality algorithms mixin."""

from typing import Any

from ..graph._base import BaseMixin
from ._parsing import safe_float, safe_int


class CentralityMixin(BaseMixin):
    """Mixin providing centrality algorithm methods."""

    def pagerank(
        self,
        damping: float = 0.85,
        iterations: int = 20
    ) -> list[dict]:
        """
        Run PageRank algorithm.

        Args:
            damping: Damping factor (default 0.85)
            iterations: Number of iterations (default 20)

        Returns:
            List of dicts with 'node_id', 'user_id', 'score'
            sorted by score descending
        """
        result = self._conn.cypher(
            f"RETURN pageRank({damping}, {iterations})"
        )

        ranks = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            score = row.get("score")
            if node_id is not None and score is not None:
                ranks.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "score": safe_float(score)
                })

        return ranks

    def degree_centrality(self) -> list[dict]:
        """
        Calculate degree centrality for all nodes.

        Returns the in-degree, out-degree, and total degree for each node.

        Returns:
            List of dicts with 'node_id', 'user_id', 'in_degree',
            'out_degree', 'degree'
        """
        result = self._conn.cypher("RETURN degreeCentrality()")

        degrees = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            in_degree = row.get("in_degree")
            out_degree = row.get("out_degree")
            degree = row.get("degree")

            if node_id is not None:
                degrees.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "in_degree": safe_int(in_degree),
                    "out_degree": safe_int(out_degree),
                    "degree": safe_int(degree)
                })

        return degrees

    def betweenness_centrality(self) -> list[dict]:
        """
        Calculate betweenness centrality for all nodes.

        Betweenness centrality measures how often a node lies on shortest
        paths between other nodes. Uses Brandes' algorithm for O(VE) complexity.

        Returns:
            List of dicts with 'node_id', 'user_id', 'score'
            where score is the betweenness centrality value
        """
        result = self._conn.cypher("RETURN betweennessCentrality()")

        scores = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            score = row.get("score")

            if node_id is not None:
                scores.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "score": safe_float(score)
                })

        return scores

    # Alias for betweenness_centrality
    betweenness = betweenness_centrality

    def closeness_centrality(self) -> list[dict]:
        """
        Calculate closeness centrality for all nodes.

        Closeness centrality measures how close a node is to all other nodes
        based on average shortest path length. Uses harmonic centrality variant
        to handle disconnected graphs. O(V * (V + E)) complexity.

        Returns:
            List of dicts with 'node_id', 'user_id', 'score'
            where score is the closeness centrality value (0 to 1)
        """
        result = self._conn.cypher("RETURN closenessCentrality()")

        scores = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            score = row.get("score")

            if node_id is not None:
                scores.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "score": safe_float(score)
                })

        return scores

    # Alias for closeness_centrality
    closeness = closeness_centrality

    def eigenvector_centrality(self, iterations: int = 100) -> list[dict]:
        """
        Calculate eigenvector centrality for all nodes.

        Eigenvector centrality measures node importance based on connections
        to other important nodes. Uses power iteration method.

        Unlike PageRank, eigenvector centrality has no damping factor and
        simply measures influence based on neighbor centrality scores.

        Args:
            iterations: Maximum iterations for power iteration (default 100)

        Returns:
            List of dicts with 'node_id', 'user_id', 'score'
            sorted by score descending
        """
        query = f"RETURN eigenvectorCentrality({iterations})"
        result = self._conn.cypher(query)

        scores = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            score = row.get("score")

            if node_id is not None:
                scores.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "score": safe_float(score)
                })

        return scores
