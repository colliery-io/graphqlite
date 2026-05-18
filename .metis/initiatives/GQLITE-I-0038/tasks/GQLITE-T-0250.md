---
id: e16-named-pattern-comprehension-p
level: task
title: "E16: Named pattern comprehension [p = (a)-->(b) | p] (Pattern2)"
short_code: "GQLITE-T-0250"
created_at: 2026-05-18T19:00:00+00:00
updated_at: 2026-05-18T19:54:49.826710+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E16: Named pattern comprehension [p = (a)-->(b) | p] (Pattern2)

Parent initiative: [[GQLITE-I-0038]] · Cluster **Pattern2** · Current count: **~10 scenarios**

## Objective

Pattern comprehensions accept a path-variable name on the LHS:
`[p = (n)-->() | p]` — bind every match of the pattern to `p`, then
collect the projected expression. Our grammar accepts the unnamed
form `[(n)-->() | p]` but parses the named form as a syntax error:

```
SELECT cypher('MATCH (n) RETURN [p = (n)-->() | p] AS list');
-- Line 2, Col 22: syntax error, unexpected '|', expecting ...
```

Pattern2 [1]–[6] all use named pattern comprehensions; this single
grammar gap blocks them.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

CREATE (a:A), (b:B), (c:C);
CREATE (a)-[:T]->(b), (b)-[:T]->(c);

-- Pattern2 [1]: named path inside comprehension
SELECT cypher('MATCH (n) RETURN [p = (n)-->() | p] AS list');
-- expect: 3 rows, one per matched `n`, with `list` being a list of
--         the matched paths from n.

-- Pattern2 [6]: aggregate over named pattern comprehension
SELECT cypher('MATCH (a:A) RETURN size([p = (a)-->() | p]) AS hops');
-- expect: 1 row, hops=1
EOF
```

## Target files

- `src/backend/parser/cypher_gram.y::pattern_comprehension` — add
  rules that accept a leading `IDENTIFIER '='` before the pattern:
  ```
  '[' IDENTIFIER '=' path '|' expr ']'
  '[' IDENTIFIER '=' path WHERE expr '|' expr ']'
  ```
  Path is the existing path rule used in MATCH. Inside the
  comprehension scope, register the IDENTIFIER as `VAR_KIND_PATH`.
- `src/include/parser/cypher_ast.h::cypher_pattern_comprehension` —
  add a `char *path_var` field; populate from the new grammar rule.
- `src/backend/transform/transform_return.c` (the section that
  handles pattern comprehensions) — if `path_var` is set, register
  it in `var_ctx` so the `| expr` body can reference `p` as a path
  value and project the path JSON correctly.

## Expected delta

`+8` to `+12`.

Scenarios expected to flip:
- `expressions/pattern/Pattern2.feature` [1], [2], [3], [5], [6]
  (named-path examples)
- Possibly a few more wherever named pattern comprehension is used.

## Verification

```sh
angreal build extension
angreal test tck --filter Pattern2 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per reproducer.

# Positive: unnamed pattern comprehension still parses
sqlite3 :memory: <<'EOF'
.load build/graphqlite
CREATE (a:A)-[:T]->(b:B);
SELECT cypher('MATCH (n:A) RETURN [(n)-->(m) | m] AS others');
EOF

angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] `[p = pattern | expr]` parses and `p` is bound as a path
      variable in the body.
- [ ] WHERE-form `[p = pattern WHERE pred | expr]` also parses.
- [ ] Existing unnamed pattern comprehensions still work.
- [ ] Pattern2 [1]/[2]/[3] return the expected row count.

## Risks

- Adding `IDENTIFIER '='` to the comprehension head can collide
  with list-comprehension's `IDENTIFIER 'IN'` head. The GLR parser
  should handle the ambiguity; verify shift/reduce counts after.
- The named path needs to be wrapped as a path JSON for projection.
  Reuse the existing path-formatting code rather than building a
  new shape.

## Status updates

### 2026-05-18 — blocked / investigation only

**Outcome:** TCK unchanged (3231). No code changes. The grammar
extension is small but the transform-side scope work isn't.

**What's needed beyond the grammar rule:**

1. **AST field.** `cypher_pattern_comprehension` has no `path_var`
   field. Adding one is straightforward but touches the AST header,
   the make_* constructor, and every consumer of the struct.

2. **Path-variable scope binding.** The new grammar rule must
   register `p` in `var_ctx` as `VAR_KIND_PATH` for the body of the
   comprehension only, then unregister on exit. The existing
   list-comprehension scope helper handles this for scalar vars but
   not paths.

3. **Path-projection translation.** The body `| p` must compile to
   a JSON object `{nodes: [...], rels: [...]}` over the matched
   path. Today we have node and edge JSON shapes but no first-class
   path serializer inside a `json_group_array` aggregation.

**Recommendation:** Split into 3 sub-tasks (grammar + AST field;
scope binding; path-as-JSON projection). Each is well-bounded but
together they exceed the task budget.

**Files touched:** none.

**Acceptance criteria:** none met; deferred to follow-up tasks.