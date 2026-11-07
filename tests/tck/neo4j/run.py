"""Run Neo4j's acceptance features against GraphQLite (GQLITE-T-0501).

The features are GPL-3.0 (neo4j/neo4j): they are fetched at test time from a
pinned commit into build/neo4j-acceptance and never committed here
(decision on COLLIERY-I-0670, 2026-11-05). The files in skip.txt are left
out; the rest run through the TCK runner. The result is compared with
baseline.txt, the scenarios that passed when the baseline was recorded.

    python -m tests.tck.neo4j.run [--ext build/graphqlite] [--update-baseline]

Exit 1 when a baseline scenario no longer passes; 0 otherwise.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO = "https://github.com/neo4j/neo4j.git"
COMMIT = "27f24499b751708815d42c8cf26bfb6597b3516c"   # release/5.26.32
DIRS = ("community/cypher/spec-suite-tools/src/test/resources/acceptance/features",
        "community/cypher/spec-suite-tools/src/test/resources/cypher25/acceptance/features")
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def fetch(dest: Path) -> None:
    """A sparse checkout of the pinned commit, kept between runs."""
    def git(*a, **kw):
        return subprocess.run(["git", "-C", str(dest), *a], check=True, capture_output=True, text=True, **kw)
    if (dest / ".git").exists():
        if git("rev-parse", "HEAD").stdout.strip() == COMMIT:
            return
    else:
        dest.mkdir(parents=True, exist_ok=True)
        git("init", "-q")
        git("remote", "add", "origin", REPO)
    git("sparse-checkout", "init", "--cone")
    git("sparse-checkout", "set", *DIRS)
    git("fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", COMMIT)
    git("checkout", "-q", "FETCH_HEAD")


def skipped() -> set[str]:
    names = set()
    for line in (HERE / "skip.txt").read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            names.add(line.split()[0])
    return names


def scenario_id(o: dict) -> str:
    return f"{Path(o['feature_file']).name}::{o['scenario_name']}"


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.tck.neo4j.run")
    p.add_argument("--ext", default=None, help="extension (default build/graphqlite)")
    p.add_argument("--update-baseline", action="store_true")
    args = p.parse_args(argv)

    src = ROOT / "build" / "neo4j-acceptance"
    fetch(src)
    feats = ROOT / "build" / "neo4j-features"
    shutil.rmtree(feats, ignore_errors=True)
    skip = skipped()
    n = 0
    for d in DIRS:
        for f in sorted((src / d).glob("*.feature")):
            if f.stem in skip:
                continue
            out = feats / Path(d).parts[-3] / f.name
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(f, out)
            n += 1
    results = ROOT / "build" / "neo4j-results.json"
    cmd = [sys.executable, "-m", "tests.tck", "--features", str(feats), "--quiet",
           "--out", str(results), "--parity-out", str(ROOT / "build" / "neo4j-parity.json")]
    if args.ext:
        cmd += ["--ext", args.ext]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    summary = [l for l in r.stdout.splitlines() if l.startswith("TCK [")]
    print(f"Neo4j acceptance features @ {COMMIT[:7]}: {n} files ({len(skip)} skipped)")
    print(summary[-1] if summary else r.stdout[-500:] + r.stderr[-500:])
    outcomes = json.loads(results.read_text())
    passing = {scenario_id(o) for o in outcomes if o["status"] == "pass"}

    baseline_file = HERE / "baseline.txt"
    if args.update_baseline:
        baseline_file.write_text(
            "# Neo4j acceptance scenarios that pass (GQLITE-T-0501); "
            "angreal test neo4j-acceptance --update-baseline\n"
            + "".join(f"{s}\n" for s in sorted(passing)))
        print(f"baseline: {len(passing)} passing scenarios written")
        return 0
    baseline = {l for l in baseline_file.read_text().splitlines() if l and not l.startswith("#")}
    lost = sorted(baseline - passing)
    gained = sorted(passing - baseline)
    for s in lost:
        print(f"NO LONGER PASSES: {s}")
    print(f"baseline {len(baseline)}: {len(lost)} no longer pass, {len(gained)} newly pass")
    if gained:
        print("  (record them with --update-baseline)")
    return 1 if lost else 0


if __name__ == "__main__":
    sys.exit(main())
