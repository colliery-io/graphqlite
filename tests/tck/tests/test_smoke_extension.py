"""End-to-end smoke test: harness loads the extension and runs one scenario."""

from __future__ import annotations

from pathlib import Path

import pytest

from tests.tck.backends.extension import ExtensionBackend
from tests.tck.gherkin import parse_feature
from tests.tck.runner import run_feature


@pytest.fixture
def feature_file(tmp_path: Path) -> Path:
    p = tmp_path / "smoke.feature"
    p.write_text(
        "Feature: smoke\n"
        "\n"
        "  Scenario: empty match returns empty\n"
        "    Given an empty graph\n"
        "    When executing query:\n"
        '      """\n'
        "      MATCH (n) RETURN n\n"
        '      """\n'
        "    Then the result should be empty\n"
    )
    return p


def test_extension_backend_smoke(feature_file: Path):
    ext = Path("build/graphqlite.dylib")
    if not ext.exists():
        pytest.skip("extension not built")
    backend = ExtensionBackend(extension_path=Path("build/graphqlite"))
    feat = parse_feature(feature_file)
    outcomes = run_feature(feat, backend)
    backend.close()
    assert len(outcomes) == 1
    # We accept pass OR fail here — the contract for v1 is that the harness
    # completes without crashing and produces a verdict. Triage of the
    # extension's actual return-value semantics is GQLITE-T-0210/0211.
    assert outcomes[0].status in {"pass", "fail", "error"}, outcomes[0]
