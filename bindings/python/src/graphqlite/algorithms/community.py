"""Community detection algorithms mixin."""

from typing import Optional

from ..graph._base import BaseMixin
from ._parsing import safe_int


class CommunityMixin(BaseMixin):
    """Mixin providing community detection algorithm methods."""

    def community_detection(self, iterations: int = 10) -> list[dict]:
        """
        Run community detection using label propagation.

        Args:
            iterations: Number of iterations (default 10)

        Returns:
            List of dicts with 'node_id', 'user_id', 'community'
        """
        result = self._conn.cypher(f"RETURN labelPropagation({iterations})")

        communities = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            community = row.get("community")
            if node_id is not None and community is not None:
                communities.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "community": safe_int(community)
                })

        return communities

    def louvain(self, resolution: float = 1.0) -> list[dict]:
        """
        Run Louvain community detection algorithm.

        Louvain is a fast modularity optimization algorithm that produces
        high-quality communities. More sophisticated than label propagation.

        Args:
            resolution: Resolution parameter (default 1.0). Higher values
                       produce more communities, lower values fewer.

        Returns:
            List of dicts with 'node_id', 'user_id', 'community'
        """
        result = self._conn.cypher(f"RETURN louvain({resolution})")

        communities = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            community = row.get("community")

            if node_id is not None:
                communities.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "community": safe_int(community)
                })

        return communities

    def leiden_communities(
        self,
        resolution: float = 1.0,
        random_seed: Optional[int] = None
    ) -> list[dict]:
        """
        Run Leiden community detection.

        Uses graspologic's leiden algorithm for high-quality community detection.

        Requires graspologic: pip install graphqlite[leiden]

        Args:
            resolution: Resolution parameter (higher = more communities)
            random_seed: Random seed for reproducibility

        Returns:
            List of dicts with 'node_id', 'community'
        """
        try:
            from graspologic.partition import leiden
        except ImportError:
            raise ImportError(
                "graspologic is required for leiden_communities(). "
                "Install with: pip install graphqlite[leiden]"
            )

        # Get all edges as weighted edge list (source, target, weight)
        edges = self.get_all_edges()

        if not edges:
            return []

        # Build edge list in graspologic format
        edge_list = []
        nodes = set()
        for edge in edges:
            source = edge.get("source")
            target = edge.get("target")
            if source and target:
                nodes.add(source)
                nodes.add(target)
                edge_list.append((source, target, 1.0))

        if not edge_list:
            return []

        # Run Leiden
        partitions = leiden(
            edge_list,
            resolution=resolution,
            random_seed=random_seed
        )

        # Convert results
        results = []
        for node_id, community in partitions.items():
            results.append({
                "node_id": str(node_id),
                "community": int(community)
            })

        return results
