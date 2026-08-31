import { test } from 'node:test';
import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { existsSync } from 'node:fs';
import type { DatabaseSync as DatabaseSyncInstance } from 'node:sqlite';
import {
  insertNodesBulk,
  insertEdgesBulk,
  insertGraphBulk,
  resolveNodeIds,
  sanitizeRelType,
} from '../src/graph/bulk.ts';
import { graph, type Graph } from '../src/graph/index.ts';

// ── sanitizeRelType (bulk-only) — pure, no DB ────────────────────────────────
// Deliberately weaker than utils.ts's version: no reserved-word check, and an
// empty result becomes "REL" (not "REL_").
test('sanitizeRelType: alphanumerics and underscore pass through unchanged', () => {
  assert.equal(sanitizeRelType('KNOWS'), 'KNOWS');
  assert.equal(sanitizeRelType('rel_type_1'), 'rel_type_1');
});

test('sanitizeRelType: non-alphanumeric chars become underscores', () => {
  assert.equal(sanitizeRelType('a-b c!'), 'a_b_c_');
  assert.equal(sanitizeRelType('!!!'), '___'); // not empty → NOT collapsed to REL
});

test('sanitizeRelType: leading digit gets a REL_ prefix', () => {
  assert.equal(sanitizeRelType('123'), 'REL_123');
  assert.equal(sanitizeRelType('9x'), 'REL_9x');
});

test('sanitizeRelType: empty string becomes REL (not REL_)', () => {
  assert.equal(sanitizeRelType(''), 'REL');
});

test('sanitizeRelType: reserved words are NOT rewritten (unlike utils version)', () => {
  // utils.sanitizeRelType would turn 'MATCH' into 'REL_MATCH'; the bulk one leaves it.
  assert.equal(sanitizeRelType('MATCH'), 'MATCH');
  assert.equal(sanitizeRelType('CREATE'), 'CREATE');
});

test('sanitizeRelType: Unicode letters survive (matches Python str.isalnum)', () => {
  assert.equal(sanitizeRelType('관계'), '관계');
});

// ── Integration tests (gated on the staged extension) ─────────────────────────
const DYLIB =
  process.env.GRAPHQLITE_EXTENSION_PATH ??
  resolve(import.meta.dirname, '..', 'npm', 'darwin-arm64', 'graphqlite.dylib');
const gate = { skip: !existsSync(DYLIB) };

function newGraph(): Graph {
  return graph(':memory:', { extensionPath: DYLIB });
}

/** Which typed table holds the given (externalId, key) property, or null. */
function propTable(db: DatabaseSyncInstance, externalId: string, key: string): string | null {
  const idKeyRow = db.prepare("SELECT id FROM property_keys WHERE key = 'id'").get() as
    | { id?: number }
    | undefined;
  if (!idKeyRow || idKeyRow.id === undefined) return null;
  const nodeRow = db
    .prepare('SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?')
    .get(idKeyRow.id, externalId) as { node_id?: number } | undefined;
  if (!nodeRow || nodeRow.node_id === undefined) return null;
  const keyRow = db.prepare('SELECT id FROM property_keys WHERE key = ?').get(key) as
    | { id?: number }
    | undefined;
  if (!keyRow || keyRow.id === undefined) return null;
  for (const suffix of ['int', 'real', 'text', 'bool']) {
    const r = db
      .prepare(`SELECT 1 AS x FROM node_props_${suffix} WHERE node_id = ? AND key_id = ?`)
      .get(nodeRow.node_id, keyRow.id);
    if (r !== undefined) return suffix;
  }
  return null;
}

test('insertNodesBulk: returns externalId→rowid, stores id/label/props', gate, () => {
  using g = newGraph();
  const idMap = insertNodesBulk(g.connection, [
    ['alice', { name: 'Alice', age: 30 }, 'Person'],
    ['bob', { name: 'Bob' }, 'Person'],
  ]);
  assert.equal(idMap.size, 2);
  assert.ok(idMap.has('alice'));
  assert.ok(idMap.has('bob'));
  assert.equal(typeof idMap.get('alice'), 'number');

  // The external id is stored as the `id` text property, so getNode finds it.
  const node = g.getNode('alice') as { properties?: Record<string, unknown> } | null;
  assert.ok(node);
  assert.equal(g.getAllNodes('Person').length, 2);
});

// AC#2 — the load-bearing 1.0→int divergence, proven by raw SELECT.
test('insertProperty: JS 1.0 lands in node_props_int (NOT real) — raw SELECT proof', gate, () => {
  using g = newGraph();
  insertNodesBulk(g.connection, [
    ['n1', { whole: 1.0, frac: 1.5, flag: true, name: 'hi', count: 3 }, 'T'],
  ]);
  const db = g.connection.database;

  // The crux: 1.0 is Number.isInteger → int table, unlike Python (real).
  assert.equal(propTable(db, 'n1', 'whole'), 'int');
  // And the value really is there as an integer 1.
  const row = db
    .prepare(
      "SELECT npi.value AS v FROM node_props_int npi " +
        'JOIN property_keys pk ON pk.id = npi.key_id ' +
        "WHERE pk.key = 'whole' AND npi.node_id = ?",
    )
    .get(1) as { v?: number } | undefined;
  assert.equal(row?.v, 1);

  // Sanity on the other branches of the type ladder.
  assert.equal(propTable(db, 'n1', 'frac'), 'real');
  assert.equal(propTable(db, 'n1', 'flag'), 'bool');
  assert.equal(propTable(db, 'n1', 'name'), 'text');
  assert.equal(propTable(db, 'n1', 'count'), 'int');
});

test('insertEdgesBulk: resolves via idMap, fallback cache, then DB lookup', gate, () => {
  using g = newGraph();
  const idMap = insertNodesBulk(g.connection, [
    ['a', {}, 'N'],
    ['b', {}, 'N'],
  ]);
  // 'c' is NOT in idMap → forces a DB lookup path.
  insertNodesBulk(g.connection, [['c', {}, 'N']]);

  const inserted = insertEdgesBulk(
    g.connection,
    [
      ['a', 'b', { weight: 1 }, 'KNOWS'],
      ['b', 'c', {}, 'KNOWS'], // b via idMap, c via lookup then cache
      ['c', 'a', {}, 'KNOWS'], // c via fallback cache, a via idMap
    ],
    idMap,
  );
  assert.equal(inserted, 3);
  assert.equal(g.getAllEdges().length, 3);
});

test('insertEdgesBulk: throws (after ROLLBACK) when an endpoint cannot be resolved', gate, () => {
  using g = newGraph();
  insertNodesBulk(g.connection, [['a', {}, 'N']]);
  assert.throws(
    () => insertEdgesBulk(g.connection, [['a', 'ghost', {}, 'KNOWS']]),
    /not found/,
  );
  // Nothing committed.
  assert.equal(g.getAllEdges().length, 0);
});

test('insertGraphBulk: nodesInserted = idMap.size, shrinks on duplicate external ids', gate, () => {
  using g = newGraph();
  const result = insertGraphBulk(
    g.connection,
    [
      ['x', { name: 'X1' }, 'N'],
      ['y', { name: 'Y' }, 'N'],
      ['x', { name: 'X2' }, 'N'], // duplicate external id → 3 rows, but idMap size 2
    ],
    [['x', 'y', {}, 'LINKS']],
  );
  // Three node ROWS were written, but nodesInserted counts distinct external ids.
  assert.equal(result.nodesInserted, 2);
  assert.equal(result.idMap.size, 2);
  assert.equal(result.edgesInserted, 1);
  // Proof the row count really is 3 (larger than nodesInserted).
  const cnt = g.connection.database
    .prepare('SELECT COUNT(*) AS c FROM nodes')
    .get() as { c?: number };
  assert.equal(cnt.c, 3);
});

test('insertNodesBulk: mid-transaction failure rolls back and re-throws (quirk #1)', gate, () => {
  using g = newGraph();
  assert.throws(() => {
    // Second item is malformed (null) → destructuring throws inside the tx.
    insertNodesBulk(g.connection, [['good', {}, 'N'], null as unknown as never]);
  });
  // ROLLBACK undid the 'good' insert — no partial state survives.
  assert.equal(g.getAllNodes().length, 0);
  assert.equal(resolveNodeIds(g.connection, ['good']).size, 0);
});

test('resolveNodeIds: empty when no id key; resolves existing, omits unknown', gate, () => {
  using empty = newGraph();
  // No nodes written at all → no `id` property key → empty map.
  assert.equal(resolveNodeIds(empty.connection, ['anyone']).size, 0);

  using g = newGraph();
  insertNodesBulk(g.connection, [
    ['alice', {}, 'N'],
    ['bob', {}, 'N'],
  ]);
  const resolved = resolveNodeIds(g.connection, ['alice', 'bob', 'nobody']);
  assert.equal(resolved.size, 2);
  assert.ok(resolved.has('alice'));
  assert.ok(resolved.has('bob'));
  assert.ok(!resolved.has('nobody'));
});

test('insertNodesBulk / insertEdgesBulk: empty input is a no-op', gate, () => {
  using g = newGraph();
  assert.equal(insertNodesBulk(g.connection, []).size, 0);
  assert.equal(insertEdgesBulk(g.connection, []), 0);
});
