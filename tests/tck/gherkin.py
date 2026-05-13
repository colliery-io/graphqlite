"""
Minimal Gherkin parser for openCypher TCK .feature files.

The TCK uses a restricted Gherkin dialect: Feature / Scenario / Given /
And / When / Then / But, optional docstrings delimited by triple double-
quotes, and pipe tables. We do not need full Cucumber.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


STEP_KEYWORDS = ("Given", "When", "Then", "And", "But")


@dataclass
class Step:
    keyword: str           # "Given" / "When" / "Then" / "And" / "But"
    text: str              # the rest of the step line, stripped
    docstring: str | None = None   # text inside """ ... """, or None
    table: list[list[str]] | None = None  # rows of cells, or None
    line: int = 0


@dataclass
class Scenario:
    name: str
    steps: list[Step] = field(default_factory=list)
    tags: list[str] = field(default_factory=list)
    line: int = 0
    # For Scenario Outline: each row of the Examples table becomes one
    # expanded scenario. The parser produces those expansions directly so the
    # runner sees concrete Scenarios.
    is_outline: bool = False


@dataclass
class Feature:
    name: str
    path: Path
    scenarios: list[Scenario] = field(default_factory=list)
    tags: list[str] = field(default_factory=list)


def parse_feature(path: Path) -> Feature:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    i = 0
    n = len(lines)
    feature: Feature | None = None
    pending_tags: list[str] = []
    current_scenario: Scenario | None = None

    def stripped(s: str) -> str:
        return s.strip()

    while i < n:
        raw = lines[i]
        line = stripped(raw)
        i += 1

        if not line or line.startswith("#"):
            continue

        if line.startswith("@"):
            pending_tags.extend(line.split())
            continue

        if line.startswith("Feature:"):
            feature = Feature(name=line[len("Feature:"):].strip(), path=path, tags=pending_tags)
            pending_tags = []
            continue

        if line.startswith("Scenario:") or line.startswith("Scenario Outline:"):
            is_outline = line.startswith("Scenario Outline:")
            kw = "Scenario Outline:" if is_outline else "Scenario:"
            name = line[len(kw):].strip()
            current_scenario = Scenario(name=name, tags=pending_tags, line=i,
                                        is_outline=is_outline)
            pending_tags = []
            assert feature is not None, f"Scenario before Feature in {path}"
            feature.scenarios.append(current_scenario)
            continue

        if line.startswith("Background:"):
            # Treat Background as a synthetic scenario only by appending
            # its steps to every following scenario. For TCK this is rare;
            # we record it as its own scenario for now and let the runner
            # decide. Simpler: accumulate steps and prepend later.
            current_scenario = Scenario(name="__background__", line=i)
            assert feature is not None
            feature.scenarios.append(current_scenario)
            continue

        if line.startswith("Examples:"):
            # Read the Examples pipe-table and, for each data row, expand
            # the current Scenario Outline into a concrete Scenario with
            # <placeholders> in step text/docstring/tables substituted.
            rows: list[list[str]] = []
            while i < n:
                ln = lines[i].strip()
                i += 1
                if not ln:
                    if rows:
                        break
                    continue
                if not ln.startswith("|"):
                    i -= 1
                    break
                rows.append(_parse_table_row(lines[i - 1]))
            if rows and current_scenario is not None and current_scenario.is_outline:
                headers = rows[0]
                template = current_scenario
                # Remove the template from the feature; we replace it with
                # one expanded scenario per example row.
                feature.scenarios.pop()
                for ridx, row in enumerate(rows[1:], 1):
                    subs = dict(zip(headers, row))
                    expanded = Scenario(
                        name=f"{template.name} (example {ridx})",
                        tags=list(template.tags),
                        line=template.line,
                        is_outline=False,
                    )
                    for s in template.steps:
                        expanded.steps.append(_substitute_step(s, subs))
                    feature.scenarios.append(expanded)
                current_scenario = None
            continue

        kw_match = _match_step_keyword(line)
        if kw_match and current_scenario is not None:
            keyword, rest = kw_match
            step = Step(keyword=keyword, text=rest, line=i)
            # Lookahead for docstring or table on subsequent lines.
            i = _consume_step_payload(lines, i, step)
            current_scenario.steps.append(step)
            continue

        # Unknown line — ignore quietly. TCK files are tidy.

    assert feature is not None, f"No Feature: in {path}"
    _inline_background(feature)
    return feature


def _match_step_keyword(line: str) -> tuple[str, str] | None:
    for kw in STEP_KEYWORDS:
        prefix = kw + " "
        if line.startswith(prefix):
            return kw, line[len(prefix):].strip().rstrip(":")
        if line == kw + ":":
            # e.g. "When executing query:" arrives via the prefix branch;
            # bare keyword on its own line is not valid Gherkin.
            return kw, ""
    return None


def _consume_step_payload(lines: list[str], i: int, step: Step) -> int:
    # Skip blank lines, then accept either a docstring or a table.
    n = len(lines)
    j = i
    while j < n and not lines[j].strip():
        j += 1
    if j >= n:
        return i

    nxt = lines[j].strip()
    if nxt.startswith('"""'):
        # Docstring — capture lines until the closing """.
        body: list[str] = []
        j += 1
        while j < n and lines[j].strip() != '"""':
            body.append(lines[j])
            j += 1
        # Skip the closing """ if present.
        if j < n:
            j += 1
        step.docstring = "\n".join(body)
        return j

    if nxt.startswith("|"):
        rows: list[list[str]] = []
        while j < n and lines[j].strip().startswith("|"):
            row = _parse_table_row(lines[j])
            rows.append(row)
            j += 1
        step.table = rows
        return j

    return i


def _substitute_step(s: Step, subs: dict[str, str]) -> Step:
    """Return a copy of step `s` with <placeholder> substitutions applied."""
    def sub(text: str) -> str:
        out = text
        for k, v in subs.items():
            out = out.replace(f"<{k}>", v)
        return out
    new_step = Step(keyword=s.keyword, text=sub(s.text), line=s.line)
    if s.docstring is not None:
        new_step.docstring = sub(s.docstring)
    if s.table is not None:
        new_step.table = [[sub(c) for c in row] for row in s.table]
    return new_step


def _parse_table_row(raw: str) -> list[str]:
    # Drop leading/trailing pipes, split on un-escaped pipes, strip each cell.
    s = raw.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|"):
        s = s[:-1]
    # Cells in TCK do not embed escaped pipes in practice. Simple split.
    return [cell.strip() for cell in s.split("|")]


def _inline_background(feature: Feature) -> None:
    """Inline a Background scenario's steps into every real scenario."""
    background: Scenario | None = None
    keep: list[Scenario] = []
    for sc in feature.scenarios:
        if sc.name == "__background__":
            background = sc
        else:
            keep.append(sc)
    if background is None:
        feature.scenarios = keep
        return
    for sc in keep:
        sc.steps = list(background.steps) + sc.steps
    feature.scenarios = keep


def walk_features(root: Path) -> Iterable[Feature]:
    for path in sorted(root.rglob("*.feature")):
        yield parse_feature(path)
