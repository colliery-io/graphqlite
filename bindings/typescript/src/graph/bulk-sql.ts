import type {
  DatabaseSync as DatabaseSyncInstance,
  StatementSync,
} from 'node:sqlite';

interface PropertyKeyStatements {
  readonly select: StatementSync;
  readonly insert: StatementSync;
}

interface PropertyStatements {
  readonly bool: StatementSync;
  readonly int: StatementSync;
  readonly real: StatementSync;
  readonly text: StatementSync;
}

export interface NodeBulkStatements {
  readonly propertyKeys: PropertyKeyStatements;
  readonly properties: PropertyStatements;
  readonly insertNode: StatementSync;
  readonly insertLabel: StatementSync;
}

export interface EdgeBulkStatements {
  readonly propertyKeys: PropertyKeyStatements;
  readonly properties: PropertyStatements;
  readonly selectIdKey: StatementSync;
  readonly selectNodeId: StatementSync;
  readonly insertEdge: StatementSync;
}

interface PropertyWrite {
  readonly entityId: number;
  readonly keyId: number;
  readonly value: unknown;
}

function preparePropertyKeyStatements(db: DatabaseSyncInstance): PropertyKeyStatements {
  return {
    select: db.prepare('SELECT id FROM property_keys WHERE key = ?'),
    insert: db.prepare('INSERT INTO property_keys (key) VALUES (?)'),
  };
}

function preparePropertyStatements(
  db: DatabaseSyncInstance,
  entityType: 'node' | 'edge',
): PropertyStatements {
  const tablePrefix = entityType === 'node' ? 'node_props' : 'edge_props';
  const idColumn = entityType === 'node' ? 'node_id' : 'edge_id';
  return {
    bool: db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_bool (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ),
    int: db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_int (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ),
    real: db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_real (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ),
    text: db.prepare(
      `INSERT OR REPLACE INTO ${tablePrefix}_text (${idColumn}, key_id, value) VALUES (?, ?, ?)`,
    ),
  };
}

export function prepareNodeBulkStatements(db: DatabaseSyncInstance): NodeBulkStatements {
  return {
    propertyKeys: preparePropertyKeyStatements(db),
    properties: preparePropertyStatements(db, 'node'),
    insertNode: db.prepare('INSERT INTO nodes DEFAULT VALUES'),
    insertLabel: db.prepare(
      'INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)',
    ),
  };
}

export function prepareEdgeBulkStatements(db: DatabaseSyncInstance): EdgeBulkStatements {
  return {
    propertyKeys: preparePropertyKeyStatements(db),
    properties: preparePropertyStatements(db, 'edge'),
    selectIdKey: db.prepare("SELECT id FROM property_keys WHERE key = 'id'"),
    selectNodeId: db.prepare(
      'SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?',
    ),
    insertEdge: db.prepare('INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)'),
  };
}

export function ensurePreparedPropertyKey(
  statements: PropertyKeyStatements,
  key: string,
): number {
  const row = statements.select.get(key);
  const id = row?.['id'];
  if (typeof id === 'number') {
    return id;
  }
  return Number(statements.insert.run(key).lastInsertRowid);
}

export function lookupPreparedNodeId(
  statements: EdgeBulkStatements,
  externalId: string,
): number {
  const idKey = statements.selectIdKey.get()?.['id'];
  if (typeof idKey !== 'number') {
    throw new Error(`Node with id '${externalId}' not found (no 'id' property key)`);
  }
  const nodeId = statements.selectNodeId.get(idKey, externalId)?.['node_id'];
  if (typeof nodeId !== 'number') {
    throw new Error(`Node with id '${externalId}' not found`);
  }
  return nodeId;
}

export function insertPreparedProperty(
  statements: PropertyStatements,
  write: PropertyWrite,
): void {
  const { entityId, keyId, value } = write;
  if (typeof value === 'boolean') {
    statements.bool.run(entityId, keyId, value ? 1 : 0);
  } else if (typeof value === 'number' && Number.isInteger(value)) {
    statements.int.run(entityId, keyId, value);
  } else if (typeof value === 'number') {
    statements.real.run(entityId, keyId, value);
  } else {
    statements.text.run(entityId, keyId, String(value));
  }
}

export function ensurePropertyKey(db: DatabaseSyncInstance, key: string): number {
  return ensurePreparedPropertyKey(preparePropertyKeyStatements(db), key);
}

export function lookupNodeId(db: DatabaseSyncInstance, externalId: string): number {
  return lookupPreparedNodeId(prepareEdgeBulkStatements(db), externalId);
}

export function insertProperty(
  db: DatabaseSyncInstance,
  entityType: 'node' | 'edge',
  entityId: number,
  keyId: number,
  value: unknown,
): void {
  insertPreparedProperty(preparePropertyStatements(db, entityType), {
    entityId,
    keyId,
    value,
  });
}
