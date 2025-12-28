"""
Ingest HotpotQA into GraphQLite + sqlite-vec.

Creates a knowledge graph where:
- Article nodes contain Wikipedia paragraph text
- Question nodes contain multi-hop questions
- Edges connect articles that appear together in question contexts

This structure captures the multi-hop reasoning paths required by HotpotQA.
"""

import argparse
import os
import struct
import logging
import warnings
from pathlib import Path

# Suppress library logging before imports
os.environ["TOKENIZERS_PARALLELISM"] = "true"
logging.getLogger("sentence_transformers").setLevel(logging.WARNING)
logging.getLogger("transformers").setLevel(logging.WARNING)
logging.getLogger("huggingface_hub").setLevel(logging.WARNING)
warnings.filterwarnings("ignore", category=FutureWarning)

import numpy as np
from sentence_transformers import SentenceTransformer

from graphqlite import graph
from hotpotqa import (
    download_dataset,
    load_dataset,
    get_dataset_stats,
    HOTPOTQA_DEV_DISTRACTOR,
)


def make_safe_id(s: str) -> str:
    """Create a safe node ID from a string by removing problematic characters."""
    # Replace quotes and backslashes with underscores
    return s.replace("'", "_").replace('"', "_").replace("\\", "_")


def load_vec_extension(g):
    """Load sqlite-vec extension for vector similarity search."""
    import sqlite_vec

    sqlite_conn = g.connection.sqlite_connection
    sqlite_conn.enable_load_extension(True)
    sqlite_vec.load(sqlite_conn)
    sqlite_conn.enable_load_extension(False)

    # Create embedding table for articles (keyed by article id)
    sqlite_conn.execute("""
        CREATE VIRTUAL TABLE IF NOT EXISTS article_embeddings USING vec0(
            article_id TEXT PRIMARY KEY,
            embedding FLOAT[384]
        )
    """)
    g.connection.commit()


def serialize_embedding(embedding: np.ndarray) -> bytes:
    """Serialize embedding to bytes for sqlite-vec."""
    return struct.pack(f"{len(embedding)}f", *embedding.astype(np.float32))


def ingest_hotpotqa(
    db_path: str = "hotpotqa.db",
    limit: int | None = None,
    verbose: bool = True,
):
    """
    Ingest HotpotQA dataset into GraphQLite.

    Args:
        db_path: Path to SQLite database
        limit: Max examples to load (None for full dataset)
        verbose: Print progress
    """
    if verbose:
        print("=" * 60)
        print("HotpotQA GraphRAG Ingestion")
        print("=" * 60)

    # Download and load dataset
    json_path = download_dataset(HOTPOTQA_DEV_DISTRACTOR, verbose=verbose)
    examples = load_dataset(json_path, limit=limit, verbose=verbose)

    if verbose:
        stats = get_dataset_stats(examples)
        print(f"\nDataset statistics:")
        print(f"  Questions: {stats['num_examples']}")
        print(f"  Unique articles: {stats['num_unique_articles']}")
        print(f"  Question types: {stats['question_types']}")

    # Remove existing db
    db_file = Path(db_path)
    if db_file.exists():
        db_file.unlink()
        if verbose:
            print(f"\nRemoved existing database: {db_path}")

    # Initialize graph
    if verbose:
        print(f"\nInitializing database: {db_path}")

    g = graph(db_path)
    load_vec_extension(g)

    # Load embedding model
    if verbose:
        print("\nLoading embedding model (all-MiniLM-L6-v2)...")
    embed_model = SentenceTransformer("all-MiniLM-L6-v2")

    # Collect unique articles
    if verbose:
        print("\nCollecting unique articles...")

    articles = {}  # title -> text
    for ex in examples:
        for para in ex.context:
            text = " ".join(para.sentences)
            if para.title not in articles:
                articles[para.title] = text
            else:
                if text not in articles[para.title]:
                    articles[para.title] += " " + text

    if verbose:
        print(f"  Found {len(articles)} unique articles")

    # Generate embeddings
    if verbose:
        print(f"\nGenerating embeddings for {len(articles)} articles...")

    titles = list(articles.keys())
    texts = list(articles.values())
    embeddings = embed_model.encode(
        texts,
        batch_size=64,
        show_progress_bar=False,
        convert_to_numpy=True,
        normalize_embeddings=True,
    )
    if verbose:
        print(f"  Embeddings generated: {embeddings.shape}")

    # Create Article nodes
    if verbose:
        print("\nCreating Article nodes...")

    sqlite_conn = g.connection.sqlite_connection

    for i, (title, text) in enumerate(zip(titles, texts)):
        # Use sanitized title as the node ID
        article_id = f"article:{make_safe_id(title)}"
        clean_text = text[:500].replace("\n", " ").replace("\r", " ")
        # Also sanitize text for node properties
        safe_text = make_safe_id(clean_text)

        g.upsert_node(
            article_id,
            {"title": make_safe_id(title), "text": safe_text},
            "Article"
        )

        # Store embedding (use original title for lookup)
        embedding_bytes = serialize_embedding(embeddings[i])
        sqlite_conn.execute(
            "INSERT INTO article_embeddings (article_id, embedding) VALUES (?, ?)",
            (article_id, embedding_bytes)
        )

        if verbose and (i + 1) % 5000 == 0:
            print(f"    Progress: {i + 1}/{len(titles)} articles")

    g.connection.commit()
    if verbose:
        print(f"  Created {len(titles)} Article nodes with embeddings")

    # Create Question nodes and relationships
    if verbose:
        print("\nCreating Question nodes and relationships...")

    question_count = 0
    context_edges = 0
    cooccur_set = set()  # Track unique co-occurrence pairs

    for i, ex in enumerate(examples):
        # Create Question node
        question_id = f"question:{ex.id}"
        g.upsert_node(
            question_id,
            {
                "question_id": ex.id,
                "text": make_safe_id(ex.question),
                "answer": make_safe_id(ex.answer),
                "question_type": ex.question_type,
                "level": ex.level,
            },
            "Question"
        )
        question_count += 1

        # Link Question to its context Articles
        context_titles = [p.title for p in ex.context]
        supporting_titles = [sf.title for sf in ex.supporting_facts]

        for title in context_titles:
            article_id = f"article:{make_safe_id(title)}"
            is_supporting = title in supporting_titles

            g.upsert_edge(
                question_id,
                article_id,
                {"is_supporting": is_supporting},
                "HAS_CONTEXT"
            )
            context_edges += 1

        # Create COOCCURS edges between articles in same context
        for j, title1 in enumerate(context_titles):
            for title2 in context_titles[j+1:]:
                # Create canonical edge pair (sorted to avoid duplicates)
                pair = tuple(sorted([title1, title2]))
                if pair not in cooccur_set:
                    cooccur_set.add(pair)
                    both_supporting = title1 in supporting_titles and title2 in supporting_titles
                    g.upsert_edge(
                        f"article:{make_safe_id(title1)}",
                        f"article:{make_safe_id(title2)}",
                        {"supporting_pair": both_supporting},
                        "COOCCURS"
                    )

        if verbose and (i + 1) % 2000 == 0:
            print(f"    Progress: {i + 1}/{len(examples)} questions")

    g.connection.commit()

    if verbose:
        print(f"  Created {question_count} Question nodes")
        print(f"  Created {context_edges} HAS_CONTEXT edges")
        print(f"  Created {len(cooccur_set)} COOCCURS edges")

    # Print summary
    if verbose:
        print("\n" + "=" * 60)
        print("Ingestion Complete")
        print("=" * 60)

        result = g.connection.cypher("MATCH (n:Article) RETURN count(n) AS cnt")
        article_count = int(result[0]["cnt"]) if result else 0

        result = g.connection.cypher("MATCH (n:Question) RETURN count(n) AS cnt")
        q_count = int(result[0]["cnt"]) if result else 0

        result = g.connection.cypher("MATCH ()-[r:HAS_CONTEXT]->() RETURN count(r) AS cnt")
        ctx_count = int(result[0]["cnt"]) if result else 0

        result = g.connection.cypher("MATCH ()-[r:COOCCURS]->() RETURN count(r) AS cnt")
        cooccur_count = int(result[0]["cnt"]) if result else 0

        print(f"\nGraph statistics:")
        print(f"  Article nodes: {article_count}")
        print(f"  Question nodes: {q_count}")
        print(f"  HAS_CONTEXT edges: {ctx_count}")
        print(f"  COOCCURS edges: {cooccur_count}")
        print(f"\nDatabase: {db_path}")
        print(f"Database size: {db_file.stat().st_size / 1024 / 1024:.1f} MB")

    g.close()


def main():
    parser = argparse.ArgumentParser(
        description="Ingest HotpotQA into GraphQLite for GraphRAG"
    )
    parser.add_argument(
        "--db",
        default="hotpotqa.db",
        help="Database path (default: hotpotqa.db)"
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=None,
        help="Limit number of examples (default: all ~7500)"
    )
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Quiet mode (less output)"
    )

    args = parser.parse_args()

    ingest_hotpotqa(
        db_path=args.db,
        limit=args.limit,
        verbose=not args.quiet,
    )


if __name__ == "__main__":
    main()
