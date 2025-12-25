"""Graph export algorithms mixin."""

from ..graph._base import BaseMixin


class ExportMixin(BaseMixin):
    """Mixin providing graph export functionality."""

    def to_rustworkx(self):
        """
        Export the graph to a rustworkx PyDiGraph.

        Requires rustworkx to be installed: pip install rustworkx

        Returns:
            Tuple of (rustworkx.PyDiGraph, dict mapping node_id to index)

        Raises:
            ImportError: If rustworkx is not installed
        """
        try:
            import rustworkx as rx
        except ImportError:
            raise ImportError(
                "rustworkx is required for to_rustworkx(). "
                "Install with: pip install rustworkx"
            )

        G = rx.PyDiGraph()
        node_id_to_index = {}

        # Add nodes with their properties
        nodes = self.get_all_nodes()
        for node in nodes:
            if isinstance(node, dict):
                props = node.get("properties", {})
                node_id = props.get("id")
                if node_id:
                    idx = G.add_node({"id": node_id, **props})
                    node_id_to_index[node_id] = idx

        # Add edges
        edges = self.get_all_edges()
        for edge in edges:
            source = edge.get("source")
            target = edge.get("target")
            if source and target and source in node_id_to_index and target in node_id_to_index:
                edge_props = edge.get("r", {})
                if isinstance(edge_props, dict):
                    props = edge_props.get("properties", {})
                else:
                    props = {}
                G.add_edge(node_id_to_index[source], node_id_to_index[target], props)

        return G, node_id_to_index
