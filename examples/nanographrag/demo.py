#!/usr/bin/env python3
"""
Demo: GraphQLite + sqlite-vec for Knowledge Graph with Vector Search

This demonstrates building a knowledge graph with:
1. Entity extraction (simulated - in real use, you'd use an LLM)
2. Graph storage via GraphQLite (Cypher queries)
3. Vector storage via sqlite-vec (similarity search)
4. Graph algorithms (PageRank, community detection)
"""

import struct
import sqlite3
from pathlib import Path
import sqlite_vec
import graphqlite
from graphqlite_graph import GraphQLiteGraph

import numpy as np
from sentence_transformers import SentenceTransformer
from entity_extractor import EntityExtractor


def serialize_float32(vec: list[float]) -> bytes:
    """Serialize a list of floats to bytes for sqlite-vec."""
    return struct.pack(f"{len(vec)}f", *vec)


def load_documents(docs_dir: str = "docs") -> list[str]:
    """
    Load all markdown documents from the docs directory.

    Args:
        docs_dir: Path to the docs directory (relative to this script)

    Returns:
        List of document contents
    """
    script_dir = Path(__file__).parent
    docs_path = script_dir / docs_dir

    if not docs_path.exists():
        raise FileNotFoundError(f"Docs directory not found: {docs_path}")

    documents = []
    for md_file in sorted(docs_path.glob("*.md")):
        content = md_file.read_text(encoding="utf-8")
        documents.append(content)

    print(f"Loaded {len(documents)} documents from {docs_path}")
    return documents


# =============================================================================
# Sample Knowledge Graph Data (for simple demo without entity extraction)
# =============================================================================

SAMPLE_ENTITIES = [
    # (id, name, type, description)
    ("e1", "Alice", "Person", "Software engineer at TechCorp"),
    ("e2", "Bob", "Person", "Data scientist working on ML projects"),
    ("e3", "Carol", "Person", "Product manager for the AI team"),
    ("e4", "TechCorp", "Organization", "Technology company specializing in AI"),
    ("e5", "ML Platform", "Project", "Internal machine learning platform"),
    ("e6", "Python", "Technology", "Programming language"),
    ("e7", "GraphQL", "Technology", "Query language for APIs"),
    ("e8", "Neo4j", "Technology", "Graph database"),
    ("e9", "SQLite", "Technology", "Embedded relational database"),
    ("e10", "AI Team", "Team", "Artificial intelligence research group"),
]

SAMPLE_RELATIONSHIPS = [
    # (source, target, type, description)
    ("e1", "e4", "WORKS_AT", "Full-time employee since 2020"),
    ("e2", "e4", "WORKS_AT", "Contractor since 2022"),
    ("e3", "e4", "WORKS_AT", "Joined in 2021"),
    ("e1", "e5", "CONTRIBUTES_TO", "Lead developer"),
    ("e2", "e5", "CONTRIBUTES_TO", "ML specialist"),
    ("e3", "e5", "MANAGES", "Product owner"),
    ("e1", "e6", "USES", "Primary language"),
    ("e2", "e6", "USES", "For ML development"),
    ("e5", "e6", "BUILT_WITH", "Backend implementation"),
    ("e5", "e7", "USES", "API layer"),
    ("e5", "e9", "USES", "Local data storage"),
    ("e1", "e2", "COLLABORATES_WITH", "On ML features"),
    ("e2", "e3", "REPORTS_TO", "Weekly standups"),
    ("e1", "e10", "MEMBER_OF", "Core contributor"),
    ("e2", "e10", "MEMBER_OF", "ML lead"),
    ("e3", "e10", "LEADS", "Team manager"),
    ("e10", "e4", "PART_OF", "Research division"),
]


def build_sample_graph(graph: GraphQLiteGraph):
    """Build a simple sample knowledge graph (without entity extraction)."""
    print("Building sample knowledge graph...")

    # Add entities as nodes
    for eid, name, etype, description in SAMPLE_ENTITIES:
        graph.upsert_node(
            node_id=eid,
            node_data={
                "name": name,
                "entity_type": etype,
                "description": description,
            },
            label=etype
        )

    # Add relationships as edges
    for source, target, rel_type, description in SAMPLE_RELATIONSHIPS:
        graph.upsert_edge(
            source_id=source,
            target_id=target,
            edge_data={"description": description},
            rel_type=rel_type
        )

    stats = graph.stats()
    print(f"  Created {stats['nodes']} nodes and {stats['edges']} edges")


def explore_graph(graph: GraphQLiteGraph):
    """Demonstrate graph queries."""
    print("\n" + "=" * 60)
    print("GRAPH EXPLORATION")
    print("=" * 60)

    # Count by entity type
    print("\n1. Entity counts by type:")
    for label in ["Person", "Organization", "Location"]:
        result = graph.query(f"MATCH (n:{label}) RETURN count(n) AS cnt")
        cnt = result[0].get('cnt', 0) if result else 0
        print(f"   - {label}: {cnt}")

    # Find all people (founders, CEOs, etc.)
    print("\n2. Key People (sample):")
    people = graph.query("MATCH (p:Person) RETURN p.name AS name LIMIT 15")
    names = [p.get('name', '') for p in people if p.get('name')]
    print(f"   {', '.join(names)}")

    # Find organizations
    print("\n3. Organizations (sample):")
    orgs = graph.query("MATCH (o:Organization) RETURN o.name AS name LIMIT 15")
    names = [o.get('name', '') for o in orgs if o.get('name')]
    print(f"   {', '.join(names)}")

    # Find locations mentioned
    print("\n4. Locations mentioned:")
    locs = graph.query("MATCH (l:Location) RETURN DISTINCT l.name AS name LIMIT 15")
    names = [l.get('name', '') for l in locs if l.get('name')]
    print(f"   {', '.join(names)}")

    # Find connections (who is related to whom) - sample only
    print("\n5. Sample relationships (first 15):")
    rels = graph.query(
        "MATCH (a)-[r]->(b) "
        "RETURN a.name AS source_name, b.name AS target_name LIMIT 15"
    )
    for r in rels:
        src = r.get('source_name', '?')
        tgt = r.get('target_name', '?')
        print(f"   - {src} --> {tgt}")


def analyze_communities(graph: GraphQLiteGraph):
    """Run community detection on the graph."""
    print("\n" + "=" * 60)
    print("COMMUNITY DETECTION")
    print("=" * 60)

    results = graph.community_detection()

    # Group nodes by community
    community_groups: dict[int, list[str]] = {}
    for r in results:
        comm_id = r["community"]
        name = r["user_id"] or f"Node {r['node_id']}"
        if comm_id not in community_groups:
            community_groups[comm_id] = []
        community_groups[comm_id].append(name)

    # Sort by size descending
    sorted_communities = sorted(community_groups.items(), key=lambda x: len(x[1]), reverse=True)

    print(f"\nFound {len(community_groups)} communities. Top 5 largest:")
    for comm_id, members in sorted_communities[:5]:
        print(f"\n  Community {comm_id}: {len(members)} members")
        print(f"    {', '.join(members[:8])}{'...' if len(members) > 8 else ''}")


def analyze_pagerank(graph: GraphQLiteGraph):
    """Run PageRank to find important nodes."""
    print("\n" + "=" * 60)
    print("PAGERANK ANALYSIS")
    print("=" * 60)

    ranks = graph.pagerank(damping=0.85, iterations=20)

    print(f"\nTop 15 entities by PageRank importance:")
    for r in ranks[:15]:
        name = r["user_id"] or f"Node {r['node_id']}"
        print(f"  {name}: {r['score']:.4f}")


class VectorIndex:
    """
    Vector index using sqlite-vec for similarity search.

    Stores embeddings in SQLite alongside the graph data.
    """

    def __init__(self, conn: sqlite3.Connection, embedding_dim: int = 384):
        """
        Initialize vector index.

        Args:
            conn: SQLite connection (should already have graphqlite loaded)
            embedding_dim: Dimension of embeddings (384 for all-MiniLM-L6-v2)
        """
        self.conn = conn
        self.dim = embedding_dim

        # Load sqlite-vec extension
        sqlite_vec.load(conn)

        # Create virtual table for vector storage
        conn.execute(f"""
            CREATE VIRTUAL TABLE IF NOT EXISTS entity_embeddings USING vec0(
                entity_id TEXT PRIMARY KEY,
                embedding FLOAT[{embedding_dim}]
            )
        """)

        # Create metadata table for text storage
        conn.execute("""
            CREATE TABLE IF NOT EXISTS entity_texts (
                entity_id TEXT PRIMARY KEY,
                text TEXT
            )
        """)
        conn.commit()

    def add(self, entity_id: str, text: str, embedding: list[float]):
        """Add an entity embedding to the index."""
        self.conn.execute(
            "INSERT OR REPLACE INTO entity_embeddings (entity_id, embedding) VALUES (?, ?)",
            (entity_id, serialize_float32(embedding))
        )
        self.conn.execute(
            "INSERT OR REPLACE INTO entity_texts (entity_id, text) VALUES (?, ?)",
            (entity_id, text)
        )

    def search(self, query_embedding: list[float], top_k: int = 3) -> list[tuple[str, float, str]]:
        """
        Search for similar entities.

        Returns:
            List of (entity_id, distance, text) tuples, sorted by distance (ascending)
        """
        results = self.conn.execute("""
            SELECT
                e.entity_id,
                e.distance,
                t.text
            FROM entity_embeddings e
            JOIN entity_texts t ON e.entity_id = t.entity_id
            WHERE e.embedding MATCH ?
                AND k = ?
        """, (serialize_float32(query_embedding), top_k)).fetchall()

        return [(r[0], r[1], r[2]) for r in results]


def semantic_search_demo(graph: GraphQLiteGraph):
    """Demonstrate semantic search using sqlite-vec."""
    print("\n" + "=" * 60)
    print("SEMANTIC SEARCH (sqlite-vec + sentence-transformers)")
    print("=" * 60)

    # Load embedding model
    print("\nLoading embedding model: all-MiniLM-L6-v2")
    model = SentenceTransformer("all-MiniLM-L6-v2")

    # Create vector index using the same SQLite connection
    vec_index = VectorIndex(graph._conn._conn, embedding_dim=384)

    # Embed all nodes
    nodes = graph.get_all_nodes()
    texts = []
    ids = []

    for node in nodes:
        if not node:
            continue
        props = node.get("properties", {})
        node_id = props.get("id", str(node.get("id", "unknown")))
        name = props.get("name", "")
        desc = props.get("description", "")
        entity_type = props.get("entity_type", "")
        text = f"{name}. {entity_type}. {desc}".strip()
        if text and text != ". ." and name:
            texts.append(text)
            ids.append(node_id)

    print(f"Embedding {len(texts)} entities...")
    embeddings = model.encode(texts, show_progress_bar=True)

    # Store in sqlite-vec
    for node_id, text, emb in zip(ids, texts, embeddings):
        vec_index.add(node_id, text, emb.tolist())
    vec_index.conn.commit()

    print(f"Stored {len(texts)} embeddings in sqlite-vec")

    # Run queries
    queries = [
        "Who are the leaders in artificial intelligence?",
        "Which companies make computer chips and semiconductors?",
        "Who founded companies in Silicon Valley?",
        "What are the major cloud computing platforms?",
        "Who are the billionaire tech entrepreneurs?",
    ]

    for query in queries:
        print(f"\nQuery: \"{query}\"")
        query_emb = model.encode([query])[0].tolist()
        results = vec_index.search(query_emb, top_k=3)
        for entity_id, distance, text in results:
            # Convert L2 distance to similarity score (approximate)
            similarity = 1 / (1 + distance)
            print(f"  [{similarity:.3f}] {text}")


def extraction_demo():
    """Demonstrate entity extraction from markdown documents."""
    print("=" * 60)
    print("ENTITY EXTRACTION (spaCy NER)")
    print("=" * 60)

    # Load documents from docs/ folder
    documents = load_documents()

    extractor = EntityExtractor()

    all_entities = []
    all_relationships = []

    for i, doc in enumerate(documents, 1):
        entities, relationships = extractor.extract(doc)
        all_entities.extend(entities)
        all_relationships.extend(relationships)

        # Uncomment for verbose per-document logging:
        # title = doc.split('\n')[0].replace('#', '').strip()[:50]
        # print(f"\n--- Document {i}: {title} ---")
        # print(f"  Entities: {[e.name for e in entities[:10]]}{'...' if len(entities) > 10 else ''}")
        # print(f"  Relationships: {len(relationships)}")

    # Dedupe entities by ID
    unique_entities = {e.id: e for e in all_entities}

    print(f"\nTotal: {len(unique_entities)} unique entities, {len(all_relationships)} relationships")

    return list(unique_entities.values()), all_relationships


def build_graph_from_extraction(graph: GraphQLiteGraph, entities, relationships):
    """Build graph from extracted entities and relationships."""
    print("\nBuilding graph from extracted data...")

    def sanitize_node_id(name: str) -> str:
        """Sanitize name for use as node_id - remove problematic characters."""
        import re
        # Remove quotes and apostrophes, normalize whitespace
        sanitized = re.sub(r"['\"]", "", name)
        sanitized = re.sub(r"\s+", " ", sanitized).strip()
        return sanitized

    # Map hash IDs to sanitized names for relationship lookups
    id_to_name = {}

    for entity in entities:
        sanitized_name = sanitize_node_id(entity.name)
        id_to_name[entity.id] = sanitized_name
        graph.upsert_node(
            node_id=sanitized_name,  # Use sanitized name as node_id
            node_data={
                "name": entity.name,  # Keep original name in properties
                "entity_type": entity.entity_type,
                "description": entity.description,
            },
            label=entity.entity_type
        )

    import re
    for rel in relationships:
        # Map hash IDs to names
        source_name = id_to_name.get(rel.source_id)
        target_name = id_to_name.get(rel.target_id)
        if source_name and target_name and graph.has_node(source_name) and graph.has_node(target_name):
            # Clean description of whitespace issues
            desc = re.sub(r"\s+", " ", rel.description).strip()
            graph.upsert_edge(
                source_id=source_name,
                target_id=target_name,
                edge_data={"description": desc},
                rel_type=rel.rel_type
            )

    stats = graph.stats()
    print(f"  Created {stats['nodes']} nodes and {stats['edges']} edges")


def main():
    print("=" * 60)
    print("GraphQLite + sqlite-vec Knowledge Graph Demo")
    print("=" * 60)

    # Part 1: Entity extraction from documents
    entities, relationships = extraction_demo()

    # Create file-based graph (sqlite-vec needs this for extension loading)
    import tempfile
    import os
    db_file = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
    db_path = db_file.name
    db_file.close()

    with GraphQLiteGraph(db_path=db_path) as graph:
        # Build graph from extracted entities
        build_graph_from_extraction(graph, entities, relationships)

        # Explore with Cypher queries
        explore_graph(graph)

        # Run graph algorithms
        analyze_pagerank(graph)
        analyze_communities(graph)

        # Semantic search with sqlite-vec
        semantic_search_demo(graph)

    # Cleanup temp file
    os.unlink(db_path)

    print("\n" + "=" * 60)
    print("Demo complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()
