"""Base mixin with connection and utility methods."""

from typing import Any

from ..utils import escape_string, format_props


class BaseMixin:
    """Base mixin providing connection access and utility methods."""

    # These will be set by the main Graph class
    _conn: Any
    namespace: str

    def _escape(self, s: str) -> str:
        """Escape a string for Cypher queries."""
        return escape_string(s)

    def _format_props(self, props: dict[str, Any]) -> str:
        """Format a properties dict as a Cypher property string."""
        return format_props(props, self._escape)
