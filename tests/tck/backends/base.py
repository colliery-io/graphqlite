"""Backend adapter interface."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Protocol


@dataclass
class QueryResult:
    headers: list[str] = field(default_factory=list)
    rows: list[list[Any]] = field(default_factory=list)   # rows of parsed values
    error: str | None = None                              # error category if raised
    error_message: str | None = None
    side_effects: dict[str, int] = field(default_factory=dict)


class BackendError(RuntimeError):
    pass


class Backend(Protocol):
    """All backend adapters implement this surface."""

    name: str

    def reset(self) -> None: ...
    def load_named_graph(self, name: str) -> None: ...
    def execute(self, query: str, parameters: dict[str, Any] | None = None) -> QueryResult: ...
    def close(self) -> None: ...
