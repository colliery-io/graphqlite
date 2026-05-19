#!/usr/bin/env bash
# Compare the just-run TCK results against the committed baseline.
# Fails if pass count drops (regression) or fail+error rises.
#
# Usage:
#   scripts/check-tck-baseline.sh [path/to/tck-results.json]
#
# Default results path: build/tck-results.json
# Baseline path:        tests/tck/baseline.json

set -euo pipefail

RESULTS="${1:-build/tck-results.json}"
BASELINE="tests/tck/baseline.json"

if [[ ! -f "$RESULTS" ]]; then
  echo "ERROR: results file $RESULTS not found. Run 'angreal test tck' first." >&2
  exit 2
fi
if [[ ! -f "$BASELINE" ]]; then
  echo "ERROR: baseline file $BASELINE missing." >&2
  exit 2
fi

python3 - "$RESULTS" "$BASELINE" <<'PY'
import json, sys, collections
results_path, baseline_path = sys.argv[1], sys.argv[2]

with open(baseline_path) as f:
    baseline = json.load(f)

with open(results_path) as f:
    data = json.load(f)

cur = collections.Counter(r["status"] for r in data)
cur_pass    = cur.get("pass", 0)
cur_fail    = cur.get("fail", 0)
cur_error   = cur.get("error", 0)
cur_skipped = cur.get("skipped", 0)
cur_total   = len(data)

base_pass    = baseline["pass"]
base_fail    = baseline["fail"]
base_error   = baseline["error"]
base_skipped = baseline["skipped"]
base_total   = baseline["total"]

def fmt(delta):
    return f"{delta:+d}" if delta != 0 else " 0"

print("openCypher TCK conformance gate")
print("================================")
print(f"  total:   {cur_total:>5}   (baseline {base_total})")
print(f"  pass:    {cur_pass:>5}   (baseline {base_pass:>5}, Δ {fmt(cur_pass - base_pass)})")
print(f"  fail:    {cur_fail:>5}   (baseline {base_fail:>5}, Δ {fmt(cur_fail - base_fail)})")
print(f"  error:   {cur_error:>5}   (baseline {base_error:>5}, Δ {fmt(cur_error - base_error)})")
print(f"  skipped: {cur_skipped:>5}   (baseline {base_skipped:>5}, Δ {fmt(cur_skipped - base_skipped)})")
print()

# Gate logic.
regressions = []
if cur_total != base_total:
    regressions.append(
        f"scenario total changed ({base_total} → {cur_total}). "
        f"If this is intentional (TCK refresh), update tests/tck/baseline.json."
    )
if cur_pass < base_pass:
    regressions.append(f"pass count dropped by {base_pass - cur_pass}")
if (cur_fail + cur_error) > (base_fail + base_error):
    delta = (cur_fail + cur_error) - (base_fail + base_error)
    regressions.append(f"fail+error rose by {delta}")

if regressions:
    print("REGRESSION:")
    for r in regressions:
        print(f"  - {r}")
    print()
    print("If this regression is expected, update tests/tck/baseline.json")
    print("by re-running: python -m tests.tck.report --results build/tck-results.json \\")
    print("                  --out tests/tck/baseline.md --baseline-out tests/tck/baseline.json")
    sys.exit(1)

# Improvements are surfaced but non-fatal.
if cur_pass > base_pass:
    print(f"IMPROVEMENT: +{cur_pass - base_pass} scenarios passing vs baseline.")
    print("Consider refreshing tests/tck/baseline.json to lock in the new floor.")
print("OK: no regression vs baseline.")
PY
