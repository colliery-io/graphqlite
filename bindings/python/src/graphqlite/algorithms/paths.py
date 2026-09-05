"""Path finding algorithms mixin."""
from __future__ import annotations

from typing import Optional

from ..graph._base import BaseMixin
from ..utils import assert_identifier
from ._parsing import extract_algo_array, extract_algo_object, safe_float, safe_int


class PathsMixin(BaseMixin):
    """Mixin providing path finding algorithm methods."""

    def shortest_path(
        self,
        source_id: str,
        target_id: str,
        weight_property: Optional[str] = None
    ) -> dict:
        """
        Find the shortest path between two nodes using Dijkstra's algorithm.

        Args:
            source_id: Source node's id property value
            target_id: Target node's id property value
            weight_property: Optional edge property to use as weight
                           (if None, uses unweighted/hop count)

        Returns:
            Dict with 'path' (list of node ids), 'distance', and 'found' (bool)
        """
        esc_source = self._escape(source_id)
        esc_target = self._escape(target_id)

        if weight_property:
            esc_weight = self._escape(weight_property)
            query = f'RETURN dijkstra("{esc_source}", "{esc_target}", "{esc_weight}")'
        else:
            query = f'RETURN dijkstra("{esc_source}", "{esc_target}")'

        data = extract_algo_object(self._conn.cypher(query))

        if data is None:
            return {"path": [], "distance": None, "found": False}

        return {
            "path": data.get("path", []),
            "distance": data.get("distance"),
            "found": data.get("found", False)
        }

    # Alias
    dijkstra = shortest_path

    def astar(
        self,
        source_id: str,
        target_id: str,
        lat_prop: str | None = None,
        lon_prop: str | None = None
    ) -> dict:
        """
        Find shortest path using A* algorithm with heuristic guidance.

        A* extends Dijkstra's algorithm with a heuristic function that
        estimates distance to the target, making it faster when node
        coordinates are available.

        Args:
            source_id: Starting node's id property value
            target_id: Target node's id property value
            lat_prop: Property name for latitude/y coordinate (optional)
            lon_prop: Property name for longitude/x coordinate (optional)

        Returns:
            Dict with 'path' (list of node ids), 'distance', 'found', 'nodes_explored'
        """
        if lat_prop and lon_prop:
            assert_identifier(lat_prop, "coordinate property")
            assert_identifier(lon_prop, "coordinate property")
            query = f"RETURN astar('{self._escape(source_id)}', '{self._escape(target_id)}', '{lat_prop}', '{lon_prop}')"
        else:
            query = f"RETURN astar('{self._escape(source_id)}', '{self._escape(target_id)}')"

        data = extract_algo_object(self._conn.cypher(query))

        if data is None:
            return {"path": [], "distance": None, "found": False, "nodes_explored": 0}

        return {
            "path": data.get("path", []),
            "distance": data.get("distance"),
            "found": data.get("found", False),
            "nodes_explored": safe_int(data.get("nodes_explored"))
        }

    # Alias
    a_star = astar

    def all_pairs_shortest_path(self) -> list[dict]:
        """
        Compute shortest paths between all pairs of nodes.

        Uses Floyd-Warshall algorithm with O(V³) time complexity and
        O(V²) space complexity. Only practical for graphs with < 10K nodes.

        Returns:
            List of dicts with 'source', 'target', 'distance' for each
            reachable pair of nodes (excludes self-loops and unreachable pairs)
        """
        rows = extract_algo_array(self._conn.cypher("RETURN apsp()"))

        paths = []
        for row in rows:
            source = row.get("source")
            target = row.get("target")
            distance = row.get("distance")

            if source is not None and target is not None:
                paths.append({
                    "source": source,
                    "target": target,
                    "distance": safe_float(distance)
                })

        return paths

    # Alias
    apsp = all_pairs_shortest_path
