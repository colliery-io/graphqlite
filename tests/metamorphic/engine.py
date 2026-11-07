"""Run Cypher on a fresh in-memory database and normalise results."""

from __future__ import annotations

import json
import sqlite3
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Outcome:
    ok: bool
    rows: list | None = None      # read result: list of row dicts
    error: str | None = None

    def key(self) -> str:
        """A comparable form: errors by message, rows as a sorted multiset."""
        if not self.ok:
            return "ERROR: " + (self.error or "")
        return json.dumps(sorted(canon_row(r) for r in self.rows))


def _canon(v):
    """Canonical value: lists sorted (collect order is unspecified),
    floats rounded, maps by key."""
    if isinstance(v, list):
        return sorted((_canon(x) for x in v), key=lambda x: json.dumps(x, sort_keys=True))
    if isinstance(v, dict):
        return {k: _canon(x) for k, x in sorted(v.items())}
    if isinstance(v, float):
        return round(v, 9)
    return v


def canon_row(row: dict) -> str:
    return json.dumps(_canon(row), sort_keys=True)


def _float_constant(name: str) -> float:
    """A result writes an infinite or NaN float as the bare token Infinity,
    -Infinity or NaN (GQLITE-T-0484). json.loads calls this only for those
    three words, so any other spelling (Inf.0, inf.0) is still an invalid
    result."""
    return float(name)


class Engine:
    def __init__(self, ext: Path):
        self.ext = str(ext)

    def fresh(self) -> sqlite3.Connection:
        conn = sqlite3.connect(":memory:")
        conn.enable_load_extension(True)
        conn.load_extension(self.ext)
        conn.enable_load_extension(False)
        return conn

    @staticmethod
    def storage_problems(conn: sqlite3.Connection) -> str | None:
        """Violations of the stored graph (gql_check_storage, GQLITE-T-0525)
        and of its constraints, or None when consistent. None also when the
        extension has no checker (an older build given with --ext)."""
        try:
            v = conn.execute("SELECT gql_check_storage()").fetchone()[0]
        except sqlite3.OperationalError as e:
            if "no such function" in str(e):
                return None
            return f"storage check failed: {e}"
        if v and v != "[]":
            return f"storage inconsistent: {v}"
        try:
            conn.execute("SELECT gql_check_constraints()").fetchone()
        except sqlite3.OperationalError as e:
            if "no such function" not in str(e):
                return f"constraint violated: {e}"
        return None

    @staticmethod
    def run(conn: sqlite3.Connection, query: str) -> Outcome:
        if query.startswith("SQL:"):         # BEGIN / ROLLBACK around writes (GQLITE-T-0526)
            try:
                conn.execute(query[4:])
            except sqlite3.Error as e:
                return Outcome(False, error=str(e))
            return Outcome(True, rows=[])
        try:
            raw = conn.execute("SELECT cypher(?)", (query,)).fetchone()[0]
        except sqlite3.Error as e:
            msg = str(e)
            try:
                msg = json.loads(msg).get("error", msg)
            except (ValueError, AttributeError):
                pass
            return Outcome(False, error=msg)
        if raw is None:
            return Outcome(True, rows=[])
        try:
            data = json.loads(raw, parse_constant=_float_constant)
        except ValueError as e:
            return Outcome(False, error=f"invalid JSON result: {e}: {raw[:200]}")
        if isinstance(data, dict):          # write statistics
            return Outcome(True, rows=[])
        return Outcome(True, rows=data)
