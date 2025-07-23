#include "gql_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Parser Lifecycle
// ============================================================================

gql_parser_t* gql_parser_create(const char *input) {
    if (!input) return NULL;
    
    gql_parser_t *parser = malloc(sizeof(gql_parser_t));
    if (!parser) return NULL;
    
    parser->lexer = gql_lexer_create(input);
    if (!parser->lexer) {
        free(parser);
        return NULL;
    }
    
    // Initialize parser state
    parser->error_message = NULL;
    parser->error_line = 0;
    parser->error_column = 0;
    parser->has_error = false;
    parser->at_end = false;
    
    // Load first two tokens
    parser->current_token = gql_lexer_next_token(parser->lexer);
    parser->peek_token = gql_lexer_next_token(parser->lexer);
    
    return parser;
}

void gql_parser_destroy(gql_parser_t *parser) {
    if (!parser) return;
    
    if (parser->lexer) {
        gql_lexer_destroy(parser->lexer);
    }
    
    if (parser->error_message) {
        free(parser->error_message);
    }
    
    gql_token_free(&parser->current_token);
    gql_token_free(&parser->peek_token);
    
    free(parser);
}

// ============================================================================
// Utility Functions
// ============================================================================

void advance_token(gql_parser_t *parser) {
    if (parser->at_end) return;
    
    gql_token_free(&parser->current_token);
    parser->current_token = parser->peek_token;
    parser->peek_token = gql_lexer_next_token(parser->lexer);
    
    if (parser->current_token.type == GQL_TOKEN_EOF) {
        parser->at_end = true;
    }
}

bool match_token(gql_parser_t *parser, gql_token_type_t token_type) {
    if (parser->current_token.type == token_type) {
        advance_token(parser);
        return true;
    }
    return false;
}

bool expect_token(gql_parser_t *parser, gql_token_type_t expected_type) {
    if (parser->current_token.type == expected_type) {
        advance_token(parser);
        return true;
    }
    
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), 
             "Expected %s but found %s", 
             gql_token_type_name(expected_type),
             gql_token_type_name(parser->current_token.type));
    parser_error_at_token(parser, error_msg);
    return false;
}

gql_operator_t token_to_operator(gql_token_type_t token_type) {
    switch (token_type) {
        case GQL_TOKEN_AND: return GQL_OP_AND;
        case GQL_TOKEN_OR: return GQL_OP_OR;
        case GQL_TOKEN_NOT: return GQL_OP_NOT;
        case GQL_TOKEN_EQUALS: return GQL_OP_EQUALS;
        case GQL_TOKEN_NOT_EQUALS: return GQL_OP_NOT_EQUALS;
        case GQL_TOKEN_LESS_THAN: return GQL_OP_LESS_THAN;
        case GQL_TOKEN_LESS_EQUAL: return GQL_OP_LESS_EQUAL;
        case GQL_TOKEN_GREATER_THAN: return GQL_OP_GREATER_THAN;
        case GQL_TOKEN_GREATER_EQUAL: return GQL_OP_GREATER_EQUAL;
        default: return GQL_OP_EQUALS; // Default fallback
    }
}

// ============================================================================
// Error Handling
// ============================================================================

void parser_error(gql_parser_t *parser, const char *message) {
    if (parser->has_error) return; // Don't overwrite first error
    
    parser->has_error = true;
    parser->error_message = strdup(message);
    parser->error_line = parser->current_token.line;
    parser->error_column = parser->current_token.column;
}

void parser_error_at_token(gql_parser_t *parser, const char *message) {
    if (parser->has_error) return;
    
    char full_message[512];
    snprintf(full_message, sizeof(full_message),
             "%s at line %zu, column %zu",
             message, parser->current_token.line, parser->current_token.column);
    parser_error(parser, full_message);
}

const char* gql_parser_get_error(gql_parser_t *parser) {
    return parser ? parser->error_message : NULL;
}

bool gql_parser_has_error(gql_parser_t *parser) {
    return parser && parser->has_error;
}

// ============================================================================
// Expression Parsing
// ============================================================================

gql_ast_node_t* parse_primary_expression(gql_parser_t *parser) {
    switch (parser->current_token.type) {
        case GQL_TOKEN_STRING:
        {
            char *value = strdup(parser->current_token.value);
            advance_token(parser);
            return gql_ast_create_string_literal(value);
        }
        
        case GQL_TOKEN_INTEGER:
        {
            int64_t value = strtoll(parser->current_token.value, NULL, 10);
            advance_token(parser);
            return gql_ast_create_integer_literal(value);
        }
        
        case GQL_TOKEN_TRUE:
            advance_token(parser);
            return gql_ast_create_boolean_literal(true);
            
        case GQL_TOKEN_FALSE:
            advance_token(parser);
            return gql_ast_create_boolean_literal(false);
            
        case GQL_TOKEN_NULL:
            advance_token(parser);
            return gql_ast_create_null_literal();
            
        case GQL_TOKEN_IDENTIFIER:
        {
            char *name = strdup(parser->current_token.value);
            advance_token(parser);
            
            // Check for property access (identifier.property)
            if (match_token(parser, GQL_TOKEN_DOT)) {
                if (parser->current_token.type == GQL_TOKEN_IDENTIFIER) {
                    char *property = strdup(parser->current_token.value);
                    advance_token(parser);
                    gql_ast_node_t *result = gql_ast_create_property_access(name, property);
                    free(name);
                    free(property);
                    return result;
                } else {
                    parser_error_at_token(parser, "Expected property name after '.'");
                    free(name);
                    return NULL;
                }
            }
            
            gql_ast_node_t *result = gql_ast_create_identifier(name);
            free(name);
            return result;
        }
        
        case GQL_TOKEN_LPAREN:
        {
            advance_token(parser); // consume '('
            gql_ast_node_t *expr = parse_expression(parser);
            if (!expect_token(parser, GQL_TOKEN_RPAREN)) {
                gql_ast_free_recursive(expr);
                return NULL;
            }
            return expr;
        }
        
        default:
            parser_error_at_token(parser, "Expected expression");
            return NULL;
    }
}

gql_ast_node_t* parse_comparison_expression(gql_parser_t *parser) {
    gql_ast_node_t *left = parse_primary_expression(parser);
    if (!left) return NULL;
    
    // Check for comparison operators
    gql_token_type_t op_token = parser->current_token.type;
    if (op_token == GQL_TOKEN_EQUALS || op_token == GQL_TOKEN_NOT_EQUALS ||
        op_token == GQL_TOKEN_LESS_THAN || op_token == GQL_TOKEN_LESS_EQUAL ||
        op_token == GQL_TOKEN_GREATER_THAN || op_token == GQL_TOKEN_GREATER_EQUAL) {
        
        gql_operator_t op = token_to_operator(op_token);
        advance_token(parser);
        
        gql_ast_node_t *right = parse_primary_expression(parser);
        if (!right) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        return gql_ast_create_binary_expr(op, left, right);
    }
    
    // Check for string operators
    if (parser->current_token.type == GQL_TOKEN_STARTS) {
        advance_token(parser);
        if (!expect_token(parser, GQL_TOKEN_WITH)) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        gql_ast_node_t *right = parse_primary_expression(parser);
        if (!right) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        return gql_ast_create_binary_expr(GQL_OP_STARTS_WITH, left, right);
    }
    
    if (parser->current_token.type == GQL_TOKEN_ENDS) {
        advance_token(parser);
        if (!expect_token(parser, GQL_TOKEN_WITH)) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        gql_ast_node_t *right = parse_primary_expression(parser);
        if (!right) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        return gql_ast_create_binary_expr(GQL_OP_ENDS_WITH, left, right);
    }
    
    if (parser->current_token.type == GQL_TOKEN_CONTAINS) {
        advance_token(parser);
        
        gql_ast_node_t *right = parse_primary_expression(parser);
        if (!right) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        return gql_ast_create_binary_expr(GQL_OP_CONTAINS, left, right);
    }
    
    // Check for IS NULL / IS NOT NULL
    if (parser->current_token.type == GQL_TOKEN_IS) {
        advance_token(parser);
        
        if (parser->current_token.type == GQL_TOKEN_NOT) {
            advance_token(parser);
            if (!expect_token(parser, GQL_TOKEN_NULL)) {
                gql_ast_free_recursive(left);
                return NULL;
            }
            return gql_ast_create_unary_expr(GQL_OP_IS_NOT_NULL, left);
        } else if (parser->current_token.type == GQL_TOKEN_NULL) {
            advance_token(parser);
            return gql_ast_create_unary_expr(GQL_OP_IS_NULL, left);
        } else {
            parser_error_at_token(parser, "Expected NULL after IS");
            gql_ast_free_recursive(left);
            return NULL;
        }
    }
    
    return left;
}

gql_ast_node_t* parse_not_expression(gql_parser_t *parser) {
    if (match_token(parser, GQL_TOKEN_NOT)) {
        gql_ast_node_t *operand = parse_comparison_expression(parser);
        if (!operand) return NULL;
        
        return gql_ast_create_unary_expr(GQL_OP_NOT, operand);
    }
    
    return parse_comparison_expression(parser);
}

gql_ast_node_t* parse_and_expression(gql_parser_t *parser) {
    gql_ast_node_t *left = parse_not_expression(parser);
    if (!left) return NULL;
    
    while (match_token(parser, GQL_TOKEN_AND)) {
        gql_ast_node_t *right = parse_not_expression(parser);
        if (!right) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        left = gql_ast_create_binary_expr(GQL_OP_AND, left, right);
    }
    
    return left;
}

gql_ast_node_t* parse_or_expression(gql_parser_t *parser) {
    gql_ast_node_t *left = parse_and_expression(parser);
    if (!left) return NULL;
    
    while (match_token(parser, GQL_TOKEN_OR)) {
        gql_ast_node_t *right = parse_and_expression(parser);
        if (!right) {
            gql_ast_free_recursive(left);
            return NULL;
        }
        
        left = gql_ast_create_binary_expr(GQL_OP_OR, left, right);
    }
    
    return left;
}

gql_ast_node_t* parse_expression(gql_parser_t *parser) {
    return parse_or_expression(parser);
}

// ============================================================================
// Pattern Parsing
// ============================================================================

gql_ast_node_t* parse_property_map(gql_parser_t *parser) {
    if (!match_token(parser, GQL_TOKEN_LBRACE)) {
        return NULL; // No property map
    }
    
    gql_ast_node_t *properties = gql_ast_create_list(GQL_AST_PROPERTY_MAP);
    if (!properties) return NULL;
    
    // Parse properties: key: value, key: value, ...
    if (parser->current_token.type != GQL_TOKEN_RBRACE) {
        do {
            if (parser->current_token.type != GQL_TOKEN_IDENTIFIER) {
                parser_error_at_token(parser, "Expected property name");
                gql_ast_free_recursive(properties);
                return NULL;
            }
            
            char *key = strdup(parser->current_token.value);
            advance_token(parser);
            
            if (!expect_token(parser, GQL_TOKEN_COLON)) {
                free(key);
                gql_ast_free_recursive(properties);
                return NULL;
            }
            
            gql_ast_node_t *value = parse_primary_expression(parser);
            if (!value) {
                free(key);
                gql_ast_free_recursive(properties);
                return NULL;
            }
            
            // Create property assignment node
            gql_ast_node_t *prop = gql_ast_create_binary_expr(GQL_OP_EQUALS,
                                                             gql_ast_create_identifier(key),
                                                             value);
            free(key);
            
            gql_ast_list_append(properties, prop);
            
        } while (match_token(parser, GQL_TOKEN_COMMA));
    }
    
    if (!expect_token(parser, GQL_TOKEN_RBRACE)) {
        gql_ast_free_recursive(properties);
        return NULL;
    }
    
    return properties;
}

gql_ast_node_t* parse_node_pattern(gql_parser_t *parser) {
    if (!expect_token(parser, GQL_TOKEN_LPAREN)) {
        return NULL;
    }
    
    char *variable = NULL;
    gql_ast_node_t *properties = NULL;
    
    // Parse variable name (optional)
    if (parser->current_token.type == GQL_TOKEN_IDENTIFIER) {
        variable = strdup(parser->current_token.value);
        advance_token(parser);
    }
    
    // Parse labels (optional)
    gql_ast_node_t *labels = NULL;
    if (match_token(parser, GQL_TOKEN_COLON)) {
        labels = gql_ast_create_list(GQL_AST_RETURN_LIST);  // Reuse list type for labels
        if (!labels) {
            free(variable);
            return NULL;
        }
        
        // Parse first label
        if (parser->current_token.type != GQL_TOKEN_IDENTIFIER) {
            parser_error_at_token(parser, "Expected label name after ':'");
            free(variable);
            gql_ast_free_recursive(labels);
            return NULL;
        }
        
        gql_ast_node_t *label_node = gql_ast_create_string_literal(parser->current_token.value);
        if (!label_node) {
            free(variable);
            gql_ast_free_recursive(labels);
            return NULL;
        }
        gql_ast_list_append(labels, label_node);
        advance_token(parser);
        
        // Parse additional labels separated by &
        while (match_token(parser, GQL_TOKEN_AMPERSAND)) {
            if (parser->current_token.type != GQL_TOKEN_IDENTIFIER) {
                parser_error_at_token(parser, "Expected label name after '&'");
                free(variable);
                gql_ast_free_recursive(labels);
                return NULL;
            }
            
            gql_ast_node_t *additional_label = gql_ast_create_string_literal(parser->current_token.value);
            if (!additional_label) {
                free(variable);
                gql_ast_free_recursive(labels);
                return NULL;
            }
            gql_ast_list_append(labels, additional_label);
            advance_token(parser);
        }
    }
    
    // Parse properties (optional)
    properties = parse_property_map(parser);
    
    if (!expect_token(parser, GQL_TOKEN_RPAREN)) {
        free(variable);
        gql_ast_free_recursive(labels);
        gql_ast_free_recursive(properties);
        return NULL;
    }
    
    gql_ast_node_t *result = gql_ast_create_node_pattern(variable, labels, properties);
    free(variable);
    
    return result;
}

gql_ast_node_t* parse_edge_pattern(gql_parser_t *parser) {
    bool directed = true;
    bool left_arrow = false;
    
    // Check for left arrow
    if (match_token(parser, GQL_TOKEN_ARROW_LEFT)) {
        left_arrow = true;
    } else if (!match_token(parser, GQL_TOKEN_DASH)) {
        return NULL; // No edge pattern
    }
    
    if (!expect_token(parser, GQL_TOKEN_LBRACKET)) {
        return NULL;
    }
    
    char *variable = NULL;
    char *type = NULL;
    gql_ast_node_t *properties = NULL;
    
    // Parse variable name (optional)
    if (parser->current_token.type == GQL_TOKEN_IDENTIFIER) {
        variable = strdup(parser->current_token.value);
        advance_token(parser);
    }
    
    // Parse type (optional)
    if (match_token(parser, GQL_TOKEN_COLON)) {
        if (parser->current_token.type != GQL_TOKEN_IDENTIFIER) {
            parser_error_at_token(parser, "Expected edge type after ':'");
            free(variable);
            return NULL;
        }
        type = strdup(parser->current_token.value);
        advance_token(parser);
    }
    
    // Parse properties (optional)
    properties = parse_property_map(parser);
    
    if (!expect_token(parser, GQL_TOKEN_RBRACKET)) {
        free(variable);
        free(type);
        gql_ast_free_recursive(properties);
        return NULL;
    }
    
    // Check for right arrow
    if (match_token(parser, GQL_TOKEN_ARROW_RIGHT)) {
        directed = true;
    } else if (match_token(parser, GQL_TOKEN_DASH)) {
        directed = false;
    } else {
        parser_error_at_token(parser, "Expected '->' or '-' after edge pattern");
        free(variable);
        free(type);
        gql_ast_free_recursive(properties);
        return NULL;
    }
    
    gql_ast_node_t *result = gql_ast_create_edge_pattern(variable, type, properties, directed && !left_arrow);
    free(variable);
    free(type);
    
    return result;
}

gql_ast_node_t* parse_pattern(gql_parser_t *parser) {
    gql_ast_node_t *node = parse_node_pattern(parser);
    if (!node) return NULL;
    
    // Check for edge pattern
    gql_ast_node_t *edge = parse_edge_pattern(parser);
    if (!edge) {
        return node; // Just a single node pattern
    }
    
    // Parse target node
    gql_ast_node_t *target_node = parse_node_pattern(parser);
    if (!target_node) {
        parser_error_at_token(parser, "Expected target node after edge pattern");
        gql_ast_free_recursive(node);
        gql_ast_free_recursive(edge);
        return NULL;
    }
    
    // Create the initial pattern
    gql_ast_node_t *pattern = gql_ast_create_pattern(node, edge, target_node);
    if (!pattern) {
        gql_ast_free_recursive(node);
        gql_ast_free_recursive(edge);
        gql_ast_free_recursive(target_node);
        return NULL;
    }
    
    // Check for additional hops (multi-hop patterns)
    while (true) {
        gql_ast_node_t *next_edge = parse_edge_pattern(parser);
        if (!next_edge) {
            break; // No more edges, we're done
        }
        
        gql_ast_node_t *next_node = parse_node_pattern(parser);
        if (!next_node) {
            parser_error_at_token(parser, "Expected node after edge pattern in multi-hop");
            gql_ast_free_recursive(next_edge);
            gql_ast_free_recursive(pattern);
            return NULL;
        }
        
        // Create a new pattern that extends the previous one
        gql_ast_node_t *new_pattern = gql_ast_create_pattern(pattern, next_edge, next_node);
        if (!new_pattern) {
            gql_ast_free_recursive(next_edge);
            gql_ast_free_recursive(next_node);
            gql_ast_free_recursive(pattern);
            return NULL;
        }
        
        pattern = new_pattern;
    }
    
    return pattern;
}

gql_ast_node_t* parse_pattern_list(gql_parser_t *parser) {
    gql_ast_node_t *patterns = gql_ast_create_list(GQL_AST_PATTERN_LIST);
    if (!patterns) return NULL;
    
    do {
        gql_ast_node_t *pattern = parse_pattern(parser);
        if (!pattern) {
            gql_ast_free_recursive(patterns);
            return NULL;
        }
        
        gql_ast_list_append(patterns, pattern);
        
    } while (match_token(parser, GQL_TOKEN_COMMA));
    
    return patterns;
}

// ============================================================================
// Clause Parsing
// ============================================================================

gql_ast_node_t* parse_where_clause(gql_parser_t *parser) {
    if (!match_token(parser, GQL_TOKEN_WHERE)) {
        return NULL; // No WHERE clause
    }
    
    gql_ast_node_t *expression = parse_expression(parser);
    if (!expression) return NULL;
    
    gql_ast_node_t *where_clause = malloc(sizeof(gql_ast_node_t));
    if (!where_clause) {
        gql_ast_free_recursive(expression);
        return NULL;
    }
    
    where_clause->type = GQL_AST_WHERE_CLAUSE;
    where_clause->data.where_clause.expression = expression;
    where_clause->next = NULL;
    where_clause->owns_strings = true;
    
    return where_clause;
}

gql_ast_node_t* parse_return_clause(gql_parser_t *parser) {
    if (!expect_token(parser, GQL_TOKEN_RETURN)) {
        return NULL;
    }
    
    bool distinct = match_token(parser, GQL_TOKEN_DISTINCT);
    
    gql_ast_node_t *items = gql_ast_create_list(GQL_AST_RETURN_LIST);
    if (!items) return NULL;
    
    do {
        gql_ast_node_t *expression = parse_expression(parser);
        if (!expression) {
            gql_ast_free_recursive(items);
            return NULL;
        }
        
        // Check for AS alias
        char *alias = NULL;
        if (match_token(parser, GQL_TOKEN_AS)) {
            if (parser->current_token.type != GQL_TOKEN_IDENTIFIER) {
                parser_error_at_token(parser, "Expected alias name after AS");
                gql_ast_free_recursive(expression);
                gql_ast_free_recursive(items);
                return NULL;
            }
            alias = strdup(parser->current_token.value);
            advance_token(parser);
        }
        
        // Create return item with expression and optional alias
        gql_ast_node_t *return_item = gql_ast_create_return_item(expression, alias);
        if (!return_item) {
            gql_ast_free_recursive(expression);
            free(alias);
            gql_ast_free_recursive(items);
            return NULL;
        }
        free(alias); // Safe to free since create_return_item makes its own copy
        
        gql_ast_list_append(items, return_item);
        
    } while (match_token(parser, GQL_TOKEN_COMMA));
    
    gql_ast_node_t *return_clause = malloc(sizeof(gql_ast_node_t));
    if (!return_clause) {
        gql_ast_free_recursive(items);
        return NULL;
    }
    
    return_clause->type = GQL_AST_RETURN_CLAUSE;
    return_clause->data.return_clause.items = items;
    return_clause->data.return_clause.distinct = distinct;
    return_clause->next = NULL;
    return_clause->owns_strings = true;
    
    return return_clause;
}

// ============================================================================
// Query Parsing
// ============================================================================

gql_ast_node_t* parse_match_query(gql_parser_t *parser) {
    if (!expect_token(parser, GQL_TOKEN_MATCH)) {
        return NULL;
    }
    
    gql_ast_node_t *patterns = parse_pattern_list(parser);
    if (!patterns) return NULL;
    
    gql_ast_node_t *where_clause = parse_where_clause(parser);
    
    gql_ast_node_t *return_clause = parse_return_clause(parser);
    if (!return_clause) {
        gql_ast_free_recursive(patterns);
        gql_ast_free_recursive(where_clause);
        return NULL;
    }
    
    return gql_ast_create_match_query(patterns, where_clause, return_clause);
}

gql_ast_node_t* parse_create_query(gql_parser_t *parser) {
    if (!expect_token(parser, GQL_TOKEN_CREATE)) {
        return NULL;
    }
    
    gql_ast_node_t *patterns = parse_pattern_list(parser);
    if (!patterns) return NULL;
    
    return gql_ast_create_create_query(patterns);
}

gql_ast_node_t* parse_query(gql_parser_t *parser) {
    switch (parser->current_token.type) {
        case GQL_TOKEN_MATCH:
            return parse_match_query(parser);
        case GQL_TOKEN_CREATE:
            return parse_create_query(parser);
        default:
            parser_error_at_token(parser, "Expected query (MATCH, CREATE, SET, or DELETE)");
            return NULL;
    }
}

// ============================================================================
// Main Parse Function
// ============================================================================

gql_ast_node_t* gql_parser_parse(gql_parser_t *parser) {
    if (!parser) return NULL;
    
    if (parser->current_token.type == GQL_TOKEN_EOF) {
        parser_error(parser, "Empty query");
        return NULL;
    }
    
    gql_ast_node_t *ast = parse_query(parser);
    
    // Expect end of input
    if (ast && parser->current_token.type != GQL_TOKEN_EOF) {
        parser_error_at_token(parser, "Unexpected token after query");
        gql_ast_free_recursive(ast);
        return NULL;
    }
    
    return ast;
}