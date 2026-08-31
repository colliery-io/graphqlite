import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, rmSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { graph } from '../src/graph/index.ts';
import { getExtensionName, getPlatformKey } from '../src/platform.ts';
import { UnsupportedOperationError } from '../src/errors.ts';
import {
  pagerankAsync,
  louvainAsync,
  betweennessCentralityAsync,
  apspAsync,
  nodeSimilarityAsync,
  knnAsync,
} from '../src/async/index.ts';
import type { CentralityScore } from '../src/algorithms/centrality.ts';
import type { CommunityResult } from '../src/algorithms/community.ts';
import type { AllPairsPath } from '../src/algorithms/paths.ts';
import type { NodeSimilarityPair, KnnNeighbor } from '../src/algorithms/similarity.ts';

// --- :memory: guard (no extension needed — throws before spawning a worker) ---
// This is the acceptance criterion for the async wrappers: an in-memory database
// is private to the connection that created it, so the worker (which opens its
// own connection) can never see it. All six variants must reject it up front.
test(':memory: is rejected with UnsupportedOperationError before any worker spawns', async () => {
  await assert.rejects(async () => pagerankAsync(':memory:'), UnsupportedOperationError);
  await assert.rejects(async () => louvainAsync(':memory:'), UnsupportedOperationError);
  await assert.rejects(
    async () => betweennessCentralityAsync(':memory:'),
    UnsupportedOperationError,
  );
  await assert.rejects(async () => apspAsync(':memory:'), UnsupportedOperationError);
  await assert.rejects(async () => nodeSimilarityAsync(':memory:'), UnsupportedOperationError);
  await assert.rejects(async () => knnAsync(':memory:', 'a'), UnsupportedOperationError);
});

// --- Integration: async result === sync result (gated on the staged build) ---
// The staged extension lives under npm/<platform>-<arch>/. When it is present we
// build a file-backed graph, capture every sync result, then assert each async
// variant returns a deep-equal value (proving the worker offload is faithful).
function stagedExtensionPath(): string | null {
  try {
    const candidate = fileURLToPath(
      new URL(`../npm/${getPlatformKey()}/${getExtensionName()}`, import.meta.url),
    );
    return existsSync(candidate) ? candidate : null;
  } catch {
    return null;
  }
}

const extensionPath = stagedExtensionPath();
const gate = { skip: extensionPath === null };

let dbPath = '';
const sync: {
  pagerank: CentralityScore[];
  louvain: CommunityResult[];
  betweenness: CentralityScore[];
  apsp: AllPairsPath[];
  nodeSimilarity: NodeSimilarityPair[];
  knn: KnnNeighbor[];
} = {
  pagerank: [],
  louvain: [],
  betweenness: [],
  apsp: [],
  nodeSimilarity: [],
  knn: [],
};

before(() => {
  if (!extensionPath) {
    return;
  }
  dbPath = join(tmpdir(), `graphqlite-async-${process.pid}-${Date.now()}.db`);
  const g = graph(dbPath, { extensionPath });
  try {
    for (const id of ['a', 'b', 'c', 'd']) {
      g.upsertNode(id, {}, 'Person');
    }
    g.upsertEdge('a', 'b', {}, 'KNOWS');
    g.upsertEdge('b', 'c', {}, 'KNOWS');
    g.upsertEdge('c', 'a', {}, 'KNOWS');
    g.upsertEdge('c', 'd', {}, 'KNOWS');

    sync.pagerank = g.pagerank();
    sync.louvain = g.louvain();
    sync.betweenness = g.betweennessCentrality();
    sync.apsp = g.apsp();
    sync.nodeSimilarity = g.nodeSimilarity();
    sync.knn = g.knn('a');
  } finally {
    g.close();
  }
});

after(() => {
  if (dbPath && existsSync(dbPath)) {
    rmSync(dbPath, { force: true });
  }
});

test('pagerankAsync matches the sync pagerank result', gate, async () => {
  assert.ok(sync.pagerank.length > 0, 'sync pagerank should produce data to compare against');
  assert.deepEqual(await pagerankAsync(dbPath, { extensionPath: extensionPath! }), sync.pagerank);
});

test('louvainAsync matches the sync louvain result', gate, async () => {
  assert.deepEqual(await louvainAsync(dbPath, { extensionPath: extensionPath! }), sync.louvain);
});

test('betweennessCentralityAsync matches the sync result', gate, async () => {
  assert.deepEqual(
    await betweennessCentralityAsync(dbPath, { extensionPath: extensionPath! }),
    sync.betweenness,
  );
});

test('apspAsync matches the sync apsp result', gate, async () => {
  assert.deepEqual(await apspAsync(dbPath, { extensionPath: extensionPath! }), sync.apsp);
});

test('nodeSimilarityAsync matches the sync nodeSimilarity result', gate, async () => {
  assert.deepEqual(
    await nodeSimilarityAsync(dbPath, { extensionPath: extensionPath! }),
    sync.nodeSimilarity,
  );
});

test('knnAsync matches the sync knn result', gate, async () => {
  assert.deepEqual(await knnAsync(dbPath, 'a', { extensionPath: extensionPath! }), sync.knn);
});
