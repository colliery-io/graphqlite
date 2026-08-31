import { test } from 'node:test';
import assert from 'node:assert/strict';
import { upsertNodesBatch, upsertEdgesBatch } from '../src/graph/batch.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult } from '../src/result.ts';

// --- Connection test double -------------------------------------------------
interface Call {
  query: string;
  params?: Record<string, unknown> | null;
}

// `throwOn` lets a test simulate a mid-batch failure. Every count/hasNode query
// returns empty (→ node treated as new → CREATE path), which keeps queries
// deterministic without a real database.
function makeConn(throwOn?: (call: Call) => boolean) {
  const calls: Call[] = [];
  const conn = {
    cypher(query: string, params?: Record<string, unknown> | null): CypherResult {
      const call = { query, params };
      if (throwOn?.(call)) throw new Error('boom');
      calls.push(call);
      return new CypherResult([], []);
    },
  };
  return { conn: conn as never, calls };
}

// --- upsertNodesBatch: loop over upsertNode, in order -----------------------
test('upsertNodesBatch: one upsertNode per item, in order, tuple [id, props, label]', () => {
  const { conn, calls } = makeConn();
  upsertNodesBatch(conn, [
    ['a', {}, 'P'],
    ['b', { x: 1 }, 'Q'],
  ]);
  const creates = calls.filter((c) => c.query.startsWith('CREATE'));
  assert.equal(creates.length, 2);
  assert.match(creates[0]!.query, /CREATE \(n:P \{/);
  assert.match(creates[1]!.query, /CREATE \(n:Q \{/);
});

// --- No atomicity: a mid-batch throw leaves earlier items committed ---------
test('upsertNodesBatch: NOT atomic — failure on item 2 keeps item 1 committed', () => {
  // Throw on the second node's very first query (its hasNode count, params.id === 'b').
  const { conn, calls } = makeConn((c) => c.params?.['id'] === 'b');
  assert.throws(
    () =>
      upsertNodesBatch(conn, [
        ['a', {}, 'P'],
        ['b', {}, 'Q'],
      ]),
    /boom/,
  );
  // Item 'a' already created before the throw — no rollback, no transaction.
  assert.ok(calls.some((c) => /CREATE \(n:P \{/.test(c.query)));
  assert.ok(!calls.some((c) => /CREATE \(n:Q \{/.test(c.query)));
});

// --- upsertEdgesBatch: loop, and NO edgeId (no parallel edges) --------------
test('upsertEdgesBatch: one upsertEdge per item, MERGE without an edge id', () => {
  const { conn, calls } = makeConn();
  upsertEdgesBatch(conn, [
    ['a', 'b', {}, 'KNOWS'],
    ['b', 'c', { since: 2020 }, 'KNOWS'],
  ]);
  const merges = calls.filter((c) => c.query.includes('MERGE'));
  assert.equal(merges.length, 2);
  for (const m of merges) {
    // No edgeId → the MERGE has no `{id: $eid}` clause, so batches reuse the
    // existing edge instead of creating a parallel one.
    assert.match(m.query, /MERGE \(a\)-\[r:KNOWS\]->\(b\)/);
    assert.doesNotMatch(m.query, /\{id: \$eid\}/);
    assert.equal(m.params?.['eid'], undefined);
  }
});

// --- Integration (gated on a built extension) -------------------------------
function extensionAvailable(): boolean {
  try {
    graph(':memory:').close();
    return true;
  } catch {
    return false;
  }
}
const gate = { skip: !extensionAvailable() };

test('batch upserts end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  g.upsertNodesBatch([
    ['a', { name: 'A' }, 'P'],
    ['b', { name: 'B' }, 'P'],
    ['c', {}, 'P'],
  ]);
  assert.equal(g.getAllNodes().length, 3);

  g.upsertEdgesBatch([
    ['a', 'b', { w: 1 }, 'E'],
    ['b', 'c', {}, 'E'],
  ]);
  assert.equal(g.getAllEdges().length, 2);

  // No edgeId in batch → re-running the same edge does NOT add a parallel edge.
  g.upsertEdgesBatch([['a', 'b', { w: 2 }, 'E']]);
  assert.equal(g.getAllEdges().length, 2);
});
