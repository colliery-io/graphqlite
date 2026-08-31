// Result normalization for the non-uniform `cypher()` return values.
//
// The core returns four shapes (see design doc §2.1 / §7.1):
//   ① row set    [{"name":"Alice","age":30}, ...]
//   ② node object[{"n": {"id":1,"labels":["Person"],"properties":{...}}}]
//   ③ algorithm  [{"column_0":[{"node_id":2,...}]}]
//   ④ DDL summary {"nodes_created":1,"relationships_created":0}  (JSON object, #72;
//                 pre-#72 plain "Query executed successfully - ..." still parsed as fallback)
//
// This mirrors the branch structure of Python's connection.py:162-192. The one
// intentional divergence: Python raises on error-looking non-JSON strings here;
// in this binding error detection/throwing belongs to errors.ts (#3) + the
// connection layer (#7), so `normalizeCypherResult` stays a pure shape
// normalizer and never throws.

export type CypherValue =
  | null
  | boolean
  | number
  | string
  | CypherValue[]
  | { [key: string]: CypherValue };

export type CypherRow = Record<string, CypherValue>;

/**
 * A Cypher query result. Usable both as an array (`length`, `result[0]`,
 * `for..of`) and via `toList()` — the algorithm modules mix both styles.
 */
export class CypherResult {
  [index: number]: CypherRow;

  readonly #rows: CypherRow[];
  readonly #columns: string[];

  constructor(rows: CypherRow[], columns: string[]) {
    this.#rows = rows;
    this.#columns = columns;
    for (let i = 0; i < rows.length; i++) {
      this[i] = rows[i]!;
    }
  }

  get length(): number {
    return this.#rows.length;
  }

  /** Column names, in first-row key order. */
  get columns(): string[] {
    return this.#columns;
  }

  /** The rows as a plain array. */
  toList(): CypherRow[] {
    return this.#rows;
  }

  [Symbol.iterator](): IterableIterator<CypherRow> {
    return this.#rows[Symbol.iterator]();
  }
}

/** Structured form of the ④ DDL summary line. */
export interface MutationSummary {
  nodesCreated: number;
  relationshipsCreated: number;
  raw: string;
}

function isPlainObject(value: unknown): value is CypherRow {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

/**
 * Normalize a raw `cypher()` cell into a {@link CypherResult}.
 *
 * `null` (or a null row) yields an empty result. Non-JSON text and JSON arrays
 * of scalars are wrapped as a single `{ result: <raw> }` row — this raw-string
 * fallback is load-bearing (e.g. `getAllNodes`); do not "tidy" it into parsed
 * values. Never throws.
 */
export function normalizeCypherResult(raw: string | null): CypherResult {
  if (raw === null) {
    return new CypherResult([], []);
  }

  let data: unknown;
  try {
    data = JSON.parse(raw);
  } catch {
    // Non-JSON: scalar or ④ DDL summary. Keep Python's {result: raw} shape.
    return new CypherResult([{ result: raw }], ['result']);
  }

  if (Array.isArray(data)) {
    if (data.length === 0) {
      return new CypherResult([], []);
    }
    const first = data[0];
    if (isPlainObject(first)) {
      return new CypherResult(data as CypherRow[], Object.keys(first));
    }
    // List of scalars (e.g. range(), tail(), some algorithms) — wrap the raw
    // JSON string. Callers depend on this exact fallback.
    return new CypherResult([{ result: raw }], ['result']);
  }

  if (isPlainObject(data)) {
    return new CypherResult([data], Object.keys(data));
  }

  // Bare scalar.
  return new CypherResult([{ result: data as CypherValue }], ['result']);
}

const NODES_CREATED = /nodes created:\s*(\d+)/i;
const RELATIONSHIPS_CREATED = /relationships created:\s*(\d+)/i;

/**
 * Parse the ④ DDL summary into counts. As of #72 the core returns a JSON object
 * `{"nodes_created":N,"relationships_created":M}`, which is read directly. For
 * backward compatibility the pre-#72 human string ("... nodes created: N,
 * relationships created: M") is still parsed by regex as a fallback. Never
 * raises: on no match the count is `0` and the original text is preserved in `raw`.
 */
export function parseMutationSummary(raw: string): MutationSummary {
  // Preferred: structured JSON object from the core (#72).
  try {
    const parsed: unknown = JSON.parse(raw);
    if (
      isPlainObject(parsed) &&
      ('nodes_created' in parsed || 'relationships_created' in parsed)
    ) {
      return {
        nodesCreated: Number(parsed['nodes_created'] ?? 0),
        relationshipsCreated: Number(parsed['relationships_created'] ?? 0),
        raw,
      };
    }
  } catch {
    // Not JSON — fall through to the legacy plain-text parse.
  }

  // Legacy fallback: pre-#72 human-readable string.
  const nodes = NODES_CREATED.exec(raw);
  const relationships = RELATIONSHIPS_CREATED.exec(raw);
  return {
    nodesCreated: nodes ? Number(nodes[1]) : 0,
    relationshipsCreated: relationships ? Number(relationships[1]) : 0,
    raw,
  };
}
