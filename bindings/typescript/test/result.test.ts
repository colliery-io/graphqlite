import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  CypherResult,
  normalizeCypherResult,
  parseMutationSummary,
} from '../src/result.ts';

test('① row set: exposes length, columns, indexing, iteration, toList', () => {
  const res = normalizeCypherResult('[{"name":"Alice","age":30},{"name":"Bob","age":25}]');
  assert.equal(res.length, 2);
  assert.deepEqual(res.columns, ['name', 'age']);
  assert.equal(res[0]?.name, 'Alice');
  assert.equal(res[1]?.age, 25);
  assert.deepEqual(res.toList().length, 2);
  const names = [...res].map((r) => r.name);
  assert.deepEqual(names, ['Alice', 'Bob']);
});

test('② node object: preserves nested node under its column', () => {
  const res = normalizeCypherResult('[{"n":{"id":1,"labels":["Person"],"properties":{"x":1}}}]');
  assert.deepEqual(res.columns, ['n']);
  assert.deepEqual(res[0]?.n, { id: 1, labels: ['Person'], properties: { x: 1 } });
});

test('③ algorithm: column_0 list-of-dict preserved', () => {
  const res = normalizeCypherResult('[{"column_0":[{"node_id":2,"user_id":"bob","score":0.138}]}]');
  assert.deepEqual(res.columns, ['column_0']);
  assert.equal(res.length, 1);
  assert.ok(Array.isArray(res[0]?.column_0));
});

test('scalar array is wrapped as raw {result} — not parsed (getAllNodes contract)', () => {
  const res = normalizeCypherResult('[1,2,3]');
  assert.equal(res.length, 1);
  assert.deepEqual(res.columns, ['result']);
  assert.equal(res[0]?.result, '[1,2,3]'); // raw string, not the parsed array
});

test('④ DDL summary: normalize keeps raw; parseMutationSummary structures it', () => {
  const raw = 'Query executed successfully - nodes created: 1, relationships created: 0';
  const res = normalizeCypherResult(raw);
  assert.deepEqual(res.columns, ['result']);
  assert.equal(res[0]?.result, raw);

  const summary = parseMutationSummary(raw);
  assert.deepEqual(summary, { nodesCreated: 1, relationshipsCreated: 0, raw });
});

test('④ DDL summary: JSON object from core (#72) parsed directly', () => {
  const raw = '{"nodes_created":3,"relationships_created":2}';
  // normalizeCypherResult treats the object as a single structured row.
  const res = normalizeCypherResult(raw);
  assert.deepEqual(res.columns, ['nodes_created', 'relationships_created']);
  assert.equal(res[0]?.['nodes_created'], 3);
  assert.equal(res[0]?.['relationships_created'], 2);

  // parseMutationSummary reads the counts straight from the JSON.
  const summary = parseMutationSummary(raw);
  assert.deepEqual(summary, { nodesCreated: 3, relationshipsCreated: 2, raw });
});

test('parseMutationSummary never throws; unknown format yields 0 counts + raw', () => {
  const raw = 'totally unexpected output shape';
  const summary = parseMutationSummary(raw);
  assert.deepEqual(summary, { nodesCreated: 0, relationshipsCreated: 0, raw });
});

test('parseMutationSummary reads multi-digit counts', () => {
  const summary = parseMutationSummary('nodes created: 12, relationships created: 34');
  assert.equal(summary.nodesCreated, 12);
  assert.equal(summary.relationshipsCreated, 34);
});

test('empty and null inputs become an empty CypherResult', () => {
  const fromNull = normalizeCypherResult(null);
  assert.equal(fromNull.length, 0);
  assert.deepEqual(fromNull.columns, []);

  const fromEmptyArray = normalizeCypherResult('[]');
  assert.equal(fromEmptyArray.length, 0);
});

test('a single JSON object becomes one row with its keys as columns', () => {
  const res = normalizeCypherResult('{"a":1,"b":2}');
  assert.equal(res.length, 1);
  assert.deepEqual(res.columns, ['a', 'b']);
  assert.equal(res[0]?.a, 1);
});

test('a bare non-JSON scalar string is wrapped as {result: raw}', () => {
  const res = normalizeCypherResult('just some text');
  assert.deepEqual(res.columns, ['result']);
  assert.equal(res[0]?.result, 'just some text');
});

test('CypherResult constructed directly is array-like', () => {
  const res = new CypherResult([{ x: 1 }, { x: 2 }], ['x']);
  assert.equal(res.length, 2);
  assert.equal(res[0]?.x, 1);
  assert.deepEqual([...res], [{ x: 1 }, { x: 2 }]);
  assert.deepEqual(res.toList(), [{ x: 1 }, { x: 2 }]);
});
