// Graph query operations as pure functions.
//
// Each takes a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/graph/queries.py.
import type { Connection } from '../connection.ts';
import { sanitizeRelType } from '../utils.ts';
import type { CypherRow, CypherValue } from '../result.ts';

/** Degree (in + out) of a node. Empty result → `0` (mirrors queries.py:28-31). */
export function nodeDegree(conn: Connection, nodeId: string): number {
  const result = conn.cypher('MATCH (n {id: $id})-[r]-() RETURN count(r) AS degree', { id: nodeId });
  if (result.length === 0) {
    return 0;
  }
  const deg = result[0]?.['degree'];
  return deg ? Number(deg) : 0;
}

/** Distinct neighbouring nodes (either direction). Only truthy `m` cells kept. */
export function getNeighbors(conn: Connection, nodeId: string): CypherValue[] {
  const result = conn.cypher('MATCH (n {id: $id})-[]-(m) RETURN DISTINCT m', { id: nodeId });
  const neighbors: CypherValue[] = [];
  for (const row of result) {
    if (row['m']) {
      neighbors.push(row['m']);
    }
  }
  return neighbors;
}

/**
 * Edges touching a node as `[source, target, props]` **tuples** — deliberately a
 * different shape from the other query methods (acceptance #1). A missing `r`
 * cell becomes `{}` (mirrors queries.py:67).
 */
export function getNodeEdges(
  conn: Connection,
  nodeId: string,
): [string, string, Record<string, unknown>][] {
  const result = conn.cypher(
    'MATCH (n {id: $id})-[r]-(m) RETURN n.id AS source, m.id AS target, r',
    { id: nodeId },
  );
  return [...result].map((row) => [
    String(row['source']),
    String(row['target']),
    (row['r'] ?? {}) as Record<string, unknown>,
  ]);
}

/** Outgoing edges from a node as `{ source, target, r }` rows (`toList()`). */
export function getEdgesFrom(conn: Connection, nodeId: string): CypherRow[] {
  return conn
    .cypher('MATCH (a {id: $id})-[r]->(b) RETURN a.id AS source, b.id AS target, r', { id: nodeId })
    .toList();
}

/** Incoming edges to a node as `{ source, target, r }` rows (`toList()`). */
export function getEdgesTo(conn: Connection, nodeId: string): CypherRow[] {
  return conn
    .cypher('MATCH (a)-[r]->(b {id: $id}) RETURN a.id AS source, b.id AS target, r', { id: nodeId })
    .toList();
}

/** Outgoing edges of a given type from a node. `relType` is sanitized, then interpolated. */
export function getEdgesByType(conn: Connection, nodeId: string, relType: string): CypherRow[] {
  const safe = sanitizeRelType(relType);
  return conn
    .cypher(`MATCH (a {id: $id})-[r:${safe}]->(b) RETURN a.id AS source, b.id AS target, r`, {
      id: nodeId,
    })
    .toList();
}

/** Node/edge counts. Issues **two** count queries and renames `cnt` (mirrors queries.py:130-139). */
export function stats(conn: Connection): { nodeCount: number; edgeCount: number } {
  const nodes = conn.cypher('MATCH (n) RETURN count(n) AS cnt');
  const edges = conn.cypher('MATCH ()-[r]->() RETURN count(r) AS cnt');
  const nodeCnt = nodes.length > 0 ? nodes[0]?.['cnt'] : 0;
  const edgeCnt = edges.length > 0 ? edges[0]?.['cnt'] : 0;
  return {
    nodeCount: nodeCnt ? Number(nodeCnt) : 0,
    edgeCount: edgeCnt ? Number(edgeCnt) : 0,
  };
}

/** Run a raw Cypher query, passing the caller's string through unchanged. */
export function query(
  conn: Connection,
  cypher: string,
  params?: Record<string, unknown> | null,
): CypherRow[] {
  return conn.cypher(cypher, params).toList();
}
