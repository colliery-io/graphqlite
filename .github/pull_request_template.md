## Summary
<!-- 1-3 bullet points describing what changed and why. -->

## Test plan
<!-- Bulleted checklist of how to verify the change. -->

## Semantic coverage matrix
<!-- Required when changes touch src/backend/transform/ or src/backend/executor/.
     See docs/testing/semantic-coverage-matrix.md. -->

- [ ] Cells added/modified in `docs/testing/semantic-coverage-matrix.md`:
  - _list cells or write "n/a" for pure refactors_
- [ ] Regression tests added/updated in `tests/functional/`:
  - _paths or "n/a"_
- [ ] If neither: reviewer may apply `skip-coverage-matrix` label (pure refactors only).

## Checklist
- [ ] `angreal test functional` green locally
- [ ] `angreal test unit` green locally
- [ ] PR title follows conventional-commit style (`fix:`, `feat:`, `chore:` …)
