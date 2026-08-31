// Community detection algorithms as pure functions.
//
// Like the other algorithm modules these use **no parameter binding** — the
// numeric argument is interpolated into the Cypher and is therefore validated
// to be finite first. Mirrors
// bindings/python/src/graphqlite/algorithms/community.py. `leidenCommunities`
// is intentionally NOT ported (Python-only graspologic dependency); the Graph
// facade exposes it as an UnsupportedOperationError stub — see graph/index.ts.
import type { Connection } from '../connection.ts';
import { extractAlgoArray, safeInt } from './parsing.ts';
import { ValidationError } from '../errors.ts';

/** `{ nodeId, userId, community }` — a node's assigned community. */
export interface CommunityResult {
  nodeId: string;
  userId: unknown;
  community: number;
}

/** Guard an interpolated numeric argument — it goes into the query unescaped. */
function assertFinite(value: number, name: string): void {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new ValidationError(`${name} must be a finite number, got ${JSON.stringify(value)}`, {
      code: 'VALIDATION_ERROR',
    });
  }
}

/**
 * Label-propagation community detection. **Filters on both `nodeId !== null` and
 * `community !== null`** — unlike {@link louvain}, which checks `nodeId` only.
 * This asymmetry is reproduced verbatim from community.py:community_detection.
 */
export function communityDetection(conn: Connection, iterations = 10): CommunityResult[] {
  assertFinite(iterations, 'iterations');
  const rows = extractAlgoArray(conn.cypher(`RETURN labelPropagation(${iterations})`).toList());
  const communities: CommunityResult[] = [];
  for (const row of rows) {
    const nodeId = row['node_id'];
    const community = row['community'];
    if (nodeId != null && community != null) {
      communities.push({
        nodeId: String(nodeId),
        userId: row['user_id'],
        community: safeInt(community),
      });
    }
  }
  return communities;
}

/** Louvain community detection. Filters on `nodeId !== null` only. */
export function louvain(conn: Connection, resolution = 1.0): CommunityResult[] {
  assertFinite(resolution, 'resolution');
  const rows = extractAlgoArray(conn.cypher(`RETURN louvain(${resolution})`).toList());
  const communities: CommunityResult[] = [];
  for (const row of rows) {
    const nodeId = row['node_id'];
    if (nodeId != null) {
      communities.push({
        nodeId: String(nodeId),
        userId: row['user_id'],
        community: safeInt(row['community']),
      });
    }
  }
  return communities;
}
