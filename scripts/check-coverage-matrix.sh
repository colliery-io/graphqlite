#!/usr/bin/env bash
# GQLITE-T-0204: enforce semantic-coverage-matrix discipline.
#
# When a PR modifies src/backend/transform/ or src/backend/executor/ , it must
# also update one of:
#   - docs/testing/semantic-coverage-matrix.md
#   - tests/functional/*.sql
#   - carry a `skip-coverage-matrix` label (handled by the CI job, not this
#     script; locally this is bypass-by-env).
#
# Locally:     BASE_REF=main scripts/check-coverage-matrix.sh
# In CI:       set BASE_REF to the PR base; label check happens in the job.

set -euo pipefail

BASE_REF="${BASE_REF:-origin/main}"
SKIP_ENV="${GQLITE_SKIP_COVERAGE_MATRIX:-}"

if [[ -n "$SKIP_ENV" ]]; then
    echo "coverage-matrix: skipped via GQLITE_SKIP_COVERAGE_MATRIX"
    exit 0
fi

# Ensure we can reach the base ref
if ! git rev-parse --verify "$BASE_REF" >/dev/null 2>&1; then
    echo "coverage-matrix: base ref '$BASE_REF' not available locally; fetching"
    git fetch origin "${BASE_REF#origin/}" --depth=50 >/dev/null 2>&1 || true
fi

CHANGED="$(git diff --name-only "$BASE_REF...HEAD" 2>/dev/null || git diff --name-only HEAD)"

transform_or_exec=0
matrix_or_tests=0
while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    case "$path" in
        src/backend/transform/*|src/backend/executor/*)
            transform_or_exec=1 ;;
        docs/testing/semantic-coverage-matrix.md|tests/functional/*.sql)
            matrix_or_tests=1 ;;
    esac
done <<< "$CHANGED"

if [[ $transform_or_exec -eq 0 ]]; then
    echo "coverage-matrix: no transform/executor changes; nothing to check"
    exit 0
fi

if [[ $matrix_or_tests -eq 1 ]]; then
    echo "coverage-matrix: transform/executor change paired with matrix or test diff — OK"
    exit 0
fi

cat >&2 <<EOF
coverage-matrix: FAIL

This PR modifies src/backend/transform/ or src/backend/executor/ but does
not update docs/testing/semantic-coverage-matrix.md or any file under
tests/functional/.

Options:
  1. Add or update a regression test in tests/functional/39_issue_regression_tests.sql
     (or a sibling file) exercising the changed code.
  2. Update docs/testing/semantic-coverage-matrix.md to reflect the new cell
     coverage or any known-GAP added/removed.
  3. If this PR is a pure refactor with no behavior change, apply the
     \`skip-coverage-matrix\` label (reviewer override, reserved for refactors).

See docs/testing/semantic-coverage-matrix.md for the matrix structure.
EOF
exit 1
