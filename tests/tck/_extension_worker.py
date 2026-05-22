"""
Long-lived worker that hosts the GraphQLite SQLite extension in a separate
process so a crash in the extension does not kill the whole harness.

Protocol (one JSON object per line on stdin, one JSON object per line on stdout):

    request:  {"cmd": "reset"}                      -> {"ok": true}
    request:  {"cmd": "execute", "query": "..."}    -> {"ok": true, "rows": ["..."]}
                                                       (rows are the raw cypher()
                                                        return payloads — the
                                                        comparator does its own
                                                        decoding)
                                                    -> {"ok": false, "error_class": "...",
                                                        "error_message": "..."}
    request:  {"cmd": "shutdown"}                   -> {"ok": true}, then exit

The harness's `ExtensionBackend` supervises this worker and respawns it on
unclean exit (SIGSEGV, etc.), marking the in-flight scenario as `error`.
"""

from __future__ import annotations

import json
import os
import resource
import signal
import sqlite3
import sys
import traceback
from pathlib import Path


DEFAULT_EXT = Path("build/graphqlite")


def _suppress_crash_reporter() -> None:
    """Stop the OS crash reporter from spawning a dialog for every
    extension SIGABRT/SIGSEGV. The harness already catches the worker
    death and records it as `ExtensionCrash`; the kernel-level crash
    log is just noise (and on macOS it pops a 'graphqlite crashed'
    notification per scenario, which makes iterative testing painful).

    Two layers of defense:
    1. Disable core dumps via RLIMIT_CORE — keeps disk + Crash Reporter
       cycles down.
    2. Re-raise SIGABRT as exit(134) via a signal handler — when the
       extension calls abort(), Python catches the signal first and
       exits cleanly with the conventional 128+SIGABRT code. macOS
       won't dispatch the abort to Crash Reporter because we already
       exited."""
    try:
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    except (ValueError, OSError):
        pass
    def _die(signum, _frame):
        os._exit(128 + signum)
    for sig in (signal.SIGABRT, signal.SIGSEGV, signal.SIGBUS, signal.SIGFPE, signal.SIGILL):
        try:
            signal.signal(sig, _die)
        except (ValueError, OSError):
            pass


def main() -> int:
    _suppress_crash_reporter()
    ext_path = Path(os.environ.get("GQLITE_TCK_EXT", str(DEFAULT_EXT)))
    debug_log = os.environ.get("GQLITE_TCK_DEBUG_LOG")

    # Always redirect fds 1 and 2 away from the protocol channel: the
    # GRAPHQLITE_DEBUG-compiled extension prints to stdout via CYPHER_DEBUG
    # and to stderr from various places. If we left fd 1 inherited, that
    # debug noise would corrupt the JSON protocol the parent reads.
    if debug_log:
        Path(debug_log).parent.mkdir(parents=True, exist_ok=True)
        log_fd = os.open(debug_log, os.O_WRONLY | os.O_APPEND | os.O_CREAT)
    else:
        log_fd = os.open(os.devnull, os.O_WRONLY)
    protocol_out = os.dup(1)
    os.dup2(log_fd, 1)
    os.dup2(log_fd, 2)
    os.close(log_fd)
    proto = os.fdopen(protocol_out, "w", buffering=1)

    conn = _new_conn(ext_path)

    for raw in sys.stdin:
        line = raw.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            _write(proto, {"ok": False, "error_class": "ProtocolError", "error_message": str(e)})
            continue

        cmd = req.get("cmd")
        try:
            if cmd == "reset":
                conn.close()
                conn = _new_conn(ext_path)
                _write(proto, {"ok": True})
            elif cmd == "execute":
                params = req.get("parameters")
                if params:
                    params_json = json.dumps(params)
                    raw = conn.execute("SELECT cypher(?, ?)",
                                       (req.get("query", ""), params_json)).fetchall()
                else:
                    raw = conn.execute("SELECT cypher(?)", (req.get("query", ""),)).fetchall()
                _write(proto, _decode_payload([r[0] for r in raw]))
            elif cmd == "shutdown":
                conn.close()
                _write(proto, {"ok": True})
                return 0
            else:
                _write(proto, {"ok": False, "error_class": "ProtocolError",
                               "error_message": f"unknown cmd: {cmd!r}"})
        except sqlite3.Error as e:
            _write(proto, {"ok": False, "error_class": _classify(str(e)),
                           "error_message": str(e)})
        except Exception:
            _write(proto, {"ok": False, "error_class": "WorkerError",
                           "error_message": traceback.format_exc()})

    return 0


def _decode_payload(raw_payloads: list) -> dict:
    """
    Decode the extension's `cypher()` return into the harness wire format:

        {"ok": True, "columns": [...], "rows": [[v0, v1, ...], ...]}

    `cypher()` returns one SQLite row whose single TEXT column is one of:
      - a JSON array of column-keyed objects: '[{"name":"Alice"}, ...]'  → result set
      - a status string: 'Query executed successfully - nodes created: N, ...'
        (treat as 0 data rows; side-effect counters reported in `status`)
      - the literal '[]' for empty result sets
      - a JSON error object: '{"error":"...","code":"..."}'  → surfaced upstream
    """
    if not raw_payloads:
        return {"ok": True, "columns": [], "rows": []}

    # `cypher()` always returns exactly one row, but defend against future
    # multi-row returns by concatenating their decoded forms.
    columns: list = []
    rows: list = []
    status: str | None = None

    for payload in raw_payloads:
        if payload is None:
            continue
        text = payload if isinstance(payload, str) else str(payload)
        text_stripped = text.strip()
        # Try JSON first.
        try:
            decoded = json.loads(text_stripped) if text_stripped else None
        except json.JSONDecodeError:
            decoded = None

        if decoded is None or (isinstance(decoded, str)):
            # Not JSON — treat as a status / mutation summary, not data.
            status = text
            continue

        if isinstance(decoded, list):
            # Result set: list of {col: value} objects.
            for item in decoded:
                if isinstance(item, dict):
                    if not columns:
                        columns = list(item.keys())
                    rows.append([item.get(c) for c in columns])
                else:
                    # Defensive: a scalar in the list — keep as single-cell row.
                    if not columns:
                        columns = ["value"]
                    rows.append([item])
        elif isinstance(decoded, dict) and "error" in decoded:
            return {"ok": False,
                    "error_class": _classify(decoded.get("code", "")) or "GraphQLiteError",
                    "error_message": decoded.get("error", text)}
        elif isinstance(decoded, dict):
            # Single-row object.
            if not columns:
                columns = list(decoded.keys())
            rows.append([decoded.get(c) for c in columns])

    out = {"ok": True, "columns": columns, "rows": rows}
    if status:
        out["status"] = status
    return out


def _new_conn(ext_path: Path) -> sqlite3.Connection:
    conn = sqlite3.connect(":memory:")
    conn.enable_load_extension(True)
    conn.load_extension(str(ext_path))
    _install_deterministic_random(conn)
    return conn


def _install_deterministic_random(conn: sqlite3.Connection) -> None:
    """Override SQLite's built-in random() with a deterministic LCG so
    that TCK scenarios using `rand()` (which transforms to
    `ABS(RANDOM())/...`) produce the same sequence every run.

    Several openCypher Quantifier and Comprehension scenarios test
    tautologies of the form `any(P) = NOT all(NOT P)` while feeding
    `rand()`-shuffled input lists. They're meant to be invariant
    regardless of input — but a real bug in our implementation
    surfaces only on certain inputs, so the flake hides it. Making
    rand() deterministic per-connection turns the flake into a
    consistent signal: either the test now consistently passes (no
    real bug for this seed) or consistently fails (and we can
    investigate the deterministic counterexample).

    Seed is fixed; sequence resets when the harness creates a new
    connection (i.e. between scenarios). LCG constants from Numerical
    Recipes — full int64 cycle, no zero output."""
    state = [0x5DEECE66D9F37C45]  # arbitrary fixed seed

    def _random() -> int:
        state[0] = (state[0] * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF
        # SQLite's random() returns a signed 64-bit; map our unsigned state
        # into the signed range so ABS(RANDOM()) behaves as the extension
        # expects.
        v = state[0]
        return v - (1 << 64) if v >= (1 << 63) else v

    conn.create_function("random", 0, _random, deterministic=True)


def _named_graph_file(name: str) -> Path | None:
    """TCK named-graph fixture: vendor/tck/graphs/<name>/<name>.cypher (preferred)
    falls back to cypher.cyp for legacy layout."""
    root = Path("vendor/tck/graphs") / name
    for candidate in (root / f"{name}.cypher", root / "cypher.cyp"):
        if candidate.exists():
            return candidate
    return None


def _write(proto, msg: dict) -> None:
    proto.write(json.dumps(msg) + "\n")
    proto.flush()


def _classify(msg: str) -> str:
    lower = (msg or "").lower()
    if "parse" in lower or "syntax" in lower:
        return "SyntaxError"
    if "type" in lower:
        return "TypeError"
    if "not found" in lower:
        return "EntityNotFound"
    if "constraint" in lower:
        return "ConstraintViolation"
    if "not_implemented" in lower:
        return "NotImplementedError"
    if "execution" in lower:
        return "GraphQLiteError"
    return "GraphQLiteError"


if __name__ == "__main__":
    sys.exit(main())
