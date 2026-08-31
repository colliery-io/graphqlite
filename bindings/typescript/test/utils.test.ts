import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  CYPHER_RESERVED,
  escapeString,
  sanitizeRelType,
  formatProps,
  assertIdentifier,
} from '../src/utils.ts';
import { ValidationError } from '../src/errors.ts';

// --- Ported from Python bindings/python/tests/test_graph.py (utility tests) ---

test('escape_string: single quotes', () => {
  assert.equal(escapeString("It's"), "It\\'s");
});

test('escape_string: double quotes', () => {
  assert.equal(escapeString('Say "hi"'), 'Say \\"hi\\"');
});

test('escape_string: backslash', () => {
  assert.equal(escapeString('C:\\path'), 'C:\\\\path');
});

test('escape_string: newlines become spaces', () => {
  assert.equal(escapeString('line1\nline2'), 'line1 line2');
});

test('escape_string: tabs become spaces', () => {
  assert.equal(escapeString('col1\tcol2'), 'col1 col2');
});

test('sanitize_rel_type: passthrough', () => {
  assert.equal(sanitizeRelType('KNOWS'), 'KNOWS');
});

test('sanitize_rel_type: special chars become underscore', () => {
  assert.equal(sanitizeRelType('RELATED-TO'), 'RELATED_TO');
});

test('sanitize_rel_type: leading digit gets REL_ prefix', () => {
  assert.ok(sanitizeRelType('123_TYPE').startsWith('REL_'));
});

test('sanitize_rel_type: reserved word gets REL_ prefix', () => {
  assert.equal(sanitizeRelType('CREATE'), 'REL_CREATE');
});

test('CYPHER_RESERVED contains core keywords (63 entries)', () => {
  assert.equal(CYPHER_RESERVED.size, 63);
  for (const kw of ['CREATE', 'MATCH', 'RETURN']) {
    assert.ok(CYPHER_RESERVED.has(kw));
  }
});

// --- Acceptance-criteria specific ---

test('sanitize_rel_type is Unicode-aware: CJK survives (matches Python isalnum)', () => {
  // An ASCII /[a-zA-Z0-9_]/ would turn every char into "_"; Python keeps them.
  assert.equal(sanitizeRelType('관계'), '관계');
});

test('sanitize_rel_type: leading digit before non-ASCII still prefixes', () => {
  assert.equal(sanitizeRelType('123abc'), 'REL_123abc');
  assert.equal(sanitizeRelType('match'), 'REL_match'); // reserved after upper()
});

test('format_props: strings escaped, bool/null formatted, numbers raw', () => {
  assert.equal(
    formatProps({ a: true, b: 3, c: null, d: 'x' }),
    "a: true, b: 3, c: null, d: 'x'",
  );
});

test('format_props: non-string/bool/null values interpolated without escaping', () => {
  // A number is interpolated raw; a string with a quote IS escaped.
  assert.equal(formatProps({ n: 42, s: "O'Brien" }), "n: 42, s: 'O\\'Brien'");
  assert.equal(formatProps({ f: 1.5 }), 'f: 1.5');
});

test('format_props: string values are escaped via escapeString', () => {
  assert.equal(formatProps({ name: 'a\nb' }), "name: 'a b'");
});

// --- assertIdentifier ---

test('assertIdentifier accepts valid identifiers (latin, underscore, CJK)', () => {
  for (const ok of ['Person', 'user_id', '_private', 'name2', '사람', '이름']) {
    assert.doesNotThrow(() => assertIdentifier(ok, 'label'));
  }
});

test('assertIdentifier rejects injection-shaped identifiers with ValidationError', () => {
  for (const bad of ['', '2leading', 'has space', 'a`b', "a'b", 'a;b', 'a-b', 'a.b']) {
    assert.throws(() => assertIdentifier(bad, 'property'), ValidationError, `should reject: ${bad}`);
  }
});

test('assertIdentifier error carries VALIDATION_ERROR code', () => {
  try {
    assertIdentifier('bad ident', 'graph');
    assert.fail('expected throw');
  } catch (err) {
    assert.ok(err instanceof ValidationError);
    assert.equal(err.code, 'VALIDATION_ERROR');
  }
});
