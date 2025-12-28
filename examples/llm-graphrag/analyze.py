"""
Analyze the HotpotQA graph structure.

Shows community detection and graph statistics to demonstrate
why graph structure matters for multi-hop reasoning.
"""

import argparse
from collections import Counter
from pathlib import Path

from graphqlite import graph


def analyze_graph(db_path: str = "hotpotqa.db"):
    """Analyze the graph structure."""
    if not Path(db_path).exists():
        print(f"Database not found: {db_path}")
        print("Run ingest.py first.")
        return

    g = graph(db_path)

    print("=" * 70)
    print(" HotpotQA Graph Analysis")
    print("=" * 70)

    # Basic stats
    print("\n## Graph Statistics\n")

    result = g.connection.cypher("MATCH (n:Article) RETURN count(n) AS cnt")
    article_count = int(result[0]["cnt"]) if result else 0
    print(f"  Articles: {article_count}")

    result = g.connection.cypher("MATCH (n:Question) RETURN count(n) AS cnt")
    question_count = int(result[0]["cnt"]) if result else 0
    print(f"  Questions: {question_count}")

    result = g.connection.cypher("MATCH ()-[r:HAS_CONTEXT]->() RETURN count(r) AS cnt")
    ctx_count = int(result[0]["cnt"]) if result else 0
    print(f"  HAS_CONTEXT edges: {ctx_count}")

    result = g.connection.cypher("MATCH ()-[r:COOCCURS]->() RETURN count(r) AS cnt")
    cooccur_count = int(result[0]["cnt"]) if result else 0
    print(f"  COOCCURS edges: {cooccur_count}")

    # Community detection
    print("\n## Community Detection (Louvain)\n")
    print("  Running Louvain algorithm...")

    try:
        communities = g.louvain(resolution=1.0)

        if communities:
            # Count nodes per community
            comm_counts = Counter(c["community"] for c in communities)
            num_communities = len(comm_counts)

            print(f"  Found {num_communities} communities\n")

            # Show top communities
            print("  Top 10 communities by size:")
            for comm_id, count in comm_counts.most_common(10):
                print(f"    Community {comm_id}: {count} nodes")

            # Show sample articles from top 3 communities
            print("\n## Sample Articles by Community\n")

            # Build community -> node mapping
            comm_to_nodes = {}
            for c in communities:
                comm_id = c["community"]
                node_id = c.get("user_id") or c.get("node_id")
                if comm_id not in comm_to_nodes:
                    comm_to_nodes[comm_id] = []
                comm_to_nodes[comm_id].append(node_id)

            # Show samples from top 3 communities
            for comm_id, _ in comm_counts.most_common(3):
                node_ids = comm_to_nodes.get(comm_id, [])[:5]
                print(f"  Community {comm_id}:")
                for node_id in node_ids:
                    if node_id and node_id.startswith("article:"):
                        title = node_id.replace("article:", "")
                        print(f"    - {title[:60]}")
                print()
        else:
            print("  No communities found (graph may be disconnected)")

    except Exception as e:
        print(f"  Error running community detection: {e}")

    # Graph connectivity
    print("## Why Graph Structure Matters\n")
    print("""  Traditional RAG uses only vector similarity to find relevant documents.
  But multi-hop questions require connecting information across documents.

  The COOCCURS edges capture which articles appear together in HotpotQA
  question contexts. Community detection reveals topic clusters:

  - Articles in the same community are about related topics
  - Graph traversal can find related articles that vector search misses
  - Multi-hop reasoning paths follow the graph structure

  Example: "Were Scott Derrickson and Ed Wood of the same nationality?"

  Vector search finds "Ed Wood" but may miss "Scott Derrickson".
  Graph traversal from "Ed Wood" → COOCCURS → "Scott Derrickson"
  discovers the connection needed to answer the question.
""")

    g.close()


def main():
    parser = argparse.ArgumentParser(description="Analyze HotpotQA graph")
    parser.add_argument("--db", default="hotpotqa.db", help="Database path")
    args = parser.parse_args()

    analyze_graph(args.db)


if __name__ == "__main__":
    main()
