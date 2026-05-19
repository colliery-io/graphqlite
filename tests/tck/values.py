"""
Parser and comparator for openCypher textual value forms used in TCK
result tables.

Supported literals:
    null
    true / false
    integers, floats (incl. scientific, infinity, NaN)
    strings 'single' or "double"
    lists [a, b, c]
    maps  {key: value, ...}
    nodes (:Label), (:A:B {k: v}), ()
    relationships [:TYPE {k: v}], [:TYPE]
    paths  <(a)-[r]->(b)>           (best-effort; structural compare)

`parse_value` returns a normalized Python value (see classes below).
`values_equal` does structural equality used by the comparator. Lists are
compared element-wise; maps are key-set equal with values compared
recursively; nodes/relationships compare on (labels, properties) without
identity (TCK convention — identity is not observable in result tables).
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True)
class Node:
    labels: tuple[str, ...]
    properties: tuple[tuple[str, Any], ...]  # sorted by key


@dataclass(frozen=True)
class Relationship:
    type: str
    properties: tuple[tuple[str, Any], ...]


@dataclass(frozen=True)
class Path:
    # alternating: node, (rel, direction, node), (rel, direction, node), ...
    nodes: tuple[Node, ...]
    rels: tuple[tuple[Relationship, str], ...]  # direction: ">" or "<"


class ValueParseError(ValueError):
    pass


def parse_value(text: str) -> Any:
    p = _Parser(text)
    v = p.parse_top()
    p.skip_ws()
    if not p.eof():
        raise ValueParseError(f"trailing input at pos {p.pos}: {text!r}")
    return v


def values_equal(a: Any, b: Any, lists_unordered: bool = False) -> bool:
    # Normalize: the extension emits nodes/relationships as dicts; the TCK
    # comparator parses them into Node/Relationship dataclasses. Coerce dicts
    # that look like nodes/rels into the dataclass before comparing.
    a = _coerce_graph_value(a)
    b = _coerce_graph_value(b)
    if isinstance(a, float) or isinstance(b, float):
        if isinstance(a, (int, float)) and isinstance(b, (int, float)):
            if math.isnan(float(a)) and math.isnan(float(b)):
                return True
            return float(a) == float(b)
        return False
    if isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            return False
        if lists_unordered:
            # Multiset comparison on lists. Match each expected element to
            # exactly one remaining actual element.
            remaining = list(b)
            for x in a:
                for i, y in enumerate(remaining):
                    if values_equal(x, y, lists_unordered=lists_unordered):
                        remaining.pop(i)
                        break
                else:
                    return False
            return True
        return all(values_equal(x, y, lists_unordered=lists_unordered)
                   for x, y in zip(a, b))
    if isinstance(a, dict) and isinstance(b, dict):
        if a.keys() != b.keys():
            return False
        return all(values_equal(a[k], b[k], lists_unordered=lists_unordered) for k in a)
    if isinstance(a, Node) and isinstance(b, Node):
        # Labels are an unordered set per Cypher spec; TCK expected text
        # encodes a particular order but that's a textual convention, not
        # semantic.
        return set(a.labels) == set(b.labels) and _props_equal(a.properties, b.properties)
    if isinstance(a, Relationship) and isinstance(b, Relationship):
        return a.type == b.type and _props_equal(a.properties, b.properties)
    if isinstance(a, Path) and isinstance(b, Path):
        if len(a.nodes) != len(b.nodes) or len(a.rels) != len(b.rels):
            return False
        return (
            all(values_equal(x, y) for x, y in zip(a.nodes, b.nodes))
            and all(r1[1] == r2[1] and values_equal(r1[0], r2[0])
                    for r1, r2 in zip(a.rels, b.rels))
        )
    return a == b


def _coerce_graph_value(v: Any) -> Any:
    """Coerce extension-side dict representations to our dataclasses.

    Node:         {"id": ..., "labels": [...], "properties": {...}}
    Relationship: {"id": ..., "type": "...", "properties": {...}, ...}
    Path:         {"nodes": [Node,...], "rels": [Relationship,...]}
    """
    if isinstance(v, dict):
        if "labels" in v and "properties" in v and isinstance(v["labels"], list):
            labels = tuple(v["labels"])
            props = tuple(sorted((k, v["properties"][k]) for k in v["properties"]))
            return Node(labels=labels, properties=props)
        if "type" in v and "properties" in v and isinstance(v["type"], str):
            props = tuple(sorted((k, v["properties"][k]) for k in v["properties"]))
            return Relationship(type=v["type"], properties=props)
        if "nodes" in v and "rels" in v and isinstance(v["nodes"], list) and isinstance(v["rels"], list):
            nodes = tuple(_coerce_graph_value(n) for n in v["nodes"])
            # Derive direction from each rel's startNode/endNode relative
            # to the path's node-id sequence: if rel.startNode == the
            # earlier node, the path traverses it forward (">"); else "<".
            node_ids = [n.get("id") if isinstance(n, dict) else None for n in v["nodes"]]
            rels_out = []
            for idx, r in enumerate(v["rels"]):
                direction = ">"
                if isinstance(r, dict) and idx + 1 < len(node_ids):
                    sn = r.get("startNode")
                    if sn is not None and sn == node_ids[idx + 1] \
                            and r.get("endNode") == node_ids[idx]:
                        direction = "<"
                rels_out.append((_coerce_graph_value(r), direction))
            return Path(nodes=nodes, rels=tuple(rels_out))
        # Duration objects carry an `_iso8601` rendering — collapse to that
        # string so TCK comparisons against the canonical 'PT22H' form work.
        if "_iso8601" in v and isinstance(v["_iso8601"], str):
            return v["_iso8601"]
    return v


def _props_equal(a: tuple[tuple[str, Any], ...], b: tuple[tuple[str, Any], ...]) -> bool:
    if len(a) != len(b):
        return False
    return all(k1 == k2 and values_equal(v1, v2) for (k1, v1), (k2, v2) in zip(a, b))


# ---------------------------------------------------------------------------


class _Parser:
    def __init__(self, text: str):
        self.text = text
        self.pos = 0

    def eof(self) -> bool:
        return self.pos >= len(self.text)

    def peek(self) -> str:
        return self.text[self.pos] if not self.eof() else ""

    def skip_ws(self) -> None:
        while not self.eof() and self.text[self.pos] in " \t\n\r":
            self.pos += 1

    def expect(self, ch: str) -> None:
        self.skip_ws()
        if self.eof() or self.text[self.pos] != ch:
            raise ValueParseError(f"expected {ch!r} at pos {self.pos} of {self.text!r}")
        self.pos += 1

    def parse_top(self) -> Any:
        self.skip_ws()
        return self._parse_value()

    def _parse_value(self) -> Any:
        self.skip_ws()
        if self.eof():
            raise ValueParseError("empty value")
        c = self.peek()
        if c == "(":
            return self._parse_node()
        if c == "[":
            # Either a list "[1, 2]" or a relationship "[:TYPE]".
            return self._parse_list_or_rel()
        if c == "<":
            return self._parse_path()
        if c == "{":
            return self._parse_map()
        if c in "'\"":
            return self._parse_string()
        # Word / number / sign.
        return self._parse_atom()

    def _parse_node(self) -> Node:
        self.expect("(")
        labels: list[str] = []
        self.skip_ws()
        while self.peek() == ":":
            self.pos += 1
            labels.append(self._parse_ident())
            self.skip_ws()
        props: tuple[tuple[str, Any], ...] = ()
        if self.peek() == "{":
            props = _props_from_map(self._parse_map())
        self.skip_ws()
        self.expect(")")
        return Node(labels=tuple(labels), properties=props)

    def _parse_list_or_rel(self) -> Any:
        # Disambiguate by peeking past the '['.
        save = self.pos
        self.expect("[")
        self.skip_ws()
        if self.peek() == ":":
            # Relationship: [:TYPE {...}]
            self.pos += 1
            type_ = self._parse_ident()
            self.skip_ws()
            props: tuple[tuple[str, Any], ...] = ()
            if self.peek() == "{":
                props = _props_from_map(self._parse_map())
            self.skip_ws()
            self.expect("]")
            return Relationship(type=type_, properties=props)
        # Plain list.
        items: list[Any] = []
        if self.peek() != "]":
            while True:
                items.append(self._parse_value())
                self.skip_ws()
                if self.peek() == ",":
                    self.pos += 1
                    self.skip_ws()
                    continue
                break
        self.expect("]")
        return items

    def _parse_map(self) -> dict[str, Any]:
        self.expect("{")
        out: dict[str, Any] = {}
        self.skip_ws()
        if self.peek() == "}":
            self.pos += 1
            return out
        while True:
            self.skip_ws()
            key = self._parse_ident_or_string()
            self.skip_ws()
            self.expect(":")
            val = self._parse_value()
            out[key] = val
            self.skip_ws()
            if self.peek() == ",":
                self.pos += 1
                continue
            break
        self.expect("}")
        return out

    def _parse_path(self) -> Path:
        self.expect("<")
        nodes: list[Node] = [self._parse_node()]
        rels: list[tuple[Relationship, str]] = []
        self.skip_ws()
        while self.peek() == "-" or self.peek() == "<":
            # Directionless / left arrow.
            if self.peek() == "<":
                self.pos += 1
                self.expect("-")
                rel = self._parse_rel_segment()
                self.expect("-")
                nodes.append(self._parse_node())
                rels.append((rel, "<"))
            else:
                self.pos += 1  # consume first '-'
                rel = self._parse_rel_segment()
                self.expect("-")
                # Optional '>' indicates direction.
                if self.peek() == ">":
                    self.pos += 1
                    direction = ">"
                else:
                    direction = "-"
                nodes.append(self._parse_node())
                rels.append((rel, direction))
            self.skip_ws()
        self.expect(">")
        return Path(nodes=tuple(nodes), rels=tuple(rels))

    def _parse_rel_segment(self) -> Relationship:
        # Either "[:TYPE {...}]" or implicit "" (no segment).
        self.skip_ws()
        if self.peek() == "[":
            val = self._parse_list_or_rel()
            if isinstance(val, Relationship):
                return val
            raise ValueParseError("expected relationship inside path")
        # Anonymous relationship — represent as Relationship("", ()).
        return Relationship(type="", properties=())

    def _parse_string(self) -> str:
        quote = self.text[self.pos]
        self.pos += 1
        out: list[str] = []
        while not self.eof():
            c = self.text[self.pos]
            if c == "\\" and self.pos + 1 < len(self.text):
                nxt = self.text[self.pos + 1]
                out.append({"n": "\n", "t": "\t", "r": "\r", "\\": "\\", "'": "'", '"': '"'}.get(nxt, nxt))
                self.pos += 2
                continue
            if c == quote:
                self.pos += 1
                return "".join(out)
            out.append(c)
            self.pos += 1
        raise ValueParseError("unterminated string")

    def _parse_ident(self) -> str:
        self.skip_ws()
        m = re.match(r"[A-Za-z_][A-Za-z_0-9]*", self.text[self.pos:])
        if not m:
            raise ValueParseError(f"expected identifier at pos {self.pos}")
        self.pos += m.end()
        return m.group(0)

    def _parse_ident_or_string(self) -> str:
        if self.peek() in "'\"":
            return self._parse_string()
        return self._parse_ident()

    def _parse_atom(self) -> Any:
        m = re.match(r"[A-Za-z_][A-Za-z_0-9]*", self.text[self.pos:])
        if m:
            word = m.group(0)
            self.pos += m.end()
            if word == "null":
                return None
            if word == "true":
                return True
            if word == "false":
                return False
            if word in ("Inf", "Infinity", "inf"):
                return math.inf
            if word == "NaN":
                return math.nan
            return word  # bareword (e.g. property key in a context we don't handle)
        # Number
        m = re.match(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?", self.text[self.pos:])
        if not m:
            raise ValueParseError(f"unrecognized atom at pos {self.pos}: {self.text[self.pos:self.pos+10]!r}")
        token = m.group(0)
        self.pos += m.end()
        if "." in token or "e" in token or "E" in token:
            return float(token)
        return int(token)


def _props_from_map(d: dict[str, Any]) -> tuple[tuple[str, Any], ...]:
    return tuple(sorted(d.items(), key=lambda kv: kv[0]))
