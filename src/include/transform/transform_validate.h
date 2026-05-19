/* Compile-time argument-type validation pass.
 *
 * Catches openCypher's compile-time SyntaxError/TypeError class of errors
 * that the grammar accepts but the spec rejects — e.g. `RETURN NOT 1`,
 * `RETURN 'a' AND true`, etc.
 *
 * Run AFTER the AST is built and BEFORE transform/SQL generation. If the
 * query contains a static type violation, returns -1 and sets
 * `*error_message` to a freshly-allocated diagnostic the caller owns
 * (must free with `free`). Returns 0 if the AST passes all checks.
 */

#ifndef GRAPHQLITE_TRANSFORM_VALIDATE_H
#define GRAPHQLITE_TRANSFORM_VALIDATE_H

#include "parser/cypher_ast.h"

int transform_validate_query(cypher_query *query, char **error_message);

/* Validate a UNION tree: column-name agreement across branches, and no
 * mixing of UNION with UNION ALL. */
int transform_validate_union(cypher_union *u, char **error_message);

#endif
