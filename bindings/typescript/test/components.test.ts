import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  weaklyConnectedComponents,
  stronglyConnectedComponents,
  wcc,
  connectedComponents,
  scc,
} from '../src/algorithms/components.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';

// --- Connection test double -------------------------------------------------
interface Call {
  query: string;
  params?: Record<string, unknown> | null;
}

function makeConn(columnName: string, rows: CypherRow[]) {
  const calls: Call[] = [];
  const conn = {
    cypher(query: string, params?: Record<string, unknown> | null): CypherResult {
      calls.push({ query, params });
      return new CypherResult([{ [columnName]: rows }], [columnName]);
    },
  };
  return { conn: conn as never, calls };
}

// --- Cypher parity ---------------------------------------------------------
test('weakly/stronglyConnectedComponents emit RETURN wcc() / scc() (no params)', () => {
  {
    const { conn, calls } = makeConn('wcc()', []);
    weaklyConnectedComponents(conn);
    assert.deepEqual(calls[0], { query: 'RETURN wcc()', params: undefined });
  }
  {
    const { conn, calls } = makeConn('scc()', []);
    stronglyConnectedComponents(conn);
    assert.equal(calls[0]?.query, 'RETURN scc()');
  }
});

// --- nodeId filter + keys (acceptance #1, #2) ------------------------------
test('collects only nodeId != null rows; keys nodeId/userId/component (safeInt)', () => {
  const { conn } = makeConn('wcc()', [
    { node_id: 'a', user_id: 'alice', component: 0 },
    { node_id: 'b', user_id: 'bob', component: '1' },
    { node_id: null, user_id: 'x', component: 2 }, // dropped — nodeId null
  ]);
  assert.deepEqual(weaklyConnectedComponents(conn), [
    { nodeId: 'a', userId: 'alice', component: 0 },
    { nodeId: 'b', userId: 'bob', component: 1 },
  ]);
});

// --- Aliases (acceptance #3) -----------------------------------------------
test('wcc / connectedComponents alias weaklyConnectedComponents; scc aliases strongly', () => {
  assert.equal(wcc, weaklyConnectedComponents);
  assert.equal(connectedComponents, weaklyConnectedComponents);
  assert.equal(scc, stronglyConnectedComponents);
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

test('finds components end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  for (const id of ['a', 'b', 'c', 'd']) g.upsertNode(id, {}, 'Person');
  // Two disconnected clusters: a-b and c-d.
  g.upsertEdge('a', 'b', {}, 'KNOWS');
  g.upsertEdge('c', 'd', {}, 'KNOWS');

  const w = g.weaklyConnectedComponents();
  assert.equal(w.length, 4);
  const componentCount = new Set(w.map((r) => r.component)).size;
  assert.equal(componentCount, 2); // two weakly connected components

  assert.deepEqual(g.wcc(), g.weaklyConnectedComponents());
  assert.deepEqual(g.connectedComponents(), g.weaklyConnectedComponents());

  const s = g.stronglyConnectedComponents();
  assert.ok(Array.isArray(s) && s.length === 4);
  assert.deepEqual(g.scc(), g.stronglyConnectedComponents());
});
