// Typed error hierarchy for GraphQLite.
//
// The core reports failures as JSON of the shape
//   {"error": "...", "code": "PARSE_ERROR"}
// (see src/include/runtime/gql_error.h). The Python binding throws away the
// `code`; here we keep it, promoting each known code to a typed subclass while
// preserving the offending query. See design doc §7.2.

/** Canonical `code` strings emitted by the core (src/include/runtime/gql_error.h). */
export const CORE_ERROR_CODES = {
  VALIDATION: 'VALIDATION_ERROR',
  PARSE: 'PARSE_ERROR',
  EXECUTION: 'EXECUTION_ERROR',
  MEMORY: 'MEMORY_ERROR',
  INTERNAL: 'INTERNAL_ERROR',
  NOT_IMPLEMENTED: 'NOT_IMPLEMENTED',
} as const;

export interface GraphQLiteErrorOptions {
  /** The `code` from the core error payload, when present. */
  code?: string;
  /** The Cypher query that produced the error. */
  query?: string;
}

/** Base class for every error surfaced by the binding. */
export class GraphQLiteError extends Error {
  readonly code?: string;
  readonly query?: string;

  constructor(message: string, options: GraphQLiteErrorOptions = {}) {
    super(message);
    this.name = 'GraphQLiteError';
    this.code = options.code;
    this.query = options.query;
  }
}

export interface ParseErrorOptions extends GraphQLiteErrorOptions {
  line?: number;
  column?: number;
}

/** `PARSE_ERROR` — the `Line N, Col M` position is parsed out when present. */
export class ParseError extends GraphQLiteError {
  readonly line?: number;
  readonly column?: number;

  constructor(message: string, options: ParseErrorOptions = {}) {
    super(message, options);
    this.name = 'ParseError';
    this.line = options.line;
    this.column = options.column;
  }
}

/** `VALIDATION_ERROR` — identifier/validation failures. */
export class ValidationError extends GraphQLiteError {
  constructor(message: string, options: GraphQLiteErrorOptions = {}) {
    super(message, options);
    this.name = 'ValidationError';
  }
}

/** `EXECUTION_ERROR` — failure during the execution stage. */
export class ExecutionError extends GraphQLiteError {
  constructor(message: string, options: GraphQLiteErrorOptions = {}) {
    super(message, options);
    this.name = 'ExecutionError';
  }
}

/**
 * `UNSUPPORTED_OPERATION` — a feature the Python binding has but this one does
 * not implement (e.g. it relies on a Python-only dependency). Not produced by
 * the core; thrown by binding-side stubs (see `algorithms/community.ts` / #16).
 */
export class UnsupportedOperationError extends GraphQLiteError {
  constructor(message: string, options: GraphQLiteErrorOptions = {}) {
    super(message, { code: options.code ?? 'UNSUPPORTED_OPERATION', query: options.query });
    this.name = 'UnsupportedOperationError';
  }
}

export interface ExtensionLoadErrorOptions extends GraphQLiteErrorOptions {
  searchedPaths?: string[];
}

/**
 * Client-side failure to locate/load the extension binary. Not produced by the
 * core error JSON; thrown directly by path resolution. `platform.ts` (#2) holds
 * a minimal standalone copy that will be re-pointed here on integration (#7).
 */
export class ExtensionLoadError extends GraphQLiteError {
  readonly searchedPaths: string[];

  constructor(message: string, options: ExtensionLoadErrorOptions = {}) {
    super(message, { code: options.code ?? 'EXTENSION_LOAD_ERROR', query: options.query });
    this.name = 'ExtensionLoadError';
    this.searchedPaths = options.searchedPaths ?? [];
  }
}

export interface CoreErrorPayload {
  error: string;
  code?: string;
}

/**
 * Unwrap a core error string of the form `{"error":"...","code":"..."}`.
 * Returns `null` when the input is not that JSON shape (a scalar/legacy string).
 */
export function parseCoreError(raw: string): CoreErrorPayload | null {
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return null;
  }
  if (
    parsed !== null &&
    typeof parsed === 'object' &&
    'error' in parsed &&
    typeof (parsed as Record<string, unknown>)['error'] === 'string'
  ) {
    const record = parsed as Record<string, unknown>;
    const code = record['code'];
    return {
      error: record['error'] as string,
      code: typeof code === 'string' ? code : undefined,
    };
  }
  return null;
}

const LINE_COL = /Line\s+(\d+),\s*Col\s+(\d+)/;

/**
 * Turn a raw core error string into the most specific typed error.
 *
 * Known codes map to their subclass; every other code — including the known
 * `MEMORY_ERROR` / `INTERNAL_ERROR` / `NOT_IMPLEMENTED`, which have no dedicated
 * subclass — is absorbed into {@link GraphQLiteError} with the `code` preserved.
 * This function never throws: an unmappable payload becomes a base error.
 */
export function graphQLiteErrorFrom(raw: string, query?: string): GraphQLiteError {
  const payload = parseCoreError(raw);
  const message = payload?.error ?? raw;
  const code = payload?.code;

  switch (code) {
    case CORE_ERROR_CODES.PARSE: {
      const match = LINE_COL.exec(message);
      const line = match ? Number(match[1]) : undefined;
      const column = match ? Number(match[2]) : undefined;
      return new ParseError(message, { code, query, line, column });
    }
    case CORE_ERROR_CODES.VALIDATION:
      return new ValidationError(message, { code, query });
    case CORE_ERROR_CODES.EXECUTION:
      return new ExecutionError(message, { code, query });
    default:
      return new GraphQLiteError(message, { code, query });
  }
}
