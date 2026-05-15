"""CLI entry point for the TCK harness.

Usage:
    python -m tests.tck [--features DIR] [--filter SUB] [--backend B]
                        [--out FILE] [--debug] [--limit N]
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import sys
from collections import Counter
from pathlib import Path

from .backends.extension import ExtensionBackend
from .gherkin import walk_features
from .runner import run_feature, ScenarioOutcome


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.tck")
    p.add_argument("--features", default="vendor/tck/features", type=Path)
    p.add_argument("--filter", default=None, help="substring filter on feature file path")
    p.add_argument("--backend", default="extension",
                   choices=["extension", "python", "rust", "all"])
    p.add_argument("--out", default="build/tck-results.json", type=Path)
    p.add_argument("--parity-out", default="build/tck-parity.json", type=Path)
    p.add_argument("--debug", action="store_true",
                   help="don't redirect extension stdout/stderr to a log file")
    p.add_argument("--limit", type=int, default=None,
                   help="stop after running N scenarios per backend (smoke test)")
    p.add_argument("--quiet", action="store_true",
                   help="suppress per-feature progress lines")
    args = p.parse_args(argv)

    backends = _select_backends(args.backend, debug=args.debug)
    if not backends:
        print(f"no backend available for {args.backend!r}", file=sys.stderr)
        return 2

    outcomes: list[ScenarioOutcome] = []
    scanned_per_backend: dict[str, int] = {b.name: 0 for b in backends}
    feature_count = 0
    for feat in walk_features(args.features):
        if args.filter and args.filter not in str(feat.path):
            continue
        feature_count += 1
        feature_outcomes: list[ScenarioOutcome] = []
        for backend in backends:
            if args.limit is not None and scanned_per_backend[backend.name] >= args.limit:
                continue
            results = run_feature(feat, backend)
            if args.limit is not None:
                remaining = args.limit - scanned_per_backend[backend.name]
                results = results[:remaining]
            outcomes.extend(results)
            feature_outcomes.extend(results)
            scanned_per_backend[backend.name] += len(results)
        if not args.quiet and feature_outcomes:
            counts = Counter(o.status for o in feature_outcomes)
            rel = _rel_to_features(feat.path, args.features)
            print(f"[{feature_count:3d}] {rel:65s}  "
                  f"pass={counts['pass']:3d} fail={counts['fail']:3d} "
                  f"error={counts['error']:3d} skipped={counts['skipped']:3d}",
                  flush=True)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps([dataclasses.asdict(o) for o in outcomes], indent=2, default=_json_default))

    if len(backends) > 1:
        parity = _build_parity_matrix(outcomes, [b.name for b in backends])
        args.parity_out.write_text(json.dumps(parity, indent=2, default=_json_default))

    _print_summary(outcomes)
    if len(backends) > 1:
        _print_parity_summary(parity)

    for b in backends:
        b.close()
    return 0


def _select_backends(name: str, debug: bool):
    # Only opt into a debug log when --debug is passed. Default runs send
    # worker stdout/stderr to /dev/null — the GRAPHQLITE_DEBUG-compiled
    # extension emits dozens of lines per query and was producing hundreds
    # of GB of debug output across a full TCK run.
    debug_log = Path("build/tck-debug.log") if debug else None
    out = []
    if name in ("extension", "all"):
        out.append(ExtensionBackend(debug_log=debug_log, keep_debug=debug))
    if name in ("python", "all"):
        try:
            from .backends.python_binding import PythonBindingBackend
            out.append(PythonBindingBackend(debug_log=debug_log, keep_debug=debug))
        except Exception as e:
            print(f"backend not implemented: python ({e})", file=sys.stderr)
            if name == "python":
                return []
    if name in ("rust", "all"):
        try:
            from .backends.rust_binding import RustBindingBackend
            out.append(RustBindingBackend(debug_log=debug_log))
        except Exception as e:
            print(f"backend not implemented: rust ({e})", file=sys.stderr)
            if name == "rust":
                return []
    return out


def _print_summary(outcomes: list[ScenarioOutcome]) -> None:
    by_backend: dict[str, Counter] = {}
    for o in outcomes:
        by_backend.setdefault(o.backend, Counter())[o.status] += 1
    for backend, counts in by_backend.items():
        total = sum(counts.values())
        print(f"TCK [{backend}]: {total} scenarios — "
              f"pass={counts['pass']} fail={counts['fail']} "
              f"error={counts['error']} skipped={counts['skipped']}")
    # Top-10 failing feature files (across all backends combined).
    by_file: Counter[str] = Counter()
    for o in outcomes:
        if o.status in ("fail", "error"):
            by_file[o.feature_file] += 1
    if by_file:
        print("Top failing feature files:")
        for path, n in by_file.most_common(10):
            print(f"  {n:4d}  {path}")


def _build_parity_matrix(outcomes: list[ScenarioOutcome], backend_names: list[str]) -> list[dict]:
    """Group outcomes by (feature_file, scenario_name) and emit a parity row."""
    grouped: dict[tuple[str, str], dict[str, ScenarioOutcome]] = {}
    for o in outcomes:
        grouped.setdefault((o.feature_file, o.scenario_name), {})[o.backend] = o
    parity: list[dict] = []
    for (feature_file, scenario), per_backend in grouped.items():
        statuses = {b: (per_backend[b].status if b in per_backend else "missing") for b in backend_names}
        actuals = {b: (per_backend[b].actual if b in per_backend else None) for b in backend_names}
        divergence = len(set(statuses.values())) > 1
        if not divergence:
            # All same status; check actuals when status is pass.
            if statuses[backend_names[0]] == "pass":
                seen = [actuals[b] for b in backend_names]
                divergence = any(seen[i] != seen[0] for i in range(1, len(seen)))
        parity.append({
            "feature_file": feature_file,
            "scenario_name": scenario,
            "statuses": statuses,
            "divergence": divergence,
        })
    return parity


def _print_parity_summary(parity: list[dict]) -> None:
    diverged = [p for p in parity if p["divergence"]]
    total = len(parity)
    print(f"Parity: {len(diverged)}/{total} scenarios diverge across backends.")
    if diverged:
        # Tally which backend disagrees most often by status.
        per_backend_status: dict[str, Counter] = {}
        for p in diverged:
            for b, s in p["statuses"].items():
                per_backend_status.setdefault(b, Counter())[s] += 1
        for b, counts in per_backend_status.items():
            top = ", ".join(f"{s}={n}" for s, n in counts.most_common())
            print(f"  {b}: {top}")


def _rel_to_features(p, root) -> str:
    try:
        return str(p.relative_to(root))
    except ValueError:
        return str(p)


def _json_default(o):
    return repr(o)


if __name__ == "__main__":
    raise SystemExit(main())
