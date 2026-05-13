"""Unit tests for the Gherkin parser."""

from __future__ import annotations

from pathlib import Path

import textwrap

from tests.tck.gherkin import parse_feature


def _write(tmp_path: Path, body: str) -> Path:
    p = tmp_path / "f.feature"
    p.write_text(textwrap.dedent(body).lstrip())
    return p


def test_parses_scenario_with_docstring_and_table(tmp_path):
    p = _write(tmp_path, '''
        Feature: Smoke

          Scenario: simple
            Given an empty graph
            When executing query:
              """
              MATCH (n) RETURN n
              """
            Then the result should be, in any order:
              | n |
    ''')
    feat = parse_feature(p)
    assert feat.name == "Smoke"
    assert len(feat.scenarios) == 1
    steps = feat.scenarios[0].steps
    assert [s.keyword for s in steps] == ["Given", "When", "Then"]
    assert "MATCH" in steps[1].docstring
    assert steps[2].table == [["n"]]


def test_background_inlined(tmp_path):
    p = _write(tmp_path, '''
        Feature: Bg

          Background:
            Given an empty graph

          Scenario: a
            When executing query:
              """
              RETURN 1
              """
            Then the result should be, in any order:
              | 1 |
              | 1 |
    ''')
    feat = parse_feature(p)
    sc = feat.scenarios[0]
    assert sc.steps[0].keyword == "Given"
    assert sc.steps[0].text.startswith("an empty graph")
