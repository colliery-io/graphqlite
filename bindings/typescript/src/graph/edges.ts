// Edge read/delete/upsert operations as pure functions.
//
// Each takes a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/graph/edges.py.
import type { Connection } from '../connection.ts';
import { sanitizeRelType, assertIdentifier } from '../utils.ts';
import type { CypherRow, CypherValue } from '../result.ts';

/**
 * The `[r{...}]` relationship pattern. When a `relType` is given it is sanitized
 * (never validated/thrown — {@link sanitizeRelType} coerces to a safe token) and
 * interpolated as `:TYPE`; otherwise the pattern is bare `[r]`. Node ids are
 * always bound (`$src`/`$tgt`), never interpolated.
 */
function relPattern(relType?: string): string {
  return relType ? `:${sanitizeRelType(relType)}` : '';
}

/**
 * Whether an edge exists between two nodes (optionally of a given type). Parses
 * like {@link import('./nodes.ts').hasNode}: empty → false, falsy `cnt` → false,
 * else `Number(cnt) > 0` (mirrors edges.py:31-34).
 */
export function hasEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  relType?: string,
): boolean {
  const result = conn.cypher(
    `MATCH (a {id: $src})-[r${relPattern(relType)}]->(b {id: $tgt}) RETURN count(r) AS cnt`,
    { src: sourceId, tgt: targetId },
  );
  if (result.length === 0) {
    return false;
  }
  const cnt = result[0]?.['cnt'];
  return cnt ? Number(cnt) > 0 : false;
}

/** Fetch the edge between two nodes, or `null` if none. `r` is returned unmodified. */
export function getEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  relType?: string,
): CypherValue | null {
  const result = conn.cypher(
    `MATCH (a {id: $src})-[r${relPattern(relType)}]->(b {id: $tgt}) RETURN r`,
    { src: sourceId, tgt: targetId },
  );
  if (result.length === 0) {
    return null;
  }
  return result[0]?.['r'] ?? null;
}

/** Delete the edge(s) between two nodes (optionally of a given type). */
export function deleteEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  relType?: string,
): void {
  conn.cypher(`MATCH (a {id: $src})-[r${relPattern(relType)}]->(b {id: $tgt}) DELETE r`, {
    src: sourceId,
    tgt: targetId,
  });
}

/**
 * All edges as `{ source, target, r }` rows. The `toList()` result is returned
 * **unmodified** (mirrors edges.py:145-148); callers depend on those exact keys.
 */
export function getAllEdges(conn: Connection): CypherRow[] {
  const result = conn.cypher('MATCH (a)-[r]->(b) RETURN a.id AS source, b.id AS target, r');
  return result.toList();
}

/**
 * Create or update an edge via `MERGE` (mirrors edges.py:58-119). Unlike
 * `upsertNode`, this never calls `hasEdge`/`hasNode` — MERGE is idempotent.
 *
 * - **Step 1 (MERGE)** matches both nodes and merges the relationship. Without
 *   `edgeId` the merge key is `(source, target, relType)`; with `edgeId` it is
 *   the caller-assigned `id` property (parallel edges of the same type).
 * - **Step 2 (SET)** runs only when `edgeData` is non-empty, as a **single**
 *   query — values bound (`$v0`, `$v1`, …), property keys interpolated (contrast
 *   `upsertNode`, which issues one query per key).
 *
 * If either node is missing the MATCH yields nothing and the whole thing is a
 * silent no-op — no exception. `relType` defaults to `"RELATED"` and is
 * sanitized; every property key is validated with {@link assertIdentifier}.
 */
export function upsertEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  edgeData: Record<string, unknown>,
  relType: string = 'RELATED',
  edgeId?: string,
): void {
  const safe = sanitizeRelType(relType);
  const keys = Object.keys(edgeData);
  // Validate keys before any write so a bad key never leaves a dangling edge.
  for (const key of keys) {
    assertIdentifier(key, 'property');
  }

  let relMatch: string;
  let baseParams: Record<string, unknown>;
  if (edgeId === undefined) {
    conn.cypher(`MATCH (a {id: $src}), (b {id: $tgt}) MERGE (a)-[r:${safe}]->(b)`, {
      src: sourceId,
      tgt: targetId,
    });
    relMatch = `[r:${safe}]`;
    baseParams = { src: sourceId, tgt: targetId };
  } else {
    conn.cypher(`MATCH (a {id: $src}), (b {id: $tgt}) MERGE (a)-[r:${safe} {id: $eid}]->(b)`, {
      src: sourceId,
      tgt: targetId,
      eid: edgeId,
    });
    relMatch = `[r:${safe} {id: $eid}]`;
    baseParams = { src: sourceId, tgt: targetId, eid: edgeId };
  }

  if (keys.length > 0) {
    const params: Record<string, unknown> = { ...baseParams };
    const setParts = keys.map((key, i) => {
      const paramName = `v${i}`;
      params[paramName] = edgeData[key];
      return `r.${key} = $${paramName}`;
    });
    conn.cypher(
      `MATCH (a {id: $src})-${relMatch}->(b {id: $tgt}) SET ${setParts.join(', ')}`,
      params,
    );
  }
}
