"""
GraphRAG query interface for HotpotQA.

Uses vector similarity + graph traversal to find relevant context,
then queries an LLM for the answer.
"""

import argparse
import struct
from pathlib import Path

import numpy as np
from sentence_transformers import SentenceTransformer

from graphqlite import graph
from ollama_client import OllamaClient, Message


def deserialize_embedding(blob: bytes) -> np.ndarray:
    """Deserialize embedding from sqlite-vec bytes."""
    n = len(blob) // 4
    return np.array(struct.unpack(f"{n}f", blob), dtype=np.float32)


SYSTEM_PROMPT = """You are a helpful assistant that answers questions based on the provided context.

Instructions:
- Answer the question using ONLY the information in the context below
- Be concise and direct
- If the context doesn't contain enough information, say so
- For yes/no questions, start with "Yes" or "No" then explain briefly"""


class GraphRAG:
    """GraphRAG query interface."""

    def __init__(self, db_path: str = "hotpotqa.db", model: str = "qwen3:8b"):
        self.g = graph(db_path)
        self.embed_model = SentenceTransformer("all-MiniLM-L6-v2")
        self.llm = OllamaClient(model=model)
        self._communities = None  # Lazy-loaded community map

        # Load sqlite-vec
        import sqlite_vec
        sqlite_conn = self.g.connection.sqlite_connection
        sqlite_conn.enable_load_extension(True)
        sqlite_vec.load(sqlite_conn)
        sqlite_conn.enable_load_extension(False)

    def _get_communities(self) -> dict[str, int]:
        """Lazy-load community detection results."""
        if self._communities is None:
            print("  Running community detection (Louvain)...", end="", flush=True)
            results = self.g.louvain(resolution=1.0)
            self._communities = {}
            for r in results:
                node_id = r.get("user_id") or r.get("node_id")
                if node_id:
                    self._communities[node_id] = r.get("community", -1)
            print(f" found {len(set(self._communities.values()))} communities")
        return self._communities

    def get_community_articles(self, article_id: str, limit: int = 3) -> tuple[list[dict], int]:
        """Get other articles in the same community."""
        communities = self._get_communities()
        my_community = communities.get(article_id, -1)

        if my_community == -1:
            return [], -1

        # Find other articles in same community
        same_community = []
        for node_id, comm in communities.items():
            if comm == my_community and node_id != article_id and node_id.startswith("article:"):
                same_community.append(node_id)

        # Get article details for a sample
        results = []
        for node_id in same_community[:limit]:
            node = self.g.get_node(node_id)
            if node:
                props = node.get("properties", {})
                results.append({
                    "id": node_id,
                    "title": props.get("title", "Unknown"),
                    "text": props.get("text", ""),
                })

        return results, my_community

    def embed_query(self, query: str) -> bytes:
        """Embed a query string."""
        embedding = self.embed_model.encode([query], convert_to_numpy=True)[0]
        return struct.pack(f"{len(embedding)}f", *embedding.astype(np.float32))

    def vector_search(self, query: str, k: int = 5) -> list[dict]:
        """Find top-k similar articles using vector search."""
        query_embedding = self.embed_query(query)

        sqlite_conn = self.g.connection.sqlite_connection
        cursor = sqlite_conn.execute("""
            SELECT article_id, distance
            FROM article_embeddings
            WHERE embedding MATCH ?
            ORDER BY distance
            LIMIT ?
        """, (query_embedding, k))

        results = []
        for row in cursor:
            article_id = row[0]
            distance = row[1]
            # Get article properties
            node = self.g.get_node(article_id)
            if node:
                results.append({
                    "id": article_id,
                    "distance": distance,
                    "properties": node.get("properties", {}),
                })
        return results

    def get_related_articles(self, article_id: str, max_hops: int = 1) -> tuple[list[dict], str]:
        """Get articles related via COOCCURS edges. Returns (results, query)."""
        related = []

        # Get direct neighbors via graph traversal
        query = f"""MATCH (a {{id: '{article_id}'}})-[:COOCCURS]-(b:Article)
RETURN b.id AS id, b.title AS title, b.text AS text
LIMIT 5"""

        result = self.g.connection.cypher(query)

        for row in result:
            related.append({
                "id": row.get("id"),
                "title": row.get("title"),
                "text": row.get("text"),
            })

        return related, query

    def build_context(self, query: str, k: int = 3, include_related: bool = True) -> tuple[str, list[dict], list[dict]]:
        """Build context for answering a query. Returns (context, graph_queries, community_info)."""
        # Vector search for seed articles
        seed_articles = self.vector_search(query, k=k)

        context_parts = []
        seen_ids = set()
        graph_queries = []  # Track graph traversals
        community_info = []  # Track community-based retrieval

        for article in seed_articles:
            article_id = article["id"]
            props = article["properties"]
            title = props.get("title", "Unknown")
            text = props.get("text", "")

            if article_id not in seen_ids:
                seen_ids.add(article_id)
                context_parts.append(f"## {title}\n{text}")

            if include_related:
                # Method 1: Graph traversal via COOCCURS edges
                related_articles, cypher_query = self.get_related_articles(article_id)

                graph_queries.append({
                    "source_article": title,
                    "query": cypher_query,
                    "results": [r.get("title", "Unknown") for r in related_articles],
                })

                for related in related_articles:
                    rel_id = related["id"]
                    if rel_id and rel_id not in seen_ids:
                        seen_ids.add(rel_id)
                        rel_title = related.get("title", "Unknown")
                        rel_text = related.get("text", "")
                        context_parts.append(f"## {rel_title} (via COOCCURS)\n{rel_text}")

                # Method 2: Community-based retrieval
                community_articles, community_id = self.get_community_articles(article_id, limit=2)

                if community_id != -1:
                    community_info.append({
                        "source_article": title,
                        "community_id": community_id,
                        "results": [r.get("title", "Unknown") for r in community_articles],
                    })

                    for comm_article in community_articles:
                        comm_id = comm_article["id"]
                        if comm_id and comm_id not in seen_ids:
                            seen_ids.add(comm_id)
                            comm_title = comm_article.get("title", "Unknown")
                            comm_text = comm_article.get("text", "")
                            context_parts.append(f"## {comm_title} (community {community_id})\n{comm_text}")

        return "\n\n".join(context_parts), graph_queries, community_info

    def query(self, question: str, use_llm: bool = True) -> dict:
        """
        Answer a question using GraphRAG.

        Args:
            question: The question to answer
            use_llm: Whether to use an LLM for the final answer

        Returns:
            Dict with full data flow: search results, graph queries, context, prompt, answer
        """
        # Step 1: Vector search
        search_results = self.vector_search(question, k=3)

        # Step 2: Build context with graph traversal + community retrieval
        context, graph_queries, community_info = self.build_context(question)

        # Step 3: Build prompt
        user_prompt = f"""Context:
{context}

Question: {question}

Answer:"""

        result = {
            "question": question,
            "search_results": search_results,
            "graph_queries": graph_queries,
            "community_info": community_info,
            "context": context,
            "system_prompt": SYSTEM_PROMPT,
            "user_prompt": user_prompt,
            "answer": None,
        }

        # Step 4: LLM inference
        if use_llm:
            messages = [
                Message(role="system", content=SYSTEM_PROMPT),
                Message(role="user", content=user_prompt),
            ]

            try:
                result["answer"] = self.llm.chat(messages, temperature=0.3)
            except Exception as e:
                result["answer"] = f"[LLM error: {e}]"

        return result

    def close(self):
        """Close connections."""
        self.g.close()
        self.llm.close()


def print_section(title: str, content: str, width: int = 70):
    """Print a section with a header."""
    print(f"\n{'=' * width}")
    print(f" {title}")
    print('=' * width)
    print(content)


def main():
    parser = argparse.ArgumentParser(description="Query the HotpotQA GraphRAG")
    parser.add_argument("question", nargs="?", help="Question to answer")
    parser.add_argument("--db", default="hotpotqa.db", help="Database path")
    parser.add_argument("--model", default="qwen3:8b", help="Ollama model to use")
    parser.add_argument("-k", type=int, default=3, help="Number of seed articles")
    parser.add_argument("--no-llm", action="store_true", help="Skip LLM, just show context")

    args = parser.parse_args()

    if not Path(args.db).exists():
        print(f"Database not found: {args.db}")
        print("Run ingest.py first to create the database.")
        return

    rag = GraphRAG(args.db, model=args.model)

    # Check if Ollama is available
    if not args.no_llm and not rag.llm.is_available():
        print("Warning: Ollama not available. Run 'ollama serve' first.")
        print("Falling back to context-only mode.\n")
        args.no_llm = True

    def display_result(result: dict):
        """Display the full data flow."""
        print("\n" + "#" * 70)
        print(f" QUESTION: {result['question']}")
        print("#" * 70)

        # Step 1: Vector Search Results
        search_info = []
        for i, sr in enumerate(result["search_results"], 1):
            props = sr.get("properties", {})
            title = props.get("title", "Unknown")
            dist = sr.get("distance", 0)
            search_info.append(f"  {i}. {title} (distance: {dist:.4f})")
        print_section("STEP 1: Vector Search Results", "\n".join(search_info))

        # Step 2: Graph Traversal Queries (COOCCURS edges)
        graph_info = []
        for gq in result.get("graph_queries", []):
            graph_info.append(f"Source: {gq['source_article']}")
            graph_info.append(f"Cypher: {gq['query'].replace(chr(10), ' ')}")
            results = gq.get('results', [])
            if results:
                graph_info.append(f"Returns ({len(results)}):")
                for title in results:
                    graph_info.append(f"    -> {title}")
            else:
                graph_info.append("Returns: (none)")
            graph_info.append("")
        print_section("STEP 2: Graph Traversal (COOCCURS edges)", "\n".join(graph_info))

        # Step 3: Community-Based Retrieval (Louvain)
        community_data = result.get("community_info", [])
        if community_data:
            comm_info = []
            for ci in community_data:
                comm_info.append(f"Source: {ci['source_article']} (community {ci['community_id']})")
                results = ci.get('results', [])
                if results:
                    comm_info.append(f"Related articles in same community:")
                    for title in results:
                        comm_info.append(f"    -> {title}")
                else:
                    comm_info.append("No other articles in community")
                comm_info.append("")
            print_section("STEP 3: Community Detection (Louvain)", "\n".join(comm_info))
        else:
            print_section("STEP 3: Community Detection (Louvain)", "(no community data)")

        # Step 4: Combined Context
        print_section("STEP 4: Combined Context (vector + graph + community)", result["context"])

        # Step 5: LLM Prompt Summary
        prompt_summary = f"""System: {result["system_prompt"][:100]}...

Question: {result["question"]}"""
        print_section("STEP 5: LLM Prompt", prompt_summary)

        # Step 6: Answer
        if result["answer"]:
            print_section("STEP 6: LLM Answer", result["answer"])
        else:
            print_section("STEP 6: LLM Answer", "(skipped)")

    if args.question:
        result = rag.query(args.question, use_llm=not args.no_llm)
        display_result(result)
    else:
        # Interactive mode
        print("GraphRAG Interactive Query")
        print(f"Model: {args.model}")
        print("Type 'quit' to exit\n")

        while True:
            try:
                question = input("Q: ").strip()
                if question.lower() in ("quit", "exit", "q"):
                    break
                if not question:
                    continue

                print("\nProcessing...", flush=True)
                result = rag.query(question, use_llm=not args.no_llm)
                display_result(result)
                print()

            except KeyboardInterrupt:
                print()
                break

    rag.close()


if __name__ == "__main__":
    main()
