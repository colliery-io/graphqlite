// Path-finding algorithms as pure functions.
//
// This is the **most inconsistent** algorithm module and the asymmetries are
// reproduced verbatim from bindings/python/src/graphqlite/algorithms/paths.py:
//   1. `shortestPath` quotes with " ; `astar` quotes with ' .
//   2. Both `shortestPath` and `astar` unwrap the `column_0` wrapper that this
//      core wraps results in, then fall back to direct `result[0]` field access
//      when it is absent. (Historically `astar` skipped the unwrap and always
//      returned defaults; fixed in #64.)
//   3. `astar`'s lat/lon props are interpolated unescaped in Python; here they
//      are validated with `assertIdentifier` instead.
//   4. Only `apsp` goes through `.toList()` → `extractAlgoArray` (which unwraps
//      the `column_0` array, so apsp works).
import type { Connection } from '../connection.ts';
import { escapeString, assertIdentifier } from '../utils.ts';
import { extractAlgoArray, safeFloat, safeInt } from './parsing.ts';

/** `{ path, distance, found }` — a single shortest path. */
export interface ShortestPathResult {
  path: unknown[];
  distance: number | null;
  found: boolean;
}

/** `shortestPath` result plus A*'s `nodesExplored` counter. */
export interface AStarResult extends ShortestPathResult {
  nodesExplored: number;
}

/** `{ source, target, distance }` — one reachable pair from all-pairs shortest path. */
export interface AllPairsPath {
  source: unknown;
  target: unknown;
  distance: number;
}

export interface ShortestPathOptions {
  /** Edge property used as weight; omitted → unweighted (hop count). */
  weightProp?: string;
}

export interface AStarOptions {
  /** Latitude/y coordinate property (interpolated → validated as an identifier). */
  latProp?: string;
  /** Longitude/x coordinate property (interpolated → validated as an identifier). */
  lonProp?: string;
}

/**
 * Dijkstra shortest path. **Uses double quotes** and **unwraps `column_0`** —
 * both unique to this method (see module header). Empty result → the documented
 * `{ path: [], distance: null, found: false }` default.
 */
export function shortestPath(
  conn: Connection,
  sourceId: string,
  targetId: string,
  options: ShortestPathOptions = {},
): ShortestPathResult {
  const escSource = escapeString(sourceId);
  const escTarget = escapeString(targetId);
  const query = options.weightProp
    ? `RETURN dijkstra("${escSource}", "${escTarget}", "${escapeString(options.weightProp)}")`
    : `RETURN dijkstra("${escSource}", "${escTarget}")`;

  const result = conn.cypher(query);
  if (result.length === 0) {
    return { path: [], distance: null, found: false };
  }

  const row = result[0]!;
  const col0 = row['column_0'];
  if (col0 && typeof col0 === 'object' && !Array.isArray(col0)) {
    const data = col0 as Record<string, unknown>;
    return {
      path: (data['path'] as unknown[]) ?? [],
      distance: (data['distance'] as number | null) ?? null,
      found: (data['found'] as boolean) ?? false,
    };
  }
  // Already unpacked (defensive — Python's direct-access branch).
  return {
    path: (row['path'] as unknown[]) ?? [],
    distance: (row['distance'] as number | null) ?? null,
    found: (row['found'] as boolean) ?? false,
  };
}

/** Alias for {@link shortestPath} (matches Python's `dijkstra`). */
export const dijkstra = shortestPath;

/**
 * A* shortest path. **Uses single quotes** and **unwraps `column_0`** (like
 * {@link shortestPath}), reading `path`/`distance`/`found`/`nodes_explored` off
 * the unwrapped object, with a defensive fallback to direct `result[0]` access.
 * `latProp`/`lonProp` are interpolated, so they are validated with
 * {@link assertIdentifier}. Empty result → the documented default (with
 * `nodesExplored: 0`).
 */
export function astar(
  conn: Connection,
  sourceId: string,
  targetId: string,
  options: AStarOptions = {},
): AStarResult {
  const escSource = escapeString(sourceId);
  const escTarget = escapeString(targetId);

  let query: string;
  if (options.latProp && options.lonProp) {
    assertIdentifier(options.latProp, 'property');
    assertIdentifier(options.lonProp, 'property');
    query = `RETURN astar('${escSource}', '${escTarget}', '${options.latProp}', '${options.lonProp}')`;
  } else {
    query = `RETURN astar('${escSource}', '${escTarget}')`;
  }

  const result = conn.cypher(query);
  if (result.length === 0) {
    return { path: [], distance: null, found: false, nodesExplored: 0 };
  }

  const row = result[0]!;
  const col0 = row['column_0'];
  if (col0 && typeof col0 === 'object' && !Array.isArray(col0)) {
    const data = col0 as Record<string, unknown>;
    return {
      path: (data['path'] as unknown[]) ?? [],
      distance: (data['distance'] as number | null) ?? null,
      found: (data['found'] as boolean) ?? false,
      nodesExplored: safeInt(data['nodes_explored']),
    };
  }
  // Already unpacked (defensive — Python's direct-access branch).
  return {
    path: (row['path'] as unknown[]) ?? [],
    distance: (row['distance'] as number | null) ?? null,
    found: (row['found'] as boolean) ?? false,
    nodesExplored: safeInt(row['nodes_explored']),
  };
}

/** Alias for {@link astar} (matches Python's `a_star`). */
export const aStar = astar;

/**
 * All-pairs shortest path (Floyd–Warshall). The **only** path method that runs
 * through `.toList()` → {@link extractAlgoArray} (which unwraps `column_0`).
 * Keeps only rows with non-null `source` and `target`.
 */
export function allPairsShortestPath(conn: Connection): AllPairsPath[] {
  const rows = extractAlgoArray(conn.cypher('RETURN apsp()').toList());
  const paths: AllPairsPath[] = [];
  for (const row of rows) {
    const source = row['source'];
    const target = row['target'];
    if (source != null && target != null) {
      paths.push({ source, target, distance: safeFloat(row['distance']) });
    }
  }
  return paths;
}

/** Alias for {@link allPairsShortestPath} (matches Python's `apsp`). */
export const apsp = allPairsShortestPath;
