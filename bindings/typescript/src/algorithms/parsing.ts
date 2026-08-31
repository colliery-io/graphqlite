// Shared parsing helpers for algorithm results.
//
// Ported from bindings/python/src/graphqlite/algorithms/_parsing.py. Kept
// behaviorally identical to Python, including a known quirk (see below).

// Known column names for graph algorithm results.
//
// The core wraps every algorithm result under `column_0`, which is first in
// this list, so `extractAlgoArray` matches it first and the remaining named
// entries act only as a fallback for a hypothetical directly-named column. The
// names below were snake_case (`pagerank()`, …) but the queries the algorithm
// modules actually emit are camelCase (`pageRank(...)`, …); the casing is now
// corrected to match the real function names (#65). Runtime behavior is
// unchanged — `column_0` still matches first for current core output.
export const ALGO_COLUMN_NAMES: readonly string[] = [
  'column_0', 'wcc()', 'scc()', 'pageRank()', 'degreeCentrality()',
  'betweennessCentrality()', 'closenessCentrality()', 'eigenvectorCentrality()',
  'labelPropagation()', 'louvain()', 'bfs()', 'dfs()', 'apsp()',
];

export type AlgoRow = Record<string, unknown>;

/**
 * Extract wrapped array results from graph algorithms.
 *
 * Two formats: (1) old — multiple rows with fields directly accessible;
 * (2) new — a single row whose column holds an array of objects. Detects the
 * new format and returns the inner array; otherwise returns the input as-is.
 */
export function extractAlgoArray(result: AlgoRow[]): AlgoRow[] {
  // Multiple (or zero) rows: assume old format, return as-is.
  if (result.length !== 1) {
    return result;
  }

  const row = result[0]!;
  for (const colName of ALGO_COLUMN_NAMES) {
    const value = row[colName];
    if (Array.isArray(value)) {
      return value as AlgoRow[];
    }
  }

  // No array column found.
  return result;
}

/** Safely convert a value to a float, returning `defaultValue` on null/failure. */
export function safeFloat(val: unknown, defaultValue = 0.0): number {
  if (val === null || val === undefined) {
    return defaultValue;
  }
  if (typeof val === 'boolean') {
    return val ? 1 : 0;
  }
  if (typeof val === 'string' && val.trim() === '') {
    return defaultValue;
  }
  const n = Number(val);
  return Number.isNaN(n) ? defaultValue : n;
}

/** Safely convert a value to an int, returning `defaultValue` on null/failure. */
export function safeInt(val: unknown, defaultValue = 0): number {
  if (val === null || val === undefined) {
    return defaultValue;
  }
  if (typeof val === 'boolean') {
    return val ? 1 : 0;
  }
  if (typeof val === 'number') {
    return Number.isNaN(val) ? defaultValue : Math.trunc(val);
  }
  if (typeof val === 'string') {
    const trimmed = val.trim();
    // Python int() accepts integer strings but raises on "1.5"; mirror that.
    if (/^[+-]?\d+$/.test(trimmed)) {
      return Number.parseInt(trimmed, 10);
    }
    return defaultValue;
  }
  return defaultValue;
}

export interface ScoreResult {
  node_id: string;
  user_id: unknown;
  score: number;
}

/** Parse a result row with node_id, user_id, and a score field. */
export function parseScoreResult(row: AlgoRow, scoreKey = 'score'): ScoreResult | null {
  const nodeId = row['node_id'];
  const userId = row['user_id'];
  const score = row[scoreKey];

  if (nodeId === null || nodeId === undefined) {
    return null;
  }

  return {
    node_id: String(nodeId),
    user_id: userId,
    score: score !== null && score !== undefined ? safeFloat(score) : 0.0,
  };
}

export interface CommunityResult {
  node_id: string;
  user_id: unknown;
  community: number;
}

/** Parse a community detection result row. */
export function parseCommunityResult(row: AlgoRow): CommunityResult | null {
  const nodeId = row['node_id'];
  const userId = row['user_id'];
  const community = row['community'];

  if (nodeId === null || nodeId === undefined || community === null || community === undefined) {
    return null;
  }

  return {
    node_id: String(nodeId),
    user_id: userId,
    // Python: int(community) if community else 0 — falsy (incl. 0) -> 0.
    community: community ? safeInt(community) : 0,
  };
}

export interface ComponentResult {
  node_id: string;
  user_id: unknown;
  component: number;
}

/** Parse a connected components result row. */
export function parseComponentResult(row: AlgoRow): ComponentResult | null {
  const nodeId = row['node_id'];
  const userId = row['user_id'];
  const component = row['component'];

  if (nodeId === null || nodeId === undefined) {
    return null;
  }

  return {
    node_id: String(nodeId),
    user_id: userId,
    // Python: int(component) if component is not None else 0 — 0 stays 0.
    component: component !== null && component !== undefined ? safeInt(component) : 0,
  };
}

export interface TraversalResult {
  user_id: unknown;
  depth: number;
  order: number;
}

/** Parse a BFS/DFS traversal result row. */
export function parseTraversalResult(row: AlgoRow): TraversalResult | null {
  const userId = row['user_id'];
  const depth = row['depth'];
  const order = row['order'];

  if (userId === null || userId === undefined) {
    return null;
  }

  return {
    user_id: userId,
    depth: depth !== null && depth !== undefined ? safeInt(depth) : 0,
    order: order !== null && order !== undefined ? safeInt(order) : 0,
  };
}
