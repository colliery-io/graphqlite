"""Shared parsing helpers for algorithm results."""

from typing import Any, Optional


def parse_score_result(row: dict, score_key: str = "score") -> Optional[dict]:
    """Parse a result row with node_id, user_id, and a score field."""
    node_id = row.get("node_id")
    user_id = row.get("user_id")
    score = row.get(score_key)

    if node_id is None:
        return None

    return {
        "node_id": str(node_id),
        "user_id": user_id,
        "score": float(score) if score is not None else 0.0
    }


def parse_community_result(row: dict) -> Optional[dict]:
    """Parse a community detection result row."""
    node_id = row.get("node_id")
    user_id = row.get("user_id")
    community = row.get("community")

    if node_id is None or community is None:
        return None

    return {
        "node_id": str(node_id),
        "user_id": user_id,
        "community": int(community) if community else 0
    }


def parse_component_result(row: dict) -> Optional[dict]:
    """Parse a connected components result row."""
    node_id = row.get("node_id")
    user_id = row.get("user_id")
    component = row.get("component")

    if node_id is None:
        return None

    return {
        "node_id": str(node_id),
        "user_id": user_id,
        "component": int(component) if component is not None else 0
    }


def parse_traversal_result(row: dict) -> Optional[dict]:
    """Parse a BFS/DFS traversal result row."""
    user_id = row.get("user_id")
    depth = row.get("depth")
    order = row.get("order")

    if user_id is None:
        return None

    return {
        "user_id": user_id,
        "depth": int(depth) if depth is not None else 0,
        "order": int(order) if order is not None else 0
    }


def safe_float(val: Any, default: float = 0.0) -> float:
    """Safely convert a value to float."""
    if val is None:
        return default
    try:
        return float(val)
    except (ValueError, TypeError):
        return default


def safe_int(val: Any, default: int = 0) -> int:
    """Safely convert a value to int."""
    if val is None:
        return default
    try:
        return int(val)
    except (ValueError, TypeError):
        return default
