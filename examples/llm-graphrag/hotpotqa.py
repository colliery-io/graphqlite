"""
HotpotQA dataset loader.

HotpotQA is a multi-hop question answering dataset where questions
require reasoning across multiple Wikipedia paragraphs.

Dataset: https://hotpotqa.github.io/
"""

import json
import httpx
from dataclasses import dataclass
from pathlib import Path


# Dataset URLs
HOTPOTQA_DEV_DISTRACTOR = "http://curtis.ml.cmu.edu/datasets/hotpot/hotpot_dev_distractor_v1.json"
HOTPOTQA_DEV_FULLWIKI = "http://curtis.ml.cmu.edu/datasets/hotpot/hotpot_dev_fullwiki_v1.json"
HOTPOTQA_TRAIN = "http://curtis.ml.cmu.edu/datasets/hotpot/hotpot_train_v1.1.json"


@dataclass
class Paragraph:
    """A Wikipedia paragraph from HotpotQA."""
    title: str          # Wikipedia article title
    sentences: list[str]  # List of sentences


@dataclass
class SupportingFact:
    """A supporting fact for answering a question."""
    title: str      # Article title
    sentence_idx: int  # Which sentence (0-indexed)


@dataclass
class HotpotExample:
    """A single HotpotQA example."""
    id: str
    question: str
    answer: str
    question_type: str  # "bridge" or "comparison"
    level: str          # "easy", "medium", "hard"
    context: list[Paragraph]
    supporting_facts: list[SupportingFact]


def download_dataset(
    url: str = HOTPOTQA_DEV_DISTRACTOR,
    cache_dir: Path | str = "data",
    verbose: bool = True,
) -> Path:
    """
    Download HotpotQA dataset if not cached.

    Args:
        url: Dataset URL
        cache_dir: Directory to cache downloaded file
        verbose: Print progress

    Returns:
        Path to downloaded JSON file
    """
    cache_dir = Path(cache_dir)
    cache_dir.mkdir(exist_ok=True)

    # Extract filename from URL
    filename = url.split("/")[-1]
    cache_path = cache_dir / filename

    if cache_path.exists():
        if verbose:
            print(f"Using cached: {cache_path}")
        return cache_path

    if verbose:
        print(f"Downloading {filename}...")
        print(f"  URL: {url}")

    # Download with progress
    with httpx.stream("GET", url, follow_redirects=True, timeout=300.0) as response:
        response.raise_for_status()
        total = int(response.headers.get("content-length", 0))

        with open(cache_path, "wb") as f:
            downloaded = 0
            for chunk in response.iter_bytes(chunk_size=8192):
                f.write(chunk)
                downloaded += len(chunk)
                if verbose and total:
                    pct = (downloaded / total) * 100
                    print(f"\r  Progress: {pct:.1f}%", end="", flush=True)

    if verbose:
        print(f"\n  Saved to: {cache_path}")

    return cache_path


def load_dataset(
    path: Path | str,
    limit: int | None = None,
    verbose: bool = True,
) -> list[HotpotExample]:
    """
    Load HotpotQA examples from JSON file.

    Args:
        path: Path to JSON file
        limit: Max examples to load (None for all)
        verbose: Print progress

    Returns:
        List of HotpotExample objects
    """
    if verbose:
        print(f"Loading dataset from {path}...")

    with open(path) as f:
        data = json.load(f)

    if limit:
        data = data[:limit]

    examples = []
    for item in data:
        # Parse context paragraphs
        context = []
        for title, sentences in item.get("context", []):
            context.append(Paragraph(title=title, sentences=sentences))

        # Parse supporting facts
        supporting_facts = []
        for title, sent_idx in item.get("supporting_facts", []):
            supporting_facts.append(SupportingFact(title=title, sentence_idx=sent_idx))

        examples.append(HotpotExample(
            id=item["_id"],
            question=item["question"],
            answer=item["answer"],
            question_type=item.get("type", "unknown"),
            level=item.get("level", "unknown"),
            context=context,
            supporting_facts=supporting_facts,
        ))

    if verbose:
        print(f"  Loaded {len(examples)} examples")

    return examples


def get_supporting_text(example: HotpotExample) -> list[str]:
    """
    Get the actual supporting sentences for an example.

    Returns list of sentences that contain the answer evidence.
    """
    sentences = []

    # Build lookup from title to paragraph
    title_to_para = {p.title: p for p in example.context}

    for fact in example.supporting_facts:
        para = title_to_para.get(fact.title)
        if para and 0 <= fact.sentence_idx < len(para.sentences):
            sentences.append(para.sentences[fact.sentence_idx])

    return sentences


def get_unique_titles(examples: list[HotpotExample]) -> set[str]:
    """Get all unique Wikipedia article titles from examples."""
    titles = set()
    for ex in examples:
        for para in ex.context:
            titles.add(para.title)
    return titles


def get_dataset_stats(examples: list[HotpotExample]) -> dict:
    """Get statistics about the dataset."""
    titles = get_unique_titles(examples)

    # Count question types
    types = {}
    levels = {}
    for ex in examples:
        types[ex.question_type] = types.get(ex.question_type, 0) + 1
        levels[ex.level] = levels.get(ex.level, 0) + 1

    return {
        "num_examples": len(examples),
        "num_unique_articles": len(titles),
        "question_types": types,
        "difficulty_levels": levels,
    }


if __name__ == "__main__":
    # Quick test
    print("Testing HotpotQA loader...\n")

    # Download dev set (distractor version - smaller)
    path = download_dataset(HOTPOTQA_DEV_DISTRACTOR)

    # Load a small sample
    examples = load_dataset(path, limit=100)

    # Show stats
    stats = get_dataset_stats(examples)
    print(f"\n--- Dataset Stats ---")
    print(f"  Examples: {stats['num_examples']}")
    print(f"  Unique articles: {stats['num_unique_articles']}")
    print(f"  Question types: {stats['question_types']}")
    print(f"  Difficulty: {stats['difficulty_levels']}")

    # Show sample
    print(f"\n--- Sample Example ---")
    ex = examples[0]
    print(f"  Question: {ex.question}")
    print(f"  Answer: {ex.answer}")
    print(f"  Type: {ex.question_type} ({ex.level})")
    print(f"  Context articles: {[p.title for p in ex.context]}")
    print(f"\n  Supporting facts:")
    for sent in get_supporting_text(ex):
        print(f"    - {sent[:100]}...")
