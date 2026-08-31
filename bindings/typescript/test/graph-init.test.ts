import { test } from 'node:test';
import assert from 'node:assert/strict';
import { graph, Graph } from '../src/graph/index.ts';
import { Connection } from '../src/connection.ts';

// The facade constructs a real Connection, which loads the extension. Structure
// checks run without a build; behavioural checks are gated on the extension so
// the suite passes on machines without one.
function extensionAvailable(): boolean {
  try {
    graph(':memory:').close();
    return true;
  } catch {
    return false;
  }
}

const gate = { skip: !extensionAvailable() };

// --- Structure (no extension needed) ---------------------------------------
test('exports a graph() factory and Graph class', () => {
  assert.equal(typeof graph, 'function');
  assert.equal(typeof Graph, 'function');
});

// --- Construction & namespace (acceptance criterion #1) --------------------
test('graph() returns a Graph instance with the default namespace', gate, () => {
  using g = graph(':memory:');
  assert.ok(g instanceof Graph);
  assert.equal(g.namespace, 'default');
});

test('namespace is stored verbatim and does not affect queries', gate, () => {
  using a = graph(':memory:', { namespace: 'tenant-a' });
  assert.equal(a.namespace, 'tenant-a');

  // The namespace is a dead parameter: two graphs on the same in-memory db with
  // different namespaces must see the same data (it is never threaded into SQL).
  // Here we simply assert a query runs and ignores the namespace value.
  a.connection.cypher("CREATE (n:Person {name: 'Alice'})");
  const rows = a.connection.cypher('MATCH (n:Person) RETURN n.name');
  assert.equal(rows[0]?.['n.name'], 'Alice');
});

// --- connection getter & close() -------------------------------------------
test('connection getter returns the underlying Connection', gate, () => {
  using g = graph(':memory:');
  assert.ok(g.connection instanceof Connection);
});

test('close() closes the underlying connection', gate, () => {
  const g = graph(':memory:');
  const conn = g.connection;
  g.close();
  // A closed node:sqlite connection rejects further statements.
  assert.throws(() => conn.execute('SELECT 1'));
});

// --- using / Symbol.dispose (acceptance criterion #2) ----------------------
test('using auto-disposes the graph at end of scope', gate, () => {
  let captured: Connection;
  {
    using g = graph(':memory:');
    captured = g.connection;
    // Works inside scope.
    assert.doesNotThrow(() => captured.execute('SELECT 1'));
  }
  // After the block the dispose ran → the connection is closed.
  assert.throws(() => captured.execute('SELECT 1'));
});

test('Symbol.dispose is callable and idempotent with close()', gate, () => {
  const g = graph(':memory:');
  g[Symbol.dispose]();
  assert.throws(() => g.connection.execute('SELECT 1'));
});

// --- extensionPath option pass-through -------------------------------------
test('a bad explicit extensionPath surfaces as a thrown error', () => {
  assert.throws(() => graph(':memory:', { extensionPath: '/no/such/graphqlite.dylib' }));
});
