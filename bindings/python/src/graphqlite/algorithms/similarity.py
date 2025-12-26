"""Similarity algorithms mixin."""
from __future__ import annotations

from ..graph._base import BaseMixin
from ._parsing import safe_float, safe_int


class SimilarityMixin(BaseMixin):
    """Mixin providing similarity and clustering algorithm methods."""

    def node_similarity(
        self,
        node1_id: str | None = None,
        node2_id: str | None = None,
        threshold: float = 0.0,
        top_k: int = 0
    ) -> list[dict]:
        """
        Compute node similarity using Jaccard coefficient.

        Jaccard similarity measures how similar two nodes are based on their
        shared neighbors: |N(a) ∩ N(b)| / |N(a) ∪ N(b)|

        Args:
            node1_id: First node's id (optional - if both provided, returns single pair)
            node2_id: Second node's id (optional - required with node1_id)
            threshold: Minimum similarity to include in results (default 0.0)
            top_k: Maximum number of pairs to return (0 = unlimited)

        Returns:
            List of dicts with 'node1', 'node2', 'similarity'
        """
        if node1_id and node2_id:
            query = f"RETURN nodeSimilarity('{self._escape(node1_id)}', '{self._escape(node2_id)}')"
        elif threshold > 0 and top_k > 0:
            query = f"RETURN nodeSimilarity({threshold}, {top_k})"
        elif threshold > 0:
            query = f"RETURN nodeSimilarity({threshold})"
        else:
            query = "RETURN nodeSimilarity()"

        result = self._conn.cypher(query)

        pairs = []
        for row in result:
            node1 = row.get("node1")
            node2 = row.get("node2")
            similarity = row.get("similarity")

            if node1 is not None and node2 is not None:
                pairs.append({
                    "node1": node1,
                    "node2": node2,
                    "similarity": safe_float(similarity)
                })

        return pairs

    def knn(self, node_id: str, k: int = 10) -> list[dict]:
        """
        Find K-nearest neighbors using Jaccard similarity.

        Returns the K most similar nodes to the given node based on
        shared neighbors (Jaccard coefficient).

        Args:
            node_id: The node's id property value
            k: Number of neighbors to return (default 10)

        Returns:
            List of dicts with 'neighbor', 'similarity', 'rank'
            sorted by similarity descending
        """
        query = f"RETURN knn('{self._escape(node_id)}', {k})"
        result = self._conn.cypher(query)

        neighbors = []
        for row in result:
            neighbor = row.get("neighbor")
            similarity = row.get("similarity")
            rank = row.get("rank")

            if neighbor is not None:
                neighbors.append({
                    "neighbor": neighbor,
                    "similarity": safe_float(similarity),
                    "rank": safe_int(rank)
                })

        return neighbors

    def triangle_count(self) -> list[dict]:
        """
        Count triangles each node participates in.

        A triangle is a set of 3 nodes that are all connected to each other.
        Also computes the local clustering coefficient for each node.

        Returns:
            List of dicts with 'node_id', 'user_id', 'triangles', 'clustering_coefficient'
        """
        result = self._conn.cypher("RETURN triangleCount()")

        triangles = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            tri_count = row.get("triangles")
            clustering = row.get("clustering_coefficient")

            if node_id is not None:
                triangles.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "triangles": safe_int(tri_count),
                    "clustering_coefficient": safe_float(clustering)
                })

        return triangles

    # Alias
    triangles = triangle_count
