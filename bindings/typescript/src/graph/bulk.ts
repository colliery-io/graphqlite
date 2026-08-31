// Bulk insert operations for high-performance graph construction.
//
// These bypass Cypher entirely and write **raw SQL** straight into the internal
// schema (nodes, node_labels, edges, property_keys, node_props_{text,int,real,
// bool}, edge_props_{…}). They are the only part of the binding coupled to that
// schema, so they live apart from the Cypher-based feature modules.
//
// Like batch.ts these take a Connection first (so Graph could delegate in three
// lines). The raw driver handle is reached via `conn.database` — the escape
// hatch `node:sqlite` DatabaseSync (connection.ts:220-223).
//
// Mirrors bindings/python/src/graphqlite/graph/bulk.py **1:1, quirks included**:
//   * BEGIN IMMEDIATE → work → COMMIT; on any throw, ROLLBACK then re-throw.
//   * Typed-property dispatch reproduces Python's isinstance ladder — see the
//     load-bearing 1.0→int note on {@link insertProperty}.
//   * {@link sanitizeRelType} is a **separate, weaker** sanitizer than
//     utils.ts's (no reserved-word check; empty → "REL", not "REL_"). Do not
//     merge the two.
import type { Connection } from '../connection.ts';
import type { DatabaseSync as DatabaseSyncInstance } from 'node:sqlite';

/** `[externalId, props, label]` — one node for {@link insertNodesBulk}. */
export type BulkNodeItem = [string, Record<string, unknown>, string];

/** `[sourceId, targetId, props, relType]` — one edge for {@link insertEdgesBulk}. */
export type BulkEdgeItem = [string, string, Record<string, unknown>, string];

/** Result of {@link insertGraphBulk}. */
export interface BulkInsertResult {
  /**
   * Number of nodes inserted. **This is `idMap.size`, not the row count** — if
   * the input has duplicate external IDs, later ones overwrite the map entry so
   * this is *smaller* than the number of `nodes` rows actually written. A
   * deliberate reproduction of Python's `len(id_map)` (bulk.py:264).
   */
  nodesInserted: number;
  /** Number of edge rows inserted. */
  edgesInserted: number;
  /** Mapping from external node IDs to internal SQLite rowids. */
  idMap: Map<string, number>;
}

// Unicode-aware character classes matching Python's str.isalnum() (letters +
// numbers) and str.isdigit() (decimal digits). Kept identical to utils.ts so a
// CJK relationship type survives sanitization on both paths.
const IDENTIFIER_CHAR = /[\p{L}\p{N}_]/u;
const DECIMAL_DIGIT = /\p{Nd}/u;

/**
 * Insert many nodes in one transaction, returning `externalId → internal rowid`.
 *
 * Reproduces bulk.py:46-126: ensure the `id` property key first, then per node
 * insert a `nodes` row, its label (`INSERT OR IGNORE`), its external id into
 * `node_props_text` under the `id` key, and every other property via
 * {@link insertProperty}. The property-key cache is kept for the whole
 * transaction. On any throw the transaction is rolled back and the error
 * re-thrown (quirk #1).
 */
export function insertNodesBulk(conn: Connection, nodes: BulkNodeItem[]): Map<string, number> {
  const idMap = new Map<string, number>();
  if (nodes.length === 0) {
    return idMap;
  }

  const db = conn.database;
  db.exec('BEGIN IMMEDIATE');
  try {
    const idKeyId = ensurePropertyKey(db, 'id');
    // Property-key cache within this transaction (quirk #6). Seeded with `id`.
    const propKeyCache = new Map<string, number>([['id', idKeyId]]);

    for (const [externalId, props, label] of nodes) {
      const info = db.prepare('INSERT INTO nodes DEFAULT VALUES').run();
      const nodeId = Number(info.lastInsertRowid);
      idMap.set(externalId, nodeId);

      db.prepare('INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)').run(
        nodeId,
        label,
      );

      // The external id is stored as the `id` text property (quirk #6).
      db.prepare(
        'INSERT OR REPLACE INTO node_props_text (node_id, key_id, value) VALUES (?, ?, ?)',
      ).run(nodeId, idKeyId, externalId);

      for (const [key, value] of Object.entries(props)) {
        let keyId = propKeyCache.get(key);
        if (keyId === undefined) {
          keyId = ensurePropertyKey(db, key);
          propKeyCache.set(key, keyId);
        }
        insertProperty(db, 'node', nodeId, keyId, value);
      }
    }

    db.exec('COMMIT');
  } catch (error) {
    db.exec('ROLLBACK');
    throw error;
  }

  return idMap;
}

/**
 * Insert many edges, resolving endpoints without MATCH queries.
 *
 * Resolution order per endpoint (quirk #4, bulk.py:182-198): the provided
 * `idMap` → a per-transaction fallback cache → a DB lookup via
 * {@link lookupNodeId} (whose result is then cached). A missing node throws
 * (propagated after ROLLBACK). Returns the number of edge rows inserted.
 */
export function insertEdgesBulk(
  conn: Connection,
  edges: BulkEdgeItem[],
  idMap?: Map<string, number>,
): number {
  if (edges.length === 0) {
    return 0;
  }

  const map = idMap ?? new Map<string, number>();
  const db = conn.database;
  db.exec('BEGIN IMMEDIATE');
  try {
    const propKeyCache = new Map<string, number>();
    // Cache for endpoints not present in the provided map.
    const fallbackCache = new Map<string, number>();
    let edgesInserted = 0;

    for (const [source, target, props, relType] of edges) {
      const safeRelType = sanitizeRelType(relType);
      const sourceId = resolveEndpoint(db, source, map, fallbackCache);
      const targetId = resolveEndpoint(db, target, map, fallbackCache);

      const info = db
        .prepare('INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)')
        .run(sourceId, targetId, safeRelType);
      const edgeId = Number(info.lastInsertRowid);
      edgesInserted += 1;

      for (const [key, value] of Object.entries(props)) {
        let keyId = propKeyCache.get(key);
        if (keyId === undefined) {
          keyId = ensurePropertyKey(db, key);
          propKeyCache.set(key, keyId);
        }
        insertProperty(db, 'edge', edgeId, keyId, value);
      }
    }

    db.exec('COMMIT');
    return edgesInserted;
  } catch (error) {
    db.exec('ROLLBACK');
    throw error;
  }
}

/**
 * Convenience over {@link insertNodesBulk} + {@link insertEdgesBulk}.
 *
 * Note `nodesInserted` is `idMap.size`, not the row count — see
 * {@link BulkInsertResult.nodesInserted} (quirk #5, bulk.py:260-267).
 */
export function insertGraphBulk(
  conn: Connection,
  nodes: BulkNodeItem[],
  edges: BulkEdgeItem[],
): BulkInsertResult {
  const idMap = insertNodesBulk(conn, nodes);
  const edgesInserted = insertEdgesBulk(conn, edges, idMap);
  return {
    nodesInserted: idMap.size,
    edgesInserted,
    idMap,
  };
}

/**
 * Resolve external node IDs to internal rowids (bulk.py:269-319).
 *
 * If no `id` property key exists yet, returns an empty map (no nodes were ever
 * written). Otherwise looks up each external id and includes only the ones that
 * exist — unknown IDs are simply absent from the result (quirk #7).
 */
export function resolveNodeIds(conn: Connection, externalIds: string[]): Map<string, number> {
  const result = new Map<string, number>();
  if (externalIds.length === 0) {
    return result;
  }

  const db = conn.database;
  const idKeyRow = db.prepare("SELECT id FROM property_keys WHERE key = 'id'").get() as
    | { id?: number }
    | undefined;
  if (!idKeyRow || idKeyRow.id === undefined) {
    return result; // No 'id' property key means no nodes.
  }
  const idKeyId = idKeyRow.id;

  const stmt = db.prepare('SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?');
  for (const externalId of externalIds) {
    const row = stmt.get(idKeyId, externalId) as { node_id?: number } | undefined;
    if (row && row.node_id !== undefined) {
      result.set(externalId, row.node_id);
    }
  }

  return result;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

/** Ensure a property key row exists, returning its id (bulk.py:323-333). */
export function ensurePropertyKey(db: DatabaseSyncInstance, key: string): number {
  const row = db.prepare('SELECT id FROM property_keys WHERE key = ?').get(key) as
    | { id?: number }
    | undefined;
  if (row && row.id !== undefined) {
    return row.id;
  }
  const info = db.prepare('INSERT INTO property_keys (key) VALUES (?)').run(key);
  return Number(info.lastInsertRowid);
}

/** Look up a node's internal rowid by external id, throwing if absent (bulk.py:335-354). */
export function lookupNodeId(db: DatabaseSyncInstance, externalId: string): number {
  const idKeyRow = db.prepare("SELECT id FROM property_keys WHERE key = 'id'").get() as
    | { id?: number }
    | undefined;
  if (!idKeyRow || idKeyRow.id === undefined) {
    throw new Error(`Node with id '${externalId}' not found (no 'id' property key)`);
  }
  const idKeyId = idKeyRow.id;

  const row = db
    .prepare('SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?')
    .get(idKeyId, externalId) as { node_id?: number } | undefined;
  if (!row || row.node_id === undefined) {
    throw new Error(`Node with id '${externalId}' not found`);
  }
  return row.node_id;
}

/** idMap → fallback cache → DB lookup (cached). Mirrors bulk.py:182-198. */
function resolveEndpoint(
  db: DatabaseSyncInstance,
  externalId: string,
  idMap: Map<string, number>,
  fallbackCache: Map<string, number>,
): number {
  if (idMap.has(externalId)) {
    return idMap.get(externalId)!;
  }
  if (fallbackCache.has(externalId)) {
    return fallbackCache.get(externalId)!;
  }
  const nodeId = lookupNodeId(db, externalId);
  fallbackCache.set(externalId, nodeId);
  return nodeId;
}

/**
 * Insert a property value into the typed table for its JS runtime type
 * (bulk.py:356-381). The branch order is load-bearing:
 *
 *   1. `typeof v === 'boolean'`     → `*_bool`  (1/0)
 *   2. `Number.isInteger(v)`        → `*_int`
 *   3. `typeof v === 'number'`      → `*_real`
 *   4. otherwise                    → `*_text`  (`String(v)`)
 *
 * **Deliberate divergence from Python (AC#2):** JavaScript has no int/float
 * distinction — `1.0 === 1` and `Number.isInteger(1.0)` is `true`, so a value
 * written as `1.0` lands in `*_int` here, whereas Python's `isinstance(1.0,
 * float)` sends it to `*_real`. This is intentional, documented, and covered by
 * the `bulk-float-int-quirk` parity scenario + allowlist entry.
 */
export function insertProperty(
  db: DatabaseSyncInstance,
  entityType: 'node' | 'edge',
  entityId: number,
  keyId: number,
  value: unknown,
): void {
  const tablePrefix = entityType === 'node' ? 'node_props' : 'edge_props';
  const idColumn = entityType === 'node' ? 'node_id' : 'edge_id';

  if (typeof value === 'boolean') {
    db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_bool (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ).run(entityId, keyId, value ? 1 : 0);
  } else if (typeof value === 'number' && Number.isInteger(value)) {
    db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_int (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ).run(entityId, keyId, value);
  } else if (typeof value === 'number') {
    db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_real (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ).run(entityId, keyId, value);
  } else {
    db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_text (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ).run(entityId, keyId, String(value));
  }
}

/**
 * Bulk-only relationship-type sanitizer (bulk.py:383-396).
 *
 * **Kept separate from utils.ts `sanitizeRelType` on purpose.** This one has
 * NO Cypher-reserved-word check, and an empty result becomes `"REL"` (utils
 * uses `"REL_"`). Rules:
 *   - every char that is not a letter/number/`_` → `_`;
 *   - leading decimal digit → prefix `REL_`;
 *   - empty string → `REL`.
 *
 * Unicode-aware to match Python's `str.isalnum()` / `str.isdigit()`.
 */
export function sanitizeRelType(relType: string): string {
  const chars = Array.from(relType, (c) => (IDENTIFIER_CHAR.test(c) ? c : '_'));
  let safe = chars.join('');
  if (safe.length > 0 && chars[0] !== undefined && DECIMAL_DIGIT.test(chars[0])) {
    safe = 'REL_' + safe;
  }
  if (safe.length === 0) {
    safe = 'REL';
  }
  return safe;
}
