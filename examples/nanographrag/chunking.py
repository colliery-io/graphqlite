"""
Text chunking utilities for GraphRAG document processing.

Splits documents into overlapping chunks suitable for:
- Entity extraction (smaller context windows)
- Vector embedding (fixed-size inputs)
- Retrieval (returning relevant passages)
"""

import re
import hashlib
from dataclasses import dataclass


@dataclass
class Chunk:
    """A text chunk with metadata."""
    chunk_id: str
    doc_id: str
    text: str
    start_char: int
    end_char: int
    chunk_index: int
    token_count: int  # Approximate


def estimate_tokens(text: str) -> int:
    """
    Estimate token count for text.

    Uses word count with ~1.3 multiplier (rough approximation for English).
    For exact counts, use tiktoken or a HuggingFace tokenizer.
    """
    words = len(text.split())
    return int(words * 1.3)


def chunk_text(
    text: str,
    chunk_size: int = 512,
    overlap: int = 50,
    doc_id: str = "doc",
    min_chunk_size: int = 50,
) -> list[Chunk]:
    """
    Split text into overlapping chunks.

    Args:
        text: The text to chunk
        chunk_size: Target chunk size in tokens (approximate)
        overlap: Number of tokens to overlap between chunks
        doc_id: Document identifier for chunk IDs
        min_chunk_size: Minimum chunk size (skip smaller trailing chunks)

    Returns:
        List of Chunk objects
    """
    if not text.strip():
        return []

    # Convert token targets to word estimates (tokens / 1.3)
    words_per_chunk = int(chunk_size / 1.3)
    words_overlap = int(overlap / 1.3)
    min_words = int(min_chunk_size / 1.3)

    # Split into sentences first for cleaner boundaries
    sentences = _split_sentences(text)

    chunks = []
    current_words = []
    current_word_count = 0
    chunk_index = 0
    char_offset = 0
    chunk_start_char = 0

    for sentence in sentences:
        sentence_words = sentence.split()
        sentence_word_count = len(sentence_words)

        # If adding this sentence exceeds chunk size, finalize current chunk
        if current_word_count + sentence_word_count > words_per_chunk and current_words:
            chunk_text_str = " ".join(current_words)
            chunk_end_char = chunk_start_char + len(chunk_text_str)

            chunks.append(Chunk(
                chunk_id=_make_chunk_id(doc_id, chunk_index),
                doc_id=doc_id,
                text=chunk_text_str,
                start_char=chunk_start_char,
                end_char=chunk_end_char,
                chunk_index=chunk_index,
                token_count=estimate_tokens(chunk_text_str),
            ))
            chunk_index += 1

            # Keep overlap words for next chunk
            overlap_words = current_words[-words_overlap:] if words_overlap > 0 else []
            current_words = overlap_words
            current_word_count = len(overlap_words)

            # Update start position (approximate)
            if overlap_words:
                overlap_text = " ".join(overlap_words)
                chunk_start_char = chunk_end_char - len(overlap_text)
            else:
                chunk_start_char = chunk_end_char + 1

        current_words.extend(sentence_words)
        current_word_count += sentence_word_count

    # Don't forget the last chunk
    if current_words and current_word_count >= min_words:
        chunk_text_str = " ".join(current_words)
        chunks.append(Chunk(
            chunk_id=_make_chunk_id(doc_id, chunk_index),
            doc_id=doc_id,
            text=chunk_text_str,
            start_char=chunk_start_char,
            end_char=chunk_start_char + len(chunk_text_str),
            chunk_index=chunk_index,
            token_count=estimate_tokens(chunk_text_str),
        ))

    return chunks


def chunk_documents(
    documents: list[dict],
    chunk_size: int = 512,
    overlap: int = 50,
    text_key: str = "text",
    id_key: str = "id",
) -> list[Chunk]:
    """
    Chunk multiple documents.

    Args:
        documents: List of dicts with text and id fields
        chunk_size: Target chunk size in tokens
        overlap: Token overlap between chunks
        text_key: Key for text content in document dicts
        id_key: Key for document ID in document dicts

    Returns:
        List of all Chunk objects from all documents
    """
    all_chunks = []

    for i, doc in enumerate(documents):
        text = doc.get(text_key, "")
        doc_id = doc.get(id_key, f"doc_{i}")

        chunks = chunk_text(
            text=text,
            chunk_size=chunk_size,
            overlap=overlap,
            doc_id=str(doc_id),
        )
        all_chunks.extend(chunks)

    return all_chunks


def _split_sentences(text: str) -> list[str]:
    """
    Split text into sentences.

    Uses a simple regex approach. For better accuracy,
    use spaCy's sentence segmentation.
    """
    # Handle common abbreviations to avoid false splits
    text = re.sub(r'\b(Mr|Mrs|Ms|Dr|Prof|Inc|Ltd|Corp|vs|etc)\.\s', r'\1<PERIOD> ', text)

    # Split on sentence-ending punctuation followed by space and capital
    sentences = re.split(r'(?<=[.!?])\s+(?=[A-Z])', text)

    # Restore abbreviation periods
    sentences = [s.replace('<PERIOD>', '.') for s in sentences]

    # Also split on newlines (paragraph boundaries)
    final_sentences = []
    for s in sentences:
        parts = [p.strip() for p in s.split('\n') if p.strip()]
        final_sentences.extend(parts)

    return final_sentences


def _make_chunk_id(doc_id: str, chunk_index: int) -> str:
    """Generate a stable chunk ID."""
    key = f"{doc_id}:chunk:{chunk_index}"
    return hashlib.md5(key.encode()).hexdigest()[:12]
