// Graph traversal algorithms as pure functions.
//
// Each takes a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/algorithms/traversal.py.
import type { Connection } from '../connection.ts';
import { extractAlgoArray, parseTraversalResult, type TraversalResult } from './parsing.ts';
import { escapeString } from '../utils.ts';

export type { TraversalResult };

/** Options for {@link bfs} / {@link dfs}. */
export interface TraversalOptions {
  /**
   * Maximum depth to traverse. **Only `maxDepth < 0` means unlimited** —
   * `maxDepth === 0` is passed through as `bfs('x', 0)`, not treated as
   * unlimited (a common mistake). Defaults to `-1`.
   */
  maxDepth?: number;
}

// Shared body: `RETURN <fn>('<escaped start>')` when unlimited, else the
// 2-arg form with the depth interpolated raw (it's a number, not user text).
function traverse(
  conn: Connection,
  fn: 'bfs' | 'dfs',
  startId: string,
  maxDepth: number,
): TraversalResult[] {
  const start = escapeString(startId);
  const query = maxDepth < 0 ? `RETURN ${fn}('${start}')` : `RETURN ${fn}('${start}', ${maxDepth})`;

  const rows = extractAlgoArray(conn.cypher(query).toList());
  const nodes: TraversalResult[] = [];
  for (const row of rows) {
    const parsed = parseTraversalResult(row);
    if (parsed !== null) {
      nodes.push(parsed);
    }
  }
  return nodes;
}

/**
 * Breadth-first search from a starting node — level by level.
 *
 * @returns Rows of `{ user_id, depth, order }` in traversal order.
 */
export function bfs(
  conn: Connection,
  startId: string,
  options: TraversalOptions = {},
): TraversalResult[] {
  return traverse(conn, 'bfs', startId, options.maxDepth ?? -1);
}

/**
 * Depth-first search from a starting node — down each branch before backtracking.
 *
 * @returns Rows of `{ user_id, depth, order }` in traversal order.
 */
export function dfs(
  conn: Connection,
  startId: string,
  options: TraversalOptions = {},
): TraversalResult[] {
  return traverse(conn, 'dfs', startId, options.maxDepth ?? -1);
}

/** Alias for {@link bfs} (matches Python's `breadth_first_search`). */
export const breadthFirstSearch = bfs;

/** Alias for {@link dfs} (matches Python's `depth_first_search`). */
export const depthFirstSearch = dfs;
