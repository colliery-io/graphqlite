// TypeScript parity runner for #30 [V-01] Python<->TS 대조 하네스.
//
// Loads scenarios.json, executes every scenario against a fresh in-memory graph
// built from the shared `fixture`, and emits one JSON document on stdout
// describing each step's outcome. Two modes:
//
//   results (default): record each step's return value (or the error it raised).
//   cypher           : record the sequence of Cypher query strings (+ params)
//                      the binding emits to the core while running the step,
//                      captured by wrapping Connection.cypher. Steps that throw
//                      before hitting the core (e.g. identifier validation)
//                      emit an empty/partial call list.
//
// This runner never touches the C core; it drives only the public TS binding.
// The extension is resolved by the binding itself (GRAPHQLITE_EXTENSION_PATH is
// honored — parity-check.sh sets it). Usage:
//
//   node run-ts.ts [--mode results|cypher] [--scenarios PATH]

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  graph,
  type Graph,
  insertNodesBulk,
  insertEdgesBulk,
  insertGraphBulk,
  resolveNodeIds,
  type BulkNodeItem,
  type BulkEdgeItem,
} from '../../src/index.ts';
import type { CypherResult } from '../../src/result.ts';

type Mode = 'results' | 'cypher';

interface StepSpec {
  id: string;
  method: string;
  args?: unknown[];
}

interface FixtureOp {
  method: string;
  args?: unknown[];
}

interface ScenarioSpec {
  id: string;
  group?: string;
  steps: StepSpec[];
}

interface Spec {
  fixture?: FixtureOp[];
  scenarios: ScenarioSpec[];
}

interface CypherCall {
  query: string;
  params: unknown;
}

interface StepResult {
  id: string;
  method: string;
  args: unknown[];
  status: 'ok' | 'error';
  value?: unknown;
  error?: { type: string; message: string };
  cypher?: CypherCall[];
}

function normWs(s: string): string {
  return s.replace(/\s+/g, ' ').trim();
}

function str(value: unknown): string {
  return value as string;
}

function num(value: unknown): number {
  return value as number;
}

function record(value: unknown): Record<string, unknown> {
  return value as Record<string, unknown>;
}

// Diagnostic used only by the bulk parity scenario: report which typed table
// (int/real/text/bool) a node property landed in. This is what surfaces the
// intentional 1.0→int divergence — the return VALUE of insertNodesBulk is
// identical across bindings, but the storage table differs. Raw SQL via the
// driver escape hatch (`g.connection.database`); it emits no cypher, so
// cypher-mode captures stay empty on both sides.
function propTableOf(g: Graph, externalId: string, key: string): string | null {
  const db = g.connection.database;
  const idKeyRow = db.prepare("SELECT id FROM property_keys WHERE key = 'id'").get() as
    | { id?: number }
    | undefined;
  if (!idKeyRow || idKeyRow.id === undefined) return null;
  const nodeRow = db
    .prepare('SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?')
    .get(idKeyRow.id, externalId) as { node_id?: number } | undefined;
  if (!nodeRow || nodeRow.node_id === undefined) return null;
  const keyRow = db.prepare('SELECT id FROM property_keys WHERE key = ?').get(key) as
    | { id?: number }
    | undefined;
  if (!keyRow || keyRow.id === undefined) return null;
  for (const suffix of ['int', 'real', 'text', 'bool']) {
    const r = db
      .prepare(`SELECT 1 AS x FROM node_props_${suffix} WHERE node_id = ? AND key_id = ?`)
      .get(nodeRow.node_id, keyRow.id);
    if (r !== undefined) return suffix;
  }
  return null;
}

// ── Dispatch: camelCase method -> TS binding call ──────────────────────────────
// Each adapter takes (graph, args) and performs the positional/options
// translation the TS API needs. Options-bearing methods (shortestPath,
// astar, bfs/dfs, nodeSimilarity, knn) build their options object from the
// positional scenario args.
const DISPATCH: Record<string, (g: Graph, a: unknown[]) => unknown> = {
  // nodes
  upsertNode: (g, a) => g.upsertNode(str(a[0]), record(a[1]), a[2] === undefined ? undefined : str(a[2])),
  getNode: (g, a) => g.getNode(str(a[0])),
  hasNode: (g, a) => g.hasNode(str(a[0])),
  deleteNode: (g, a) => g.deleteNode(str(a[0])),
  getAllNodes: (g, a) => g.getAllNodes(a[0] === undefined ? undefined : str(a[0])),
  // edges
  upsertEdge: (g, a) =>
    g.upsertEdge(
      str(a[0]),
      str(a[1]),
      record(a[2]),
      a[3] === undefined ? undefined : str(a[3]),
      a[4] === undefined ? undefined : str(a[4]),
    ),
  getEdge: (g, a) => g.getEdge(str(a[0]), str(a[1]), a[2] === undefined ? undefined : str(a[2])),
  hasEdge: (g, a) => g.hasEdge(str(a[0]), str(a[1]), a[2] === undefined ? undefined : str(a[2])),
  deleteEdge: (g, a) => g.deleteEdge(str(a[0]), str(a[1]), a[2] === undefined ? undefined : str(a[2])),
  getAllEdges: (g) => g.getAllEdges(),
  // queries
  nodeDegree: (g, a) => g.nodeDegree(str(a[0])),
  getNeighbors: (g, a) => g.getNeighbors(str(a[0])),
  getNodeEdges: (g, a) => g.getNodeEdges(str(a[0])),
  getEdgesFrom: (g, a) => g.getEdgesFrom(str(a[0])),
  getEdgesTo: (g, a) => g.getEdgesTo(str(a[0])),
  getEdgesByType: (g, a) => g.getEdgesByType(str(a[0]), str(a[1])),
  stats: (g) => g.stats(),
  query: (g, a) => g.query(str(a[0]), a[1] == null ? undefined : record(a[1])),
  // centrality
  pagerank: (g, a) =>
    g.pagerank(a[0] === undefined ? undefined : num(a[0]), a[1] === undefined ? undefined : num(a[1])),
  degreeCentrality: (g) => g.degreeCentrality(),
  betweennessCentrality: (g) => g.betweennessCentrality(),
  closenessCentrality: (g) => g.closenessCentrality(),
  eigenvectorCentrality: (g, a) => g.eigenvectorCentrality(a[0] === undefined ? undefined : num(a[0])),
  // community
  communityDetection: (g, a) => g.communityDetection(a[0] === undefined ? undefined : num(a[0])),
  louvain: (g, a) => g.louvain(a[0] === undefined ? undefined : num(a[0])),
  leidenCommunities: (g) => g.leidenCommunities(),
  // components
  weaklyConnectedComponents: (g) => g.weaklyConnectedComponents(),
  stronglyConnectedComponents: (g) => g.stronglyConnectedComponents(),
  // paths
  shortestPath: (g, a) =>
    g.shortestPath(str(a[0]), str(a[1]), a[2] === undefined ? undefined : { weightProp: str(a[2]) }),
  astar: (g, a) =>
    g.astar(
      str(a[0]),
      str(a[1]),
      a[2] === undefined && a[3] === undefined
        ? undefined
        : { latProp: str(a[2]), lonProp: str(a[3]) },
    ),
  allPairsShortestPath: (g) => g.allPairsShortestPath(),
  // traversal
  bfs: (g, a) => g.bfs(str(a[0]), a[1] === undefined ? undefined : { maxDepth: num(a[1]) }),
  dfs: (g, a) => g.dfs(str(a[0]), a[1] === undefined ? undefined : { maxDepth: num(a[1]) }),
  // similarity
  nodeSimilarity: (g, a) =>
    g.nodeSimilarity({
      node1: a[0] == null ? undefined : str(a[0]),
      node2: a[1] == null ? undefined : str(a[1]),
      threshold: a[2] == null ? undefined : num(a[2]),
      topK: a[3] == null ? undefined : num(a[3]),
    }),
  knn: (g, a) => g.knn(str(a[0]), a[1] === undefined ? undefined : { k: num(a[1]) }),
  triangleCount: (g) => g.triangleCount(),
  // bulk — raw SQL, bypasses Cypher (so cypher-mode captures are empty both sides).
  // Map returns are converted to plain objects so JSON matches Python's dict.
  insertNodesBulk: (g, a) => Object.fromEntries(insertNodesBulk(g.connection, a[0] as BulkNodeItem[])),
  insertEdgesBulk: (g, a) =>
    insertEdgesBulk(
      g.connection,
      a[0] as BulkEdgeItem[],
      a[1] == null
        ? undefined
        : new Map(Object.entries(record(a[1])).map(([k, v]) => [k, num(v)])),
    ),
  insertGraphBulk: (g, a) => {
    const r = insertGraphBulk(g.connection, a[0] as BulkNodeItem[], a[1] as BulkEdgeItem[]);
    return {
      nodesInserted: r.nodesInserted,
      edgesInserted: r.edgesInserted,
      idMap: Object.fromEntries(r.idMap),
    };
  },
  resolveNodeIds: (g, a) => Object.fromEntries(resolveNodeIds(g.connection, a[0] as string[])),
  bulkPropTable: (g, a) => propTableOf(g, str(a[0]), str(a[1])),
  // export — Python-only; the TS binding has no such method (allowlisted divergence).
  toRustworkx: () => {
    throw new Error('toRustworkx is not available in the TypeScript binding (Python-only rustworkx export).');
  },
};

function jsonReplacer(_key: string, value: unknown): unknown {
  return typeof value === 'bigint' ? Number(value) : value;
}

function installCypherSpy(g: Graph, sink: CypherCall[]): void {
  const conn = g.connection;
  const original: (query: string, params?: Record<string, unknown> | null) => CypherResult =
    conn.cypher.bind(conn);
  conn.cypher = (query: string, params?: Record<string, unknown> | null): CypherResult => {
    sink.push({ query: normWs(query), params: params ?? null });
    return original(query, params);
  };
}

function buildGraph(fixture: FixtureOp[], mode: Mode, sink: CypherCall[]): Graph {
  const g = graph(':memory:');
  if (mode === 'cypher') {
    installCypherSpy(g, sink);
  }
  for (const op of fixture) {
    DISPATCH[op.method]!(g, op.args ?? []);
  }
  sink.length = 0; // discard fixture-phase captures
  return g;
}

function runStep(g: Graph, step: StepSpec, mode: Mode, sink: CypherCall[]): StepResult {
  sink.length = 0;
  const args = step.args ?? [];
  const out: StepResult = { id: step.id, method: step.method, args, status: 'ok' };
  try {
    const adapter = DISPATCH[step.method];
    if (adapter === undefined) {
      throw new Error(`Unknown method in scenario: ${step.method}`);
    }
    const value = adapter(g, args);
    out.status = 'ok';
    if (mode === 'results') {
      out.value = value === undefined ? null : value;
    }
  } catch (err) {
    out.status = 'error';
    out.error = {
      type: err instanceof Error ? err.constructor.name : typeof err,
      message: err instanceof Error ? err.message : String(err),
    };
  }
  if (mode === 'cypher') {
    out.cypher = [...sink];
  }
  return out;
}

function parseArgs(argv: string[]): { mode: Mode; scenarios: string } {
  let mode: Mode = 'results';
  let scenarios = fileURLToPath(new URL('./scenarios.json', import.meta.url));
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--mode' && argv[i + 1] !== undefined) {
      const next = argv[i + 1];
      mode = next === 'cypher' ? 'cypher' : 'results';
      i++;
    } else if (argv[i] === '--scenarios' && argv[i + 1] !== undefined) {
      scenarios = argv[i + 1]!;
      i++;
    }
  }
  return { mode, scenarios };
}

function main(): void {
  const { mode, scenarios } = parseArgs(process.argv.slice(2));
  const spec = JSON.parse(readFileSync(scenarios, 'utf8')) as Spec;
  const fixture = spec.fixture ?? [];

  const out = {
    binding: 'typescript',
    mode,
    scenarios: [] as { id: string; group?: string; steps: StepResult[] }[],
  };

  for (const scenario of spec.scenarios) {
    const sink: CypherCall[] = [];
    const g = buildGraph(fixture, mode, sink);
    try {
      const steps = scenario.steps.map((s) => runStep(g, s, mode, sink));
      out.scenarios.push({ id: scenario.id, group: scenario.group, steps });
    } finally {
      g.close();
    }
  }

  process.stdout.write(JSON.stringify(out, jsonReplacer) + '\n');
}

main();
