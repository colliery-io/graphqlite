"""GraphQLite Graph - High-level graph operations using Cypher."""

import json
from pathlib import Path
from typing import Any, Optional, Union

from .connection import Connection, connect


# Cypher reserved keywords that can't be used as relationship types
CYPHER_RESERVED = {
    # Clauses
    'CREATE', 'MATCH', 'RETURN', 'WHERE', 'DELETE', 'SET', 'REMOVE',
    'ORDER', 'BY', 'SKIP', 'LIMIT', 'WITH', 'UNWIND', 'AS', 'AND', 'OR',
    'NOT', 'IN', 'IS', 'NULL', 'TRUE', 'FALSE', 'MERGE', 'ON', 'CALL',
    'YIELD', 'DETACH', 'OPTIONAL', 'UNION', 'ALL', 'CASE', 'WHEN', 'THEN',
    'ELSE', 'END', 'EXISTS', 'FOREACH',
    # Aggregate functions
    'COUNT', 'SUM', 'AVG', 'MIN', 'MAX', 'COLLECT',
    # List functions and expressions
    'REDUCE', 'FILTER', 'EXTRACT', 'ANY', 'NONE', 'SINGLE',
    # Other reserved words
    'STARTS', 'ENDS', 'CONTAINS', 'XOR', 'DISTINCT', 'LOAD', 'CSV',
    'USING', 'PERIODIC', 'COMMIT', 'CONSTRAINT', 'INDEX', 'DROP', 'ASSERT',
}


def escape_string(s: str) -> str:
    """
    Escape a string for use in Cypher queries.

    Handles backslashes, quotes, and whitespace characters.

    Args:
        s: String to escape

    Returns:
        Escaped string safe for Cypher queries
    """
    return (s.replace("\\", "\\\\")
             .replace("'", "\\'")
             .replace('"', '\\"')
             .replace("\n", " ")
             .replace("\r", " ")
             .replace("\t", " "))


def sanitize_rel_type(rel_type: str) -> str:
    """
    Sanitize a relationship type for use in Cypher.

    Ensures the type is a valid Cypher identifier and not a reserved word.

    Args:
        rel_type: Relationship type name

    Returns:
        Safe relationship type name
    """
    safe = ''.join(c if c.isalnum() or c == '_' else '_' for c in rel_type)
    if not safe or safe[0].isdigit():
        safe = "REL_" + safe
    if safe.upper() in CYPHER_RESERVED:
        safe = "REL_" + safe
    return safe


class Graph:
    """
    High-level graph operations using GraphQLite.

    Provides ergonomic node/edge CRUD, graph queries, and algorithm wrappers
    on top of the raw Cypher interface.

    Example:
        >>> graph = Graph(":memory:")
        >>> graph.upsert_node("alice", {"name": "Alice"}, label="Person")
        >>> graph.upsert_node("bob", {"name": "Bob"}, label="Person")
        >>> graph.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")
        >>> print(graph.stats())
        {'nodes': 2, 'edges': 1}
    """

    def __init__(
        self,
        db_path: Union[str, Path] = ":memory:",
        namespace: str = "default",
        extension_path: Optional[str] = None
    ):
        """
        Initialize a Graph instance.

        Args:
            db_path: Path to database file or ":memory:" for in-memory
            namespace: Optional namespace for isolating graphs (reserved for future use)
            extension_path: Path to graphqlite extension (auto-detected if None)
        """
        self._conn = connect(str(db_path), extension_path=extension_path)
        self.namespace = namespace

    def close(self) -> None:
        """Close the database connection."""
        if self._conn:
            self._conn.close()
            self._conn = None

    def __enter__(self) -> "Graph":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    @property
    def connection(self) -> Connection:
        """Access the underlying Connection object."""
        return self._conn

    # -------------------------------------------------------------------------
    # Utility Methods
    # -------------------------------------------------------------------------

    def _escape(self, s: str) -> str:
        """Escape a string for Cypher queries."""
        return escape_string(s)

    def _format_props(self, props: dict[str, Any]) -> str:
        """
        Format a properties dict as a Cypher property string.

        Args:
            props: Dictionary of property key-value pairs

        Returns:
            String like "key1: 'value1', key2: 123"
        """
        parts = []
        for k, v in props.items():
            if isinstance(v, str):
                parts.append(f"{k}: '{self._escape(v)}'")
            elif isinstance(v, bool):
                parts.append(f"{k}: {str(v).lower()}")
            elif v is None:
                parts.append(f"{k}: null")
            else:
                parts.append(f"{k}: {v}")
        return ", ".join(parts)

    # -------------------------------------------------------------------------
    # Node Operations
    # -------------------------------------------------------------------------

    def has_node(self, node_id: str) -> bool:
        """
        Check if a node exists.

        Args:
            node_id: The node's id property value

        Returns:
            True if node exists, False otherwise
        """
        result = self._conn.cypher(
            f"MATCH (n {{id: '{self._escape(node_id)}'}}) RETURN count(n) AS cnt"
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
            f"MATCH (n {{id: '{self._escape(node_id)}'}}) RETURN n"
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
            node_id: Unique identifier for the node (stored as 'id' property)
            node_data: Dictionary of properties to set
            label: Node label (only used on creation)
        """
        props = {"id": node_id, **node_data}

        if self.has_node(node_id):
            # Update existing node
            for k, v in node_data.items():
                if isinstance(v, str):
                    val = f"'{self._escape(v)}'"
                elif isinstance(v, bool):
                    val = str(v).lower()
                elif v is None:
                    val = "null"
                else:
                    val = str(v)
                self._conn.cypher(
                    f"MATCH (n {{id: '{self._escape(node_id)}'}}) "
                    f"SET n.{k} = {val} RETURN n"
                )
        else:
            # Create new node
            prop_str = self._format_props(props)
            self._conn.cypher(f"CREATE (n:{label} {{{prop_str}}})")

    def delete_node(self, node_id: str) -> None:
        """
        Delete a node and its relationships.

        Args:
            node_id: The node's id property value
        """
        self._conn.cypher(
            f"MATCH (n {{id: '{self._escape(node_id)}'}}) DETACH DELETE n"
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

    # -------------------------------------------------------------------------
    # Edge Operations
    # -------------------------------------------------------------------------

    def has_edge(self, source_id: str, target_id: str) -> bool:
        """
        Check if an edge exists between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id

        Returns:
            True if edge exists, False otherwise
        """
        result = self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r]->"
            f"(b {{id: '{self._escape(target_id)}'}}) "
            f"RETURN count(r) AS cnt"
        )
        if len(result) == 0:
            return False
        cnt = result[0].get("cnt", 0)
        return int(cnt) > 0 if cnt else False

    def get_edge(self, source_id: str, target_id: str) -> Optional[dict]:
        """
        Get edge properties between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id

        Returns:
            Edge dict or None if not found
        """
        result = self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r]->"
            f"(b {{id: '{self._escape(target_id)}'}}) RETURN r"
        )
        if len(result) == 0:
            return None
        return result[0].get("r")

    def upsert_edge(
        self,
        source_id: str,
        target_id: str,
        edge_data: dict[str, Any],
        rel_type: str = "RELATED"
    ) -> None:
        """
        Create or update an edge between two nodes.

        If an edge already exists, this is a no-op.
        Both source and target nodes must exist.

        Args:
            source_id: Source node id
            target_id: Target node id
            edge_data: Dictionary of edge properties
            rel_type: Relationship type label
        """
        safe_rel_type = sanitize_rel_type(rel_type)

        if self.has_edge(source_id, target_id):
            return

        esc_source = self._escape(source_id)
        esc_target = self._escape(target_id)

        if edge_data:
            prop_str = self._format_props(edge_data)
            query = (
                f"MATCH (a {{id: '{esc_source}'}}), (b {{id: '{esc_target}'}}) "
                f"CREATE (a)-[r:{safe_rel_type} {{{prop_str}}}]->(b)"
            )
        else:
            query = (
                f"MATCH (a {{id: '{esc_source}'}}), (b {{id: '{esc_target}'}}) "
                f"CREATE (a)-[r:{safe_rel_type}]->(b)"
            )
        self._conn.cypher(query)

    def delete_edge(self, source_id: str, target_id: str) -> None:
        """
        Delete edge between two nodes.

        Args:
            source_id: Source node id
            target_id: Target node id
        """
        self._conn.cypher(
            f"MATCH (a {{id: '{self._escape(source_id)}'}})-[r]->"
            f"(b {{id: '{self._escape(target_id)}'}}) DELETE r"
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

    # -------------------------------------------------------------------------
    # Graph Queries
    # -------------------------------------------------------------------------

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

    # -------------------------------------------------------------------------
    # Graph Algorithms
    # -------------------------------------------------------------------------

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

        # C code already returns sorted results with user_id
        ranks = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            score = row.get("score")
            if node_id is not None and score is not None:
                ranks.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "score": float(score) if score else 0.0
                })

        return ranks

    def community_detection(self, iterations: int = 10) -> list[dict]:
        """
        Run community detection using label propagation.

        Args:
            iterations: Number of iterations (default 10)

        Returns:
            List of dicts with 'node_id', 'user_id', 'community'
        """
        result = self._conn.cypher(f"RETURN labelPropagation({iterations})")

        # C code already returns user_id
        communities = []
        for row in result:
            node_id = row.get("node_id")
            user_id = row.get("user_id")
            community = row.get("community")
            if node_id is not None and community is not None:
                communities.append({
                    "node_id": str(node_id),
                    "user_id": user_id,
                    "community": int(community) if community else 0
                })

        return communities

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

        Example:
            >>> result = graph.shortest_path("alice", "carol")
            >>> print(result)
            {'path': ['alice', 'bob', 'carol'], 'distance': 2, 'found': True}
        """
        esc_source = self._escape(source_id)
        esc_target = self._escape(target_id)

        if weight_property:
            esc_weight = self._escape(weight_property)
            query = f'RETURN dijkstra("{esc_source}", "{esc_target}", "{esc_weight}")'
        else:
            query = f'RETURN dijkstra("{esc_source}", "{esc_target}")'

        result = self._conn.cypher(query)

        if len(result) == 0:
            return {"path": [], "distance": None, "found": False}

        # Result comes as a single row with the JSON object
        row = result[0]

        # Handle nested column_0 structure from algorithm return
        if "column_0" in row:
            data = row["column_0"]
            if isinstance(data, dict):
                return {
                    "path": data.get("path", []),
                    "distance": data.get("distance"),
                    "found": data.get("found", False)
                }

        # Direct access if already unpacked
        return {
            "path": row.get("path", []),
            "distance": row.get("distance"),
            "found": row.get("found", False)
        }

    def degree_centrality(self) -> list[dict]:
        """
        Calculate degree centrality for all nodes.

        Returns the in-degree, out-degree, and total degree for each node.

        Returns:
            List of dicts with 'node_id', 'user_id', 'in_degree',
            'out_degree', 'degree'

        Example:
            >>> results = graph.degree_centrality()
            >>> for node in results:
            ...     print(f"{node['user_id']}: {node['degree']} connections")
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
                    "in_degree": int(in_degree) if in_degree is not None else 0,
                    "out_degree": int(out_degree) if out_degree is not None else 0,
                    "degree": int(degree) if degree is not None else 0
                })

        return degrees

    # -------------------------------------------------------------------------
    # Batch Operations
    # -------------------------------------------------------------------------

    def upsert_nodes_batch(
        self,
        nodes: list[tuple[str, dict[str, Any], str]]
    ) -> None:
        """
        Batch upsert multiple nodes.

        Args:
            nodes: List of (node_id, properties, label) tuples
        """
        for node_id, props, label in nodes:
            self.upsert_node(node_id, props, label)

    def upsert_edges_batch(
        self,
        edges: list[tuple[str, str, dict[str, Any], str]]
    ) -> None:
        """
        Batch upsert multiple edges.

        Args:
            edges: List of (source_id, target_id, properties, rel_type) tuples
        """
        for source_id, target_id, props, rel_type in edges:
            self.upsert_edge(source_id, target_id, props, rel_type)

    # -------------------------------------------------------------------------
    # Query Interface
    # -------------------------------------------------------------------------

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


def graph(
    db_path: Union[str, Path] = ":memory:",
    namespace: str = "default",
    extension_path: Optional[str] = None
) -> Graph:
    """
    Create a new Graph instance.

    Factory function matching the style of graphqlite.connect().

    Args:
        db_path: Path to database file or ":memory:" for in-memory
        namespace: Optional namespace for isolating graphs
        extension_path: Path to graphqlite extension (auto-detected if None)

    Returns:
        Graph instance

    Example:
        >>> g = graphqlite.graph(":memory:")
        >>> g.upsert_node("n1", {"name": "Test"})
    """
    return Graph(db_path, namespace, extension_path)
