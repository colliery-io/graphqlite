// `graphqlite/async` subpath entry point.
//
// This element (#23 [G-02]) *defines the subpath* and makes it a real,
// type-checked module so `import ... from 'graphqlite/async'` resolves today.
// The worker_threads-backed offloading it will eventually front lands with
// #29 [I-06]; until then this re-exports the same public surface as the main
// entry so callers can migrate their import path now and gain true async
// behavior later without a second edit.
//
// #29 [I-06] has now landed: the sync surface is still re-exported here (so the
// migrated import path keeps working), and the six worker_threads-backed async
// variants (`pagerankAsync`, `louvainAsync`, `betweennessCentralityAsync`,
// `apspAsync`, `nodeSimilarityAsync`, `knnAsync`) are exported alongside it from
// `./async/index.ts`. The subpath (`./async`) is unchanged.
export * from './index.ts';
export * from './async/index.ts';
