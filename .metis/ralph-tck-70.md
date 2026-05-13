# TCK Conformance Ralph Loop — Drive pass rate to ≥ 70%

Drive `tests/tck/baseline.json` `pass / total` to ≥ 0.70 against the SQLite
extension (the "core cypher" plane). Each iteration is self-contained: orient,
pick one backlog ticket, fix it for real, re-run, commit.

## Each iteration

### 1. Orient

Read in this order:
- `tests/tck/baseline.json` — current numbers (source of truth)
- `docs/tck/triage-2026-05-13.md` — cluster → ticket mapping
- `git log --oneline -20` — what previous iterations did

### 2. Exit check

If `pass / total >= 0.70`, output exactly the line below and stop:

```
<promise>TCK ≥ 70%</promise>
```

Do not keep working past the target.

### 3. Pick ONE task

From `.metis/backlog/` choose one open ticket whose fix will move pass rate
the most, weighted by likely effort. Preference order:

```
P0 bugs  >  P1 bugs  >  P1 tech-debt  >  P2 bugs  >  P2 features
```

Use `mcp__metis__list_documents` to see open ones. Skip anything already
`completed`. Note the short code (`GQLITE-T-NNNN`) and transition it to
`active`.

**Iteration 1:** pick `GQLITE-T-0218` (the P0 SIGSEGV) first so subsequent
runs are quiet.

### 4. Fix it

Edit the C extension under `src/` (or the harness if the ticket is harness
work). Build with `angreal build extension`.

**Do not** touch `tests/`, `vendor/tck/`, or any scenario filter — fixes
must be real fixes, not test exclusion. Do not add xfail markers, do not
add per-scenario skips, do not relax the comparator.

### 5. Re-run baseline

```
PYTHONPATH=. /opt/local/bin/python3.13 -m tests.tck --backend extension --out build/tck-baseline-results.json
```

Then regenerate the report and `baseline.json`:

```
PYTHONPATH=. /opt/local/bin/python3.13 -m tests.tck.report --results build/tck-baseline-results.json --out docs/tck/baseline-2026-05-13.md --baseline-out tests/tck/baseline.json
```

Confirm `pass` did not regress from the prior baseline. If it did: revert
your change, pick a different ticket, and record the dead-end reason in the
ticket's Status Updates.

### 6. Close the ticket

Update the selected ticket's Status Updates with:
- one-line summary of the fix
- new pass count and delta
- any related scenarios still failing

Transition the ticket to `completed`.

### 7. Commit

One commit per iteration:

```
git add -A
git commit -m "fix(tck): <one-line> (+N passes; GQLITE-T-NNNN)"
```

## Hard constraints

- Never edit `vendor/tck/`, `tests/tck/values.py` comparator semantics, or
  step-pattern matchers in `tests/tck/runner.py` to "skip" failing
  scenarios.
- Never add a feature flag whose only purpose is to hide a failure.
- If a ticket is much bigger than expected, log that finding and pick a
  different one. Don't get stuck on a single fix across many iterations.
- Bindings (Python, Rust) are out of scope — extension only.

## Promise format

```
<promise>TCK ≥ 70%</promise>
```
