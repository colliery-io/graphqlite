// In-memory CSR graph cache control.
//
// Unlike every other graph module these call **raw SQL scalar functions** via
// `execute()` — NOT `cypher()`. Mirrors bindings/python/src/graphqlite/graph/
// __init__.py:93-187. Loading the cache makes the graph algorithms run ~28x
// faster by avoiding repeated SQLite I/O.
import type { Connection } from '../connection.ts';

/** Parsed cache-status object (shape varies by function). */
export type CacheStatus = Record<string, unknown>;

/**
 * Run a scalar cache function through `execute()` and JSON-parse its single
 * cell. `execute()` returns rows like `[{ "gql_load_graph()": "<json>" }]`, so
 * the value is the first column of the first row. No row → `{}` (acceptance #3).
 */
function callCacheFn(conn: Connection, sql: string): CacheStatus {
  const rows = conn.execute(sql) as Record<string, unknown>[];
  const first = rows[0];
  if (!first) {
    return {};
  }
  const cell = Object.values(first)[0];
  if (typeof cell !== 'string') {
    return {};
  }
  try {
    const parsed: unknown = JSON.parse(cell);
    return parsed !== null && typeof parsed === 'object' ? (parsed as CacheStatus) : {};
  } catch {
    return {};
  }
}

/**
 * Remap the core's `nodes`/`edges` keys to `nodeCount`/`edgeCount`. Applied
 * uniformly to `loadGraph`/`reloadGraph`/`unloadGraph` (#67). The core's
 * `unload` response carries no `nodes`/`edges` keys, so this is a no-op there
 * today, but keeping it uniform removes the special case and future-proofs it.
 * Other keys (e.g. `previous_nodes`) are left untouched.
 */
function remapCacheStatus(status: CacheStatus): CacheStatus {
  if ('nodes' in status) {
    status['nodeCount'] = status['nodes'];
    delete status['nodes'];
  }
  if ('edges' in status) {
    status['edgeCount'] = status['edges'];
    delete status['edges'];
  }
  return status;
}

/** Load the graph into the in-memory CSR cache. Renames `nodes`/`edges`. */
export function loadGraph(conn: Connection): CacheStatus {
  return remapCacheStatus(callCacheFn(conn, 'SELECT gql_load_graph()'));
}

/** Free the cached graph. Renames `nodes`/`edges` uniformly (#67; no-op today). */
export function unloadGraph(conn: Connection): CacheStatus {
  return remapCacheStatus(callCacheFn(conn, 'SELECT gql_unload_graph()'));
}

/** Reload the cache with the latest data. Renames `nodes`/`edges`. */
export function reloadGraph(conn: Connection): CacheStatus {
  return remapCacheStatus(callCacheFn(conn, 'SELECT gql_reload_graph()'));
}

/** Whether the cache is currently loaded. Missing `loaded` key → `false`. */
export function graphLoaded(conn: Connection): boolean {
  const result = callCacheFn(conn, 'SELECT gql_graph_loaded()');
  return Boolean(result['loaded']);
}
