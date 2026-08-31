// String escaping / serialization helpers and identifier validation.
//
// Compatibility policy: for valid input, produce byte-for-byte the same Cypher
// as the Python binding (bindings/python/src/graphqlite/utils.py) — including
// its quirks. The one deliberate divergence is injection defense:
// `assertIdentifier` is new and rejects identifiers Python would silently pass.
import { ValidationError } from './errors.ts';

// Cypher reserved keywords that can't be used as relationship types.
// Copied verbatim from Python's CYPHER_RESERVED (63 entries — the issue body's
// "72" is inaccurate; compatibility requires matching Python's actual set).
export const CYPHER_RESERVED: ReadonlySet<string> = new Set([
  // Clauses
  'CREATE', 'MATCH', 'RETURN', 'WHERE', 'DELETE', 'SET', 'REMOVE',
  'ORDER', 'BY', 'SKIP', 'LIMIT', 'WITH', 'UNWIND', 'AS', 'AND', 'OR',
  'NOT', 'IN', 'IS', 'NULL', 'TRUE', 'FALSE', 'MERGE', 'ON', 'CALL',
  'YIELD', 'DETACH', 'OPTIONAL', 'UNION', 'ALL', 'CASE', 'WHEN', 'THEN',
  'ELSE', 'END', 'EXISTS', 'FOREACH',
  // Aggregate functions
  'COUNT', 'SUM', 'AVG', 'MIN', 'MAX', 'COLLECT',
  // List functions and expressions
  'REDUCE', 'FILTER', 'EXTRACT', 'ANY', 'NONE', 'SINGLE',
  // Other reserved words
  'STARTS', 'ENDS', 'CONTAINS', 'XOR', 'DISTINCT', 'LOAD', 'CSV',
  'USING', 'PERIODIC', 'COMMIT', 'CONSTRAINT', 'INDEX', 'DROP', 'ASSERT',
]);

/**
 * Escape a string for use in Cypher queries. Sequential replacement, order
 * matters (backslash first). Mirrors Python `escape_string`.
 */
export function escapeString(s: string): string {
  return s
    .replaceAll('\\', '\\\\')
    .replaceAll("'", "\\'")
    .replaceAll('"', '\\"')
    .replaceAll('\n', ' ')
    .replaceAll('\r', ' ')
    .replaceAll('\t', ' ');
}

// Python's str.isalnum() is Unicode-aware, so a CJK relationship type like
// '관계' survives sanitization. An ASCII-only /[a-zA-Z0-9_]/ would strip it —
// use the Unicode property classes instead.
const IDENTIFIER_CHAR = /[\p{L}\p{N}_]/u;
const DECIMAL_DIGIT = /\p{Nd}/u;

/**
 * Sanitize a relationship type into a safe Cypher identifier that is not a
 * reserved word. Mirrors Python `sanitize_rel_type` (Unicode-aware).
 */
export function sanitizeRelType(relType: string): string {
  const chars = Array.from(relType, (c) => (IDENTIFIER_CHAR.test(c) ? c : '_'));
  let safe = chars.join('');
  const first = chars[0];
  if (safe.length === 0 || (first !== undefined && DECIMAL_DIGIT.test(first))) {
    safe = 'REL_' + safe;
  }
  if (CYPHER_RESERVED.has(safe.toUpperCase())) {
    safe = 'REL_' + safe;
  }
  return safe;
}

export type PropRecord = Record<string, unknown>;

/**
 * Format a properties object as a Cypher property string
 * (`key1: 'value1', key2: 123`). Mirrors Python `format_props`: the branch
 * order is significant (boolean before the raw fallback), and only strings are
 * escaped — every other type is interpolated raw, matching Python.
 */
export function formatProps(
  props: PropRecord,
  escapeFn: (s: string) => string = escapeString,
): string {
  const parts: string[] = [];
  for (const [key, value] of Object.entries(props)) {
    if (typeof value === 'string') {
      parts.push(`${key}: '${escapeFn(value)}'`);
    } else if (typeof value === 'boolean') {
      parts.push(`${key}: ${value ? 'true' : 'false'}`);
    } else if (value === null) {
      parts.push(`${key}: null`);
    } else {
      // Numbers and anything else: interpolate raw, no escaping (Python parity).
      parts.push(`${key}: ${String(value)}`);
    }
  }
  return parts.join(', ');
}

export type IdentifierKind = 'label' | 'property' | 'graph';

// A valid unquoted identifier: starts with a letter or underscore, followed by
// letters, numbers, or underscores. Unicode-aware so CJK labels pass. This is
// the injection boundary — anything else (quotes, backticks, spaces,
// semicolons, empty) is rejected.
const VALID_IDENTIFIER = /^[\p{L}_][\p{L}\p{N}_]*$/u;

/**
 * Assert that `value` is a safe identifier for the given `kind`. New in the TS
 * binding (Python performs no such check). Throws {@link ValidationError} on
 * anything that is not a valid identifier; accepts every legitimate identifier
 * Python passes, including Unicode/CJK.
 */
export function assertIdentifier(value: string, kind: IdentifierKind): void {
  if (typeof value !== 'string' || !VALID_IDENTIFIER.test(value)) {
    throw new ValidationError(`Invalid ${kind} identifier: ${JSON.stringify(value)}`, {
      code: 'VALIDATION_ERROR',
    });
  }
}
