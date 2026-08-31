#!/usr/bin/env python3
"""Normalize + compare the parity runner outputs (#30 [V-01], N-way for #84).

Reads the JSON documents produced by run-ts.ts (the pivot), run_python.py, and
run rust `parity_runner`, normalizes them all with the *same* canonicalization
so cosmetic differences collapse (the bindings differ only in key casing — TS
camelCase vs Python/Rust snake_case), then compares every scenario/step. TS is
the subject-under-test: each other binding is diffed against TS as a pair
(TS↔Python, TS↔Rust). Genuine value or behavior divergences fail the gate
(exit 1) with a report naming the scenario, step, method, args, and both sides'
values. Intended divergences listed in allowlist.json are reported but do not
fail; each allowlist entry declares which binding *pairs* it applies to via its
`bindings` field (e.g. ["python-ts"], ["rust-ts"], or both).

Backward compatible: `--python X --ts Y` still runs the original 2-way check.
Add `--rust Z` to also run the TS↔Rust pair. At least one of --python/--rust is
required; --ts is always required (it is the pivot).

Normalization (results mode):
  * dict keys camelCase -> snake_case (recursively), then sorted
  * floats rounded to 1e-6
  * arrays canonically sorted (row order across bindings is not meaningful)

Cypher mode compares the ordered list of emitted Cypher calls (query string +
params) element-by-element (order IS meaningful there); param dicts are
normalized like values.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

_CAMEL_BOUNDARY = re.compile(r"(?<=[a-z0-9])(?=[A-Z])")
FLOAT_NDIGITS = 6


def to_snake(key: str) -> str:
    return _CAMEL_BOUNDARY.sub("_", key).lower()


def normalize(value, sort_lists: bool = True):
    """Canonicalize a value for cross-binding comparison."""
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        # Compare numbers by value, not representation: Python safe_float yields
        # 1.0 where JS safeFloat yields 1 for the same score. Coerce every
        # non-bool number to a rounded float so int-vs-float noise collapses
        # while genuine value differences still surface. (-0.0 -> 0.0.)
        return round(float(value), FLOAT_NDIGITS) + 0.0
    if isinstance(value, dict):
        norm = {to_snake(k): normalize(v, sort_lists) for k, v in value.items()}
        return dict(sorted(norm.items()))
    if isinstance(value, list):
        items = [normalize(v, sort_lists) for v in value]
        if sort_lists:
            items.sort(key=lambda x: json.dumps(x, sort_keys=True, ensure_ascii=False))
        return items
    return value


def canon(value) -> str:
    return json.dumps(value, sort_keys=True, ensure_ascii=False)


def comparable_results(step: dict) -> tuple:
    if step.get("status") == "error":
        err = step.get("error") or {}
        return ("error", err.get("type"))
    return ("ok", canon(normalize(step.get("value"))))


def comparable_cypher(step: dict) -> str:
    calls = step.get("cypher") or []
    # Order matters: normalize each call but preserve the call sequence.
    norm = [
        {"query": c.get("query"), "params": normalize(c.get("params"))}
        for c in calls
    ]
    return canon(norm)


def load_allowlist(path: str, mode: str, pair: str) -> dict:
    """Load allowlist entries that apply to this (mode, pair).

    Each entry may declare `bindings` — the list of binding pairs it applies to
    (e.g. ["python-ts", "rust-ts"]). An entry with no `bindings` field applies to
    every pair (backward compatible with pre-#84 allowlists).
    """
    data = json.loads(Path(path).read_text())
    allowed = {}
    for entry in data.get("allow", []):
        modes = entry.get("modes", ["results", "cypher"])
        bindings = entry.get("bindings")  # None => applies to all pairs
        if mode in modes and (bindings is None or pair in bindings):
            allowed[entry["id"]] = entry.get("reason", "")
    return allowed


def index_steps(doc: dict) -> dict:
    idx = {}
    for scenario in doc.get("scenarios", []):
        for step in scenario.get("steps", []):
            idx[(scenario["id"], step["id"])] = step
    return idx


def _load(path: str) -> dict:
    return json.loads(Path(path).read_text())


def compare_pair(ts_idx: dict, other_idx: dict, other_name: str, mode: str, allowlist: dict):
    """Diff the TS pivot against one other binding. Returns (matched, allowlisted, mismatches)."""
    all_keys = list(dict.fromkeys(list(ts_idx) + list(other_idx)))

    matched = 0
    allowlisted = 0
    mismatches = []

    for key in all_keys:
        scenario_id, step_id = key
        dotted = f"{scenario_id}.{step_id}"
        ts_step = ts_idx.get(key)
        other_step = other_idx.get(key)

        if ts_step is None or other_step is None:
            missing_side = "TS" if ts_step is None else other_name
            if dotted in allowlist:
                allowlisted += 1
                continue
            present = ts_step or other_step or {}
            mismatches.append({
                "scenario": scenario_id, "step": step_id,
                "method": present.get("method"),
                "args": present.get("args"),
                "reason": f"missing on {missing_side} side",
                "ts": ts_step, "other": other_step,
            })
            continue

        if mode == "cypher":
            ts_cmp = comparable_cypher(ts_step)
            other_cmp = comparable_cypher(other_step)
        else:
            ts_cmp = comparable_results(ts_step)
            other_cmp = comparable_results(other_step)

        if ts_cmp == other_cmp:
            matched += 1
            continue

        if dotted in allowlist:
            allowlisted += 1
            continue

        mismatches.append({
            "scenario": scenario_id, "step": step_id,
            "method": ts_step.get("method"), "args": ts_step.get("args"),
            "ts": _display(ts_step, mode),
            "other": _display(other_step, mode),
        })

    return matched, allowlisted, mismatches


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare TS vs Python/Rust parity output (N-way)")
    parser.add_argument("--ts", required=True, help="TS runner output (pivot / subject-under-test)")
    parser.add_argument("--python", help="Python runner output (optional)")
    parser.add_argument("--rust", help="Rust runner output (optional)")
    parser.add_argument("--mode", choices=["results", "cypher"], default="results")
    parser.add_argument(
        "--allowlist",
        default=str(Path(__file__).with_name("allowlist.json")),
    )
    args = parser.parse_args()

    ts_idx = index_steps(_load(args.ts))

    others = []
    if args.python:
        others.append(("python", index_steps(_load(args.python))))
    if args.rust:
        others.append(("rust", index_steps(_load(args.rust))))

    if not others:
        print("ERROR: at least one of --python / --rust must be provided.", file=sys.stderr)
        return 2

    overall_fail = False
    for name, other_idx in others:
        pair = f"{name}-ts"
        allowlist = load_allowlist(args.allowlist, args.mode, pair)
        matched, allowlisted, mismatches = compare_pair(ts_idx, other_idx, name, args.mode, allowlist)
        _report(args.mode, name, pair, matched, allowlisted, mismatches, allowlist)
        if mismatches:
            overall_fail = True

    return 1 if overall_fail else 0


def _display(step: dict, mode: str):
    if mode == "cypher":
        return step.get("cypher")
    if step.get("status") == "error":
        return {"error": step.get("error")}
    return {"value": normalize(step.get("value"))}


def _report(mode, other_name, pair, matched, allowlisted, mismatches, allowlist) -> None:
    print("=" * 72)
    print(f"GraphQLite parity report — pair: {pair} (TS vs {other_name}) — mode: {mode}")
    print("=" * 72)
    print(f"  matched (identical)   : {matched}")
    print(f"  allowlisted (skipped) : {allowlisted}")
    print(f"  mismatches (failures) : {len(mismatches)}")
    if allowlist:
        print(f"  allowlist entries active for {pair} / {mode}:")
        for k, reason in allowlist.items():
            print(f"    - {k}: {reason}")
    if not mismatches:
        print(f"\nRESULT ({pair}): PASS — all scenarios agree (allowlisted divergences aside).")
        return
    print(f"\nRESULT ({pair}): FAIL — divergences found:\n")
    for m in mismatches:
        print("-" * 72)
        print(f"  scenario : {m['scenario']}")
        print(f"  step     : {m['step']}")
        print(f"  method   : {m.get('method')}")
        print(f"  input    : {json.dumps(m.get('args'), ensure_ascii=False)}")
        if "reason" in m:
            print(f"  reason   : {m['reason']}")
        print(f"  ts       : {json.dumps(m.get('ts'), ensure_ascii=False)}")
        print(f"  {other_name:<8} : {json.dumps(m.get('other'), ensure_ascii=False)}")
    print("-" * 72)


if __name__ == "__main__":
    raise SystemExit(main())
