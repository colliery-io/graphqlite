// Node read/delete/upsert operations as pure functions.
//
// Each takes a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/graph/nodes.py.
import type { Connection } from '../connection.ts';
import { assertIdentifier, formatProps } from '../utils.ts';
import type { CypherValue } from '../result.ts';

/**
 * Whether a node with the given `id` property exists.
 *
 * Parses defensively (matches nodes.py:26-29): empty result → false, a falsy
 * `cnt` → false, otherwise `Number(cnt) > 0`.
 */
export function hasNode(conn: Connection, nodeId: string): boolean {
  const result = conn.cypher('MATCH (n {id: $id}) RETURN count(n) AS cnt', { id: nodeId });
  if (result.length === 0) {
    return false;
  }
  const cnt = result[0]?.['cnt'];
  return cnt ? Number(cnt) > 0 : false;
}

/**
 * Fetch a node by its `id` property, or `null` if none. The `n` cell is returned
 * **unmodified** ({@link CypherValue}, the `{id, labels, properties}` shape) —
 * no reshaping, matching nodes.py:41-47.
 */
export function getNode(conn: Connection, nodeId: string): CypherValue | null {
  const result = conn.cypher('MATCH (n {id: $id}) RETURN n', { id: nodeId });
  if (result.length === 0) {
    return null;
  }
  return result[0]?.['n'] ?? null;
}

/** Delete a node and its relationships (`DETACH DELETE`). */
export function deleteNode(conn: Connection, nodeId: string): void {
  conn.cypher('MATCH (n {id: $id}) DETACH DELETE n', { id: nodeId });
}

/**
 * All nodes, optionally filtered by `label`. The label is **interpolated** into
 * the Cypher (not a bound param), so it is validated with {@link assertIdentifier}
 * first — a bad label raises `ValidationError`.
 *
 * The parse is deliberately dual-path (mirrors nodes.py:108-120): when a row
 * carries a string `result` key it is JSON-parsed and each `item.n` collected —
 * **parse failures are silently ignored** — otherwise a truthy `row.n` is
 * collected directly. Dropping this fallback yields an empty array for certain
 * result shapes (the raw-string wrapping from result.ts).
 */
export function getAllNodes(conn: Connection, label?: string): CypherValue[] {
  let result;
  if (label) {
    assertIdentifier(label, 'label');
    result = conn.cypher(`MATCH (n:${label}) RETURN n`);
  } else {
    result = conn.cypher('MATCH (n) RETURN n');
  }

  const nodes: CypherValue[] = [];
  for (const row of result) {
    const raw = row['result'];
    if (typeof raw === 'string') {
      try {
        const parsed = JSON.parse(raw);
        if (Array.isArray(parsed)) {
          for (const item of parsed) {
            if (item && typeof item === 'object' && 'n' in item) {
              nodes.push(item.n as CypherValue);
            }
          }
        }
      } catch {
        // Silently ignore non-JSON — matches Python's bare `except`.
      }
    } else if (row['n']) {
      nodes.push(row['n']);
    }
  }
  return nodes;
}

/**
 * Create a node, or update an existing one — dispatched on {@link hasNode}
 * (mirrors nodes.py:49-79).
 *
 * The two paths are **symmetric in how they treat `id`**: `nodeId` always wins.
 * - **Create** interpolates `{ id: nodeId, ...rest }` where `rest` is `nodeData`
 *   without any `id` key, so a `nodeData.id` is ignored and the node's id is
 *   exactly `nodeId`. The whole record is interpolated via {@link formatProps}
 *   in a single `CREATE`.
 * - **Update** SETs only the non-`id` `nodeData` entries — `id` is never
 *   reassigned, so it stays `nodeId` — and issues **one query per entry** (N
 *   round-trips; do not batch them).
 *
 * `label` (default `"Entity"`) is used only on creation. The label and every
 * interpolated property key are validated with {@link assertIdentifier} (the
 * core binding does this even though Python interpolates unchecked).
 */
export function upsertNode(
  conn: Connection,
  nodeId: string,
  nodeData: Record<string, unknown>,
  label: string = 'Entity',
): void {
  // `id` is the node identity: nodeId always wins, a nodeData.id is dropped.
  const rest = { ...nodeData };
  delete rest.id;
  const props: Record<string, unknown> = { id: nodeId, ...rest };

  if (hasNode(conn, nodeId)) {
    // Update: one query per entry, key interpolated, value bound. `id` is the
    // node identity and is never reassigned — a `nodeData.id` is skipped so the
    // node keeps `nodeId`, symmetric with the create path.
    for (const [key, value] of Object.entries(nodeData)) {
      if (key === 'id') continue;
      assertIdentifier(key, 'property');
      conn.cypher(`MATCH (n {id: $id}) SET n.${key} = $val RETURN n`, { id: nodeId, val: value });
    }
  } else {
    // Create: single interpolated CREATE. nodeId always wins over nodeData.id.
    assertIdentifier(label, 'label');
    for (const key of Object.keys(props)) {
      assertIdentifier(key, 'property');
    }
    const propStr = formatProps(props);
    conn.cypher(`CREATE (n:${label} {${propStr}})`);
  }
}
