// Connected-components algorithms as pure functions.
//
// Each takes a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/algorithms/components.py.
import type { Connection } from '../connection.ts';
import { extractAlgoArray, safeInt } from './parsing.ts';

/** `{ nodeId, userId, component }` — a node's assigned component id. */
export interface ComponentResult {
  nodeId: string;
  userId: unknown;
  component: number;
}

function componentRows(conn: Connection, cypher: string): ComponentResult[] {
  const rows = extractAlgoArray(conn.cypher(cypher).toList());
  const components: ComponentResult[] = [];
  for (const row of rows) {
    const nodeId = row['node_id'];
    if (nodeId != null) {
      components.push({
        nodeId: String(nodeId),
        userId: row['user_id'],
        component: safeInt(row['component']),
      });
    }
  }
  return components;
}

/** Weakly connected components (graph treated as undirected). */
export function weaklyConnectedComponents(conn: Connection): ComponentResult[] {
  return componentRows(conn, 'RETURN wcc()');
}

/** Strongly connected components (direction-aware). */
export function stronglyConnectedComponents(conn: Connection): ComponentResult[] {
  return componentRows(conn, 'RETURN scc()');
}

/** Alias for {@link weaklyConnectedComponents} (matches Python's `wcc`). */
export const wcc = weaklyConnectedComponents;

/** Alias for {@link weaklyConnectedComponents} (matches Python's `connected_components`). */
export const connectedComponents = weaklyConnectedComponents;

/** Alias for {@link stronglyConnectedComponents} (matches Python's `scc`). */
export const scc = stronglyConnectedComponents;
