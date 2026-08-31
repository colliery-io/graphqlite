import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  ALGO_COLUMN_NAMES,
  extractAlgoArray,
  safeFloat,
  safeInt,
  parseScoreResult,
  parseCommunityResult,
  parseComponentResult,
  parseTraversalResult,
} from '../src/algorithms/parsing.ts';

test('ALGO_COLUMN_NAMES has 13 entries in Python order, camelCase names (#65)', () => {
  assert.equal(ALGO_COLUMN_NAMES.length, 13);
  assert.equal(ALGO_COLUMN_NAMES[0], 'column_0');
  // Casing corrected (#65): names match the real camelCase Cypher functions.
  assert.ok(ALGO_COLUMN_NAMES.includes('pageRank()'));
  assert.ok(ALGO_COLUMN_NAMES.includes('degreeCentrality()'));
  assert.ok(!ALGO_COLUMN_NAMES.includes('pagerank()'));
  assert.deepEqual(ALGO_COLUMN_NAMES.slice(-3), ['bfs()', 'dfs()', 'apsp()']);
});

test('extractAlgoArray unwraps a single-row column_0 array', () => {
  const rows = [{ column_0: [{ node_id: 1 }, { node_id: 2 }] }];
  assert.deepEqual(extractAlgoArray(rows), [{ node_id: 1 }, { node_id: 2 }]);
});

test('extractAlgoArray returns input as-is when there are multiple rows', () => {
  const rows = [{ a: 1 }, { a: 2 }];
  assert.equal(extractAlgoArray(rows), rows);
});

test('extractAlgoArray returns input when the single row has no array column', () => {
  const rows = [{ score: 0.5 }];
  assert.equal(extractAlgoArray(rows), rows);
});

test('extractAlgoArray falls back to column_0 when a camelCase name would miss', () => {
  // A real query emitting pageRank(...) does not match the snake list, so the
  // wrapper lands under column_0 and is still unwrapped.
  const rows = [{ column_0: [{ score: 1 }], 'pageRank()': 'ignored' }];
  assert.deepEqual(extractAlgoArray(rows), [{ score: 1 }]);
});

test('safeFloat: null and conversion failure return the default', () => {
  assert.equal(safeFloat(null), 0.0);
  assert.equal(safeFloat(undefined), 0.0);
  assert.equal(safeFloat('abc'), 0.0);
  assert.equal(safeFloat('', 7), 7);
  assert.equal(safeFloat('1.5'), 1.5);
  assert.equal(safeFloat(2), 2);
  assert.equal(safeFloat(null, 9.9), 9.9);
});

test('safeInt: null, floats-as-strings, and junk return the default', () => {
  assert.equal(safeInt(null), 0);
  assert.equal(safeInt('abc'), 0);
  assert.equal(safeInt('1.5'), 0); // Python int('1.5') raises -> default
  assert.equal(safeInt('10'), 10);
  assert.equal(safeInt(3.9), 3); // truncates toward zero like Python int(float)
  assert.equal(safeInt(-3.9), -3);
  assert.equal(safeInt(null, 5), 5);
});

test('parseScoreResult: null node_id -> null; otherwise stringifies id', () => {
  assert.equal(parseScoreResult({ user_id: 'a' }), null);
  assert.deepEqual(parseScoreResult({ node_id: 7, user_id: 'bob', score: 0.42 }), {
    node_id: '7',
    user_id: 'bob',
    score: 0.42,
  });
  assert.equal(parseScoreResult({ node_id: 1, user_id: 'a' })!.score, 0.0);
});

test('parseCommunityResult: null community -> null; falsy community -> 0', () => {
  assert.equal(parseCommunityResult({ node_id: 1, community: null }), null);
  assert.equal(parseCommunityResult({ node_id: 1, community: 0 })!.community, 0);
  assert.equal(parseCommunityResult({ node_id: 1, community: 3 })!.community, 3);
});

test('parseComponentResult: component 0 stays 0 (is-not-None semantics)', () => {
  assert.equal(parseComponentResult({ user_id: 'a' }), null);
  assert.equal(parseComponentResult({ node_id: 2, component: 0 })!.component, 0);
  assert.equal(parseComponentResult({ node_id: 2, component: 5 })!.component, 5);
});

test('parseTraversalResult: null user_id -> null; missing depth/order -> 0', () => {
  assert.equal(parseTraversalResult({ depth: 1 }), null);
  assert.deepEqual(parseTraversalResult({ user_id: 'x', depth: 2, order: 3 }), {
    user_id: 'x',
    depth: 2,
    order: 3,
  });
  assert.deepEqual(parseTraversalResult({ user_id: 'x' }), { user_id: 'x', depth: 0, order: 0 });
});
