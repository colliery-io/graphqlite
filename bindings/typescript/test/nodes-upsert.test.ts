import { test } from 'node:test';
import assert from 'node:assert/strict';
import { upsertNode } from '../src/graph/nodes.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';
import { ValidationError } from '../src/errors.ts';

// --- Connection test double -------------------------------------------------
// Answers the hasNode probe with a caller-chosen count so both branches can be
// driven, and records every subsequent (query, params).
interface Call {
  query: string;
  params?: Record<string, unknown> | null;
}

function makeConn(exists: boolean) {
  const calls: Call[] = [];
  const conn = {
    cypher(query: string, params?: Record<string, unknown> | null): CypherResult {
      calls.push({ query, params });
      if (query.includes('count(n) AS cnt')) {
        const row: CypherRow = { cnt: exists ? 1 : 0 };
        return new CypherResult([row], ['cnt']);
      }
      return new CypherResult([], []);
    },
  };
  // Calls after the initial hasNode probe (index 0).
  return { conn: conn as never, calls, writes: () => calls.slice(1) };
}

// --- Create path (acceptance criteria #2, #5) ------------------------------
test('create path: default label Entity, single interpolated CREATE (Python parity)', () => {
  const { conn, writes } = makeConn(false);
  upsertNode(conn, 'alice', { name: 'Alice', age: 30 });
  const w = writes();
  assert.equal(w.length, 1);
  assert.equal(w[0]?.query, "CREATE (n:Entity {id: 'alice', name: 'Alice', age: 30})");
  assert.equal(w[0]?.params, undefined); // all interpolated, no params
});

test('create path: custom label is interpolated', () => {
  const { conn, writes } = makeConn(false);
  upsertNode(conn, 'alice', { name: 'Alice' }, 'Person');
  assert.equal(writes()[0]?.query, "CREATE (n:Person {id: 'alice', name: 'Alice'})");
});

// --- id symmetry (#70) -----------------------------------------------------
test('create path: nodeData.id is ignored, nodeId wins', () => {
  const { conn, writes } = makeConn(false);
  upsertNode(conn, 'alice', { id: 'override', name: 'A' });
  // props = { id: 'alice', ...rest } where rest drops nodeData.id → id is 'alice'.
  assert.equal(writes()[0]?.query, "CREATE (n:Entity {id: 'alice', name: 'A'})");
});

// --- Update path (acceptance criteria #4, #5) ------------------------------
test('update path: one query per entry, key interpolated, value bound, id untouched', () => {
  const { conn, writes } = makeConn(true);
  upsertNode(conn, 'alice', { name: 'A', age: 30 });
  const w = writes();
  assert.equal(w.length, 2); // N round-trips, not batched
  assert.deepEqual(w[0], {
    query: 'MATCH (n {id: $id}) SET n.name = $val RETURN n',
    params: { id: 'alice', val: 'A' },
  });
  assert.deepEqual(w[1], {
    query: 'MATCH (n {id: $id}) SET n.age = $val RETURN n',
    params: { id: 'alice', val: 30 },
  });
});

test('update path: empty nodeData issues no SET queries', () => {
  const { conn, writes } = makeConn(true);
  upsertNode(conn, 'alice', {});
  assert.equal(writes().length, 0);
});

test('update path: nodeData.id is skipped, id is never reassigned', () => {
  const { conn, writes } = makeConn(true);
  upsertNode(conn, 'alice', { id: 'override', name: 'A' });
  const w = writes();
  // Only name is SET; the id entry is skipped so the node keeps 'alice',
  // symmetric with the create path.
  assert.equal(w.length, 1);
  assert.deepEqual(w[0], {
    query: 'MATCH (n {id: $id}) SET n.name = $val RETURN n',
    params: { id: 'alice', val: 'A' },
  });
});

// --- Identifier validation (acceptance criterion #3) -----------------------
test('create path: bad label and bad property key are rejected', () => {
  {
    const { conn } = makeConn(false);
    assert.throws(() => upsertNode(conn, 'a', { name: 'x' }, 'bad label'), ValidationError);
  }
  {
    const { conn } = makeConn(false);
    assert.throws(() => upsertNode(conn, 'a', { 'bad key': 'x' }), ValidationError);
  }
});

test('update path: a bad property key is rejected', () => {
  const { conn } = makeConn(true);
  assert.throws(() => upsertNode(conn, 'a', { 'bad key': 'x' }), ValidationError);
});

// --- Integration (gated on a built extension) ------------------------------
function extensionAvailable(): boolean {
  try {
    graph(':memory:').close();
    return true;
  } catch {
    return false;
  }
}
const gate = { skip: !extensionAvailable() };

test('upsert creates then updates a node against the real extension', gate, () => {
  using g = graph(':memory:');

  g.upsertNode('alice', { name: 'Alice', age: 30 }, 'Person');
  assert.equal(g.hasNode('alice'), true);
  const created = g.getNode('alice') as { properties?: Record<string, unknown> } | null;
  assert.equal(created?.properties?.['name'], 'Alice');

  // Second upsert takes the update branch and changes a property.
  g.upsertNode('alice', { age: 31 });
  const updated = g.getNode('alice') as { properties?: Record<string, unknown> } | null;
  assert.equal(Number(updated?.properties?.['age']), 31);
  assert.equal(updated?.properties?.['name'], 'Alice'); // untouched
});
