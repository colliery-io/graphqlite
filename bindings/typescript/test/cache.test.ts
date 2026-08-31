import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadGraph, unloadGraph, reloadGraph, graphLoaded } from '../src/graph/cache.ts';
import { graph } from '../src/graph/index.ts';

// --- Connection test double -------------------------------------------------
// Records execute() SQL and (separately) any cypher() call, and returns queued
// JSON cells shaped like the real execute() output.
function makeConn(...cells: (string | null)[]) {
  const executeSql: string[] = [];
  let cypherCalls = 0;
  const queue = [...cells];
  const conn = {
    execute(sql: string): unknown[] {
      executeSql.push(sql);
      const cell = queue.shift();
      if (cell === undefined || cell === null) {
        return []; // no row
      }
      return [{ 'scalar()': cell }];
    },
    cypher() {
      cypherCalls += 1;
      throw new Error('cypher() must not be used by cache functions');
    },
  };
  return { conn: conn as never, executeSql, cypherCalls: () => cypherCalls };
}

// --- execute() path, not cypher() (acceptance #1) --------------------------
test('cache functions use execute() with the exact SQL, never cypher()', () => {
  const { conn, executeSql, cypherCalls } = makeConn(
    '{"status":"loaded","nodes":2,"edges":1}',
    '{"status":"unloaded"}',
    '{"status":"reloaded","nodes":2,"edges":1}',
    '{"loaded":true}',
  );
  loadGraph(conn);
  unloadGraph(conn);
  reloadGraph(conn);
  graphLoaded(conn);
  assert.deepEqual(executeSql, [
    'SELECT gql_load_graph()',
    'SELECT gql_unload_graph()',
    'SELECT gql_reload_graph()',
    'SELECT gql_graph_loaded()',
  ]);
  assert.equal(cypherCalls(), 0);
});

// --- Asymmetric rename (acceptance #2) -------------------------------------
test('loadGraph / reloadGraph rename nodes→nodeCount, edges→edgeCount', () => {
  assert.deepEqual(loadGraph(makeConn('{"status":"loaded","nodes":2,"edges":1}').conn), {
    status: 'loaded',
    nodeCount: 2,
    edgeCount: 1,
  });
  // reload keeps un-renamed keys (previous_*) untouched.
  assert.deepEqual(
    reloadGraph(
      makeConn('{"status":"reloaded","previous_nodes":0,"previous_edges":0,"nodes":3,"edges":2}')
        .conn,
    ),
    { status: 'reloaded', previous_nodes: 0, previous_edges: 0, nodeCount: 3, edgeCount: 2 },
  );
});

test('unloadGraph renames nodes/edges uniformly like load/reload (#67)', () => {
  // The core's unload response has no nodes/edges (so this is a no-op in
  // practice), but remapping is now applied uniformly: if the core ever did
  // return them, they are renamed the same way as load/reload.
  assert.deepEqual(unloadGraph(makeConn('{"status":"unloaded","nodes":2,"edges":1}').conn), {
    status: 'unloaded',
    nodeCount: 2,
    edgeCount: 1,
  });
});

// --- No row → {} (acceptance #3) -------------------------------------------
test('an empty result set yields an empty object', () => {
  assert.deepEqual(loadGraph(makeConn(null).conn), {});
  assert.deepEqual(unloadGraph(makeConn(null).conn), {});
  assert.equal(graphLoaded(makeConn(null).conn), false);
});

// --- graphLoaded boolean coercion ------------------------------------------
test('graphLoaded: loaded true → true; missing/false → false', () => {
  assert.equal(graphLoaded(makeConn('{"loaded":true}').conn), true);
  assert.equal(graphLoaded(makeConn('{"loaded":false}').conn), false);
  assert.equal(graphLoaded(makeConn('{"nodes":2}').conn), false); // no loaded key
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

test('cache load/unload lifecycle against the real extension', gate, () => {
  using g = graph(':memory:');
  g.upsertNode('alice', {}, 'Person');
  g.upsertNode('bob', {}, 'Person');
  g.upsertEdge('alice', 'bob', {}, 'KNOWS');

  assert.equal(g.graphLoaded(), false);

  const loaded = g.loadGraph();
  assert.equal(g.graphLoaded(), true);
  assert.equal(Number(loaded['nodeCount']), 2); // renamed
  assert.equal(Number(loaded['edgeCount']), 1);
  assert.equal(loaded['nodes'], undefined); // raw key gone

  const reloaded = g.reloadGraph();
  assert.ok('nodeCount' in reloaded);

  const unloaded = g.unloadGraph();
  assert.equal(unloaded['status'], 'unloaded');
  assert.equal(g.graphLoaded(), false);
});
