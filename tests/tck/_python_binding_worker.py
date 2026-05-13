"""
Long-lived worker that drives the GraphQLite Python binding in its own
process, so a hot loop / segfault in the underlying extension is killable
by the supervisor without taking down the whole harness.

Protocol (matches `_extension_worker.py` / `tck_runner.rs`):
    {"cmd": "reset"}                  -> {"ok": true}
    {"cmd": "execute", "query": "…"}  -> {"ok": true, "columns": [...], "rows": [{...}]}
                                         {"ok": false, "error_class": "...", "error_message": "..."}
    {"cmd": "shutdown"}               -> {"ok": true}; exits
"""

from __future__ import annotations

import json
import os
import sys
import traceback
from pathlib import Path


def main() -> int:
    # Make a checkout's binding importable without pip install.
    repo_root = Path(__file__).resolve().parents[2]
    src = repo_root / "bindings" / "python" / "src"
    if src.exists() and str(src) not in sys.path:
        sys.path.insert(0, str(src))

    try:
        import graphqlite  # noqa: F401
    except ImportError as e:
        _bootstrap_error(f"python binding not importable: {e}")
        return 1

    debug_log = os.environ.get("GQLITE_TCK_DEBUG_LOG")
    if debug_log:
        Path(debug_log).parent.mkdir(parents=True, exist_ok=True)
        log_fd = os.open(debug_log, os.O_WRONLY | os.O_APPEND | os.O_CREAT)
        protocol_out = os.dup(1)
        os.dup2(log_fd, 2)
        os.dup2(log_fd, 1)
        os.close(log_fd)
        proto = os.fdopen(protocol_out, "w", buffering=1)
    else:
        proto = sys.stdout

    gq = __import__("graphqlite")
    conn = gq.connect(":memory:")

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
                conn = gq.connect(":memory:")
                _write(proto, {"ok": True})
            elif cmd == "execute":
                res = conn.cypher(req.get("query", ""), req.get("parameters") or None)
                cols = list(res.columns) if hasattr(res, "columns") else []
                rows = [dict(rec) if not isinstance(rec, dict) else rec for rec in res]
                _write(proto, {"ok": True, "columns": cols, "rows": rows})
            elif cmd == "shutdown":
                conn.close()
                _write(proto, {"ok": True})
                return 0
            else:
                _write(proto, {"ok": False, "error_class": "ProtocolError",
                               "error_message": f"unknown cmd: {cmd!r}"})
        except Exception as e:
            _write(proto, {"ok": False, "error_class": type(e).__name__,
                           "error_message": str(e) or traceback.format_exc()})
    return 0


def _write(proto, msg: dict) -> None:
    proto.write(json.dumps(msg, default=str) + "\n")
    proto.flush()


def _bootstrap_error(msg: str) -> None:
    sys.stdout.write(json.dumps({"ok": False, "error_class": "BootstrapError",
                                  "error_message": msg}) + "\n")
    sys.stdout.flush()


if __name__ == "__main__":
    sys.exit(main())
