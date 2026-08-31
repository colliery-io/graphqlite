// Batch upsert helpers — pure loops over upsertNode / upsertEdge.
//
// These take a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/graph/batch.py.
//
// **No atomicity.** Each item is a separate upsert; if one throws partway
// through, the earlier items are already committed. These do NOT wrap the loop
// in a transaction — a deliberate reproduction of the Python behavior. For
// atomic bulk inserts, a future bulk API would be the place, not here.
import type { Connection } from '../connection.ts';
import { upsertNode } from './nodes.ts';
import { upsertEdge } from './edges.ts';

/** `[nodeId, props, label]` — one node for {@link upsertNodesBatch}. */
export type NodeBatchItem = [string, Record<string, unknown>, string];

/** `[sourceId, targetId, props, relType]` — one edge for {@link upsertEdgesBatch}. */
export type EdgeBatchItem = [string, string, Record<string, unknown>, string];

/**
 * Upsert many nodes by calling {@link upsertNode} once per item.
 *
 * Not atomic: a throw partway through leaves earlier nodes committed.
 */
export function upsertNodesBatch(conn: Connection, nodes: NodeBatchItem[]): void {
  for (const [nodeId, props, label] of nodes) {
    upsertNode(conn, nodeId, props, label);
  }
}

/**
 * Upsert many edges by calling {@link upsertEdge} once per item.
 *
 * Not atomic (see {@link upsertNodesBatch}). Note this does **not** pass an
 * `edgeId`, so a batch cannot create parallel edges between the same pair —
 * MERGE reuses the existing edge. This matches the Python binding.
 */
export function upsertEdgesBatch(conn: Connection, edges: EdgeBatchItem[]): void {
  for (const [sourceId, targetId, props, relType] of edges) {
    upsertEdge(conn, sourceId, targetId, props, relType);
  }
}
