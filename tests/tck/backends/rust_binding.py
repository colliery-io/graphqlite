"""
Backend adapter for the Rust binding.

Drives `bindings/rust/examples/tck_runner` — a long-lived REPL that speaks
one JSON object per line:

    request:  {"cmd": "reset"}
    request:  {"cmd": "execute", "query": "..."}
    response: {"ok": bool, "columns": [...], "rows": [...]} or {"ok": false, "error": "..."}
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

from .base import Backend, BackendError, QueryResult


RUNNER_REL = Path("bindings/rust/target/debug/examples/tck_runner")


class RustBindingBackend(Backend):
    name = "rust"

    def __init__(self, runner: Path | None = None, debug_log: Path | None = None):
        path = runner or RUNNER_REL
        if not path.exists():
            raise BackendError(
                f"rust tck_runner not found at {path}. Build with: "
                f"cargo build --example tck_runner --manifest-path bindings/rust/Cargo.toml"
            )
        self._debug_log = debug_log
        stderr_target = open(self._debug_log, "ab", buffering=0) if self._debug_log else subprocess.DEVNULL
        # Start the long-lived subprocess.
        self._proc = subprocess.Popen(
            [str(path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=stderr_target,
            text=True,
            bufsize=1,
        )
        self._stderr_sink = stderr_target if self._debug_log else None

    def reset(self) -> None:
        self._send({"cmd": "reset"})

    def load_named_graph(self, name: str) -> None:
        cyp = Path("vendor/tck/graphs") / name / "cypher.cyp"
        if not cyp.exists():
            raise BackendError(f"named graph not found: {name!r}")
        for stmt in (s.strip() for s in cyp.read_text(encoding="utf-8").split(";")):
            if stmt:
                self.execute(stmt)

    def execute(self, query: str, parameters: dict[str, Any] | None = None) -> QueryResult:
        # parameters are currently dropped; see TCK-06 triage. v1 contract is
        # equivalent to the extension backend.
        resp = self._send({"cmd": "execute", "query": query})
        if not resp.get("ok"):
            return QueryResult(error="RustBindingError", error_message=resp.get("error", ""))
        cols = list(resp.get("columns") or [])
        rows = []
        for record in resp.get("rows") or []:
            if cols:
                rows.append([record.get(c) for c in cols])
            else:
                rows.append([record])
        return QueryResult(headers=cols, rows=rows)

    def close(self) -> None:
        if self._proc.poll() is None:
            try:
                self._send({"cmd": "shutdown"})
            except BackendError:
                pass
            try:
                self._proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait()
        if self._stderr_sink:
            self._stderr_sink.close()

    def _send(self, msg: dict[str, Any]) -> dict[str, Any]:
        if self._proc.stdin is None or self._proc.stdout is None:
            raise BackendError("rust runner pipes closed")
        try:
            self._proc.stdin.write(json.dumps(msg) + "\n")
            self._proc.stdin.flush()
        except BrokenPipeError as e:
            raise BackendError(f"rust runner died: {e}") from e
        line = self._proc.stdout.readline()
        if not line:
            raise BackendError("rust runner closed stdout unexpectedly")
        try:
            return json.loads(line)
        except json.JSONDecodeError as e:
            raise BackendError(f"rust runner emitted non-JSON: {line!r} ({e})") from e
