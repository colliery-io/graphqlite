import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  GraphQLiteError,
  ParseError,
  ValidationError,
  ExecutionError,
  ExtensionLoadError,
  CORE_ERROR_CODES,
  parseCoreError,
  graphQLiteErrorFrom,
} from '../src/errors.ts';

const QUERY = 'MATCH (n RETURN n';

test('maps the measured PARSE_ERROR payload to ParseError with line/column', () => {
  const raw =
    '{"error":"Line 1, Col 10: syntax error, unexpected RETURN, expecting \')\'","code":"PARSE_ERROR"}';
  const err = graphQLiteErrorFrom(raw, QUERY);
  assert.ok(err instanceof ParseError);
  assert.equal(err.line, 1);
  assert.equal(err.column, 10);
  assert.equal(err.code, 'PARSE_ERROR');
  assert.equal(err.query, QUERY);
  assert.match(err.message, /syntax error/);
});

test('maps VALIDATION_ERROR to ValidationError', () => {
  const err = graphQLiteErrorFrom('{"error":"bad identifier","code":"VALIDATION_ERROR"}', QUERY);
  assert.ok(err instanceof ValidationError);
  assert.equal(err.code, CORE_ERROR_CODES.VALIDATION);
  assert.equal(err.query, QUERY);
});

test('maps EXECUTION_ERROR to ExecutionError', () => {
  const err = graphQLiteErrorFrom('{"error":"boom","code":"EXECUTION_ERROR"}', QUERY);
  assert.ok(err instanceof ExecutionError);
  assert.equal(err.code, CORE_ERROR_CODES.EXECUTION);
});

test('known codes without a subclass fall back to base GraphQLiteError, code kept', () => {
  for (const code of ['MEMORY_ERROR', 'INTERNAL_ERROR', 'NOT_IMPLEMENTED']) {
    const err = graphQLiteErrorFrom(`{"error":"x","code":"${code}"}`, QUERY);
    assert.equal(err.constructor, GraphQLiteError, `${code} should be base class`);
    assert.equal(err.code, code);
    assert.equal(err.query, QUERY);
  }
});

test('an unknown code is absorbed into the base class (never throws)', () => {
  const err = graphQLiteErrorFrom('{"error":"weird","code":"WAT_ERROR"}', QUERY);
  assert.equal(err.constructor, GraphQLiteError);
  assert.equal(err.code, 'WAT_ERROR');
});

test('a non-JSON raw string becomes a base error with the raw message and no code', () => {
  const err = graphQLiteErrorFrom('Error: legacy scalar message', QUERY);
  assert.equal(err.constructor, GraphQLiteError);
  assert.equal(err.message, 'Error: legacy scalar message');
  assert.equal(err.code, undefined);
  assert.equal(err.query, QUERY);
});

test('parseCoreError returns null for non-payload strings', () => {
  assert.equal(parseCoreError('not json'), null);
  assert.equal(parseCoreError('[1,2,3]'), null);
  assert.equal(parseCoreError('{"result":"ok"}'), null);
});

test('parseCoreError extracts error and optional code', () => {
  assert.deepEqual(parseCoreError('{"error":"e","code":"PARSE_ERROR"}'), {
    error: 'e',
    code: 'PARSE_ERROR',
  });
  assert.deepEqual(parseCoreError('{"error":"e"}'), { error: 'e', code: undefined });
});

test('instanceof chain: ParseError → GraphQLiteError → Error', () => {
  const err = new ParseError('x', { line: 2, column: 3 });
  assert.ok(err instanceof ParseError);
  assert.ok(err instanceof GraphQLiteError);
  assert.ok(err instanceof Error);
  assert.equal(err.name, 'ParseError');
});

test('ExtensionLoadError carries searchedPaths and a default code', () => {
  const err = new ExtensionLoadError('not found', { searchedPaths: ['/a', '/b'] });
  assert.ok(err instanceof GraphQLiteError);
  assert.deepEqual(err.searchedPaths, ['/a', '/b']);
  assert.equal(err.code, 'EXTENSION_LOAD_ERROR');
});
