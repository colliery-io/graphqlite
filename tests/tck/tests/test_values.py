"""Unit tests for the value parser and comparator."""

from __future__ import annotations

import math

from tests.tck.values import Node, Relationship, parse_value, values_equal


def test_scalars():
    assert parse_value("null") is None
    assert parse_value("true") is True
    assert parse_value("false") is False
    assert parse_value("42") == 42
    assert parse_value("-3") == -3
    assert parse_value("1.5") == 1.5
    assert parse_value("'hi'") == "hi"
    assert parse_value('"hi"') == "hi"


def test_nan_equal():
    assert values_equal(math.nan, parse_value("NaN"))


def test_list_and_map():
    assert parse_value("[1, 2, 3]") == [1, 2, 3]
    assert parse_value("{a: 1, b: 'x'}") == {"a": 1, "b": "x"}


def test_node_with_labels_and_props():
    v = parse_value("(:A:B {name: 'x', n: 1})")
    assert isinstance(v, Node)
    assert v.labels == ("A", "B")
    assert dict(v.properties) == {"n": 1, "name": "x"}


def test_node_anonymous():
    v = parse_value("()")
    assert isinstance(v, Node)
    assert v.labels == ()
    assert v.properties == ()


def test_relationship():
    v = parse_value("[:KNOWS {since: 2020}]")
    assert isinstance(v, Relationship)
    assert v.type == "KNOWS"
    assert dict(v.properties) == {"since": 2020}


def test_values_equal_handles_node_identity_irrelevant():
    a = parse_value("(:A {x: 1})")
    b = parse_value("(:A {x: 1})")
    assert values_equal(a, b)
