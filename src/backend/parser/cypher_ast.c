/*
 * Cypher AST Node Implementation
 * Simple C structures for representing Cypher queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Memory management functions */

ast_node* ast_node_create(ast_node_type type, int location, size_t size)
{
    ast_node *node = calloc(1, size);
    if (!node) {
        return NULL;
    }
    
    node->type = type;
    node->location = location;
    node->data = NULL;
    
    return node;
}

void ast_node_free(ast_node *node)
{
    if (!node) {
        return;
    }
    
    CYPHER_DEBUG("Freeing node type %s at %p", ast_node_type_name(node->type), (void*)node);
    
    /* Set type to unknown to detect double-free attempts */
    ast_node_type original_type = node->type;
    if (original_type == AST_NODE_UNKNOWN) {
        /* Already freed - ignore silently to prevent crash */
        CYPHER_DEBUG("Double-free detected for node at %p", (void*)node);
        return;
    }
    node->type = AST_NODE_UNKNOWN;
    
    /* Free type-specific data based on node type */
    switch (original_type) {
        case AST_NODE_QUERY:
            {
                cypher_query *query = (cypher_query*)node;
                if (query->clauses) {
                    ast_list_free(query->clauses);
                    query->clauses = NULL;
                }
            }
            break;
            
        case AST_NODE_MATCH:
            {
                cypher_match *match = (cypher_match*)node;
                if (match->pattern) {
                    ast_list_free(match->pattern);
                    match->pattern = NULL;
                }
                if (match->where) {
                    ast_node_free(match->where);
                    match->where = NULL;
                }
            }
            break;
            
        case AST_NODE_RETURN:
            {
                cypher_return *ret = (cypher_return*)node;
                if (ret->items) {
                    ast_list_free(ret->items);
                    ret->items = NULL;
                }
                if (ret->order_by) {
                    ast_list_free(ret->order_by);
                    ret->order_by = NULL;
                }
                if (ret->skip) {
                    ast_node_free(ret->skip);
                    ret->skip = NULL;
                }
                if (ret->limit) {
                    ast_node_free(ret->limit);
                    ret->limit = NULL;
                }
            }
            break;
            
        case AST_NODE_CREATE:
            {
                cypher_create *create = (cypher_create*)node;
                ast_list_free(create->pattern);
            }
            break;
            
        case AST_NODE_RETURN_ITEM:
            {
                cypher_return_item *item = (cypher_return_item*)node;
                ast_node_free(item->expr);
                free(item->alias);
            }
            break;
            
        case AST_NODE_NODE_PATTERN:
            {
                cypher_node_pattern *pattern = (cypher_node_pattern*)node;
                CYPHER_DEBUG("NODE_PATTERN variable=%p, label=%p, properties=%p", 
                       (void*)pattern->variable, (void*)pattern->label, (void*)pattern->properties);
                
                if (pattern->variable) {
                    CYPHER_DEBUG("Freeing variable: %s", pattern->variable);
                    free(pattern->variable);
                }
                if (pattern->label) {
                    CYPHER_DEBUG("Freeing label: %s", pattern->label);
                    free(pattern->label);
                }
                if (pattern->properties) {
                    CYPHER_DEBUG("Freeing properties node");
                    ast_node_free(pattern->properties);
                }
                CYPHER_DEBUG("NODE_PATTERN freeing completed");
            }
            break;
            
        case AST_NODE_REL_PATTERN:
            {
                cypher_rel_pattern *pattern = (cypher_rel_pattern*)node;
                free(pattern->variable);
                free(pattern->type);
                ast_node_free(pattern->properties);
                ast_node_free(pattern->varlen);
            }
            break;
            
        case AST_NODE_PATH:
            {
                cypher_path *path = (cypher_path*)node;
                ast_list_free(path->elements);
            }
            break;
            
        case AST_NODE_LITERAL:
            {
                cypher_literal *literal = (cypher_literal*)node;
                if (literal->literal_type == LITERAL_STRING) {
                    free(literal->value.string);
                }
            }
            break;
            
        case AST_NODE_IDENTIFIER:
            {
                cypher_identifier *id = (cypher_identifier*)node;
                free(id->name);
            }
            break;
            
        case AST_NODE_PARAMETER:
            {
                cypher_parameter *param = (cypher_parameter*)node;
                free(param->name);
            }
            break;
            
        case AST_NODE_PROPERTY:
            {
                cypher_property *prop = (cypher_property*)node;
                ast_node_free(prop->expr);
                free(prop->property_name);
            }
            break;
            
        case AST_NODE_FUNCTION_CALL:
            {
                cypher_function_call *func = (cypher_function_call*)node;
                free(func->function_name);
                ast_list_free(func->args);
            }
            break;
            
        case AST_NODE_MAP:
            {
                cypher_map *map = (cypher_map*)node;
                ast_list_free(map->pairs);
            }
            break;
            
        case AST_NODE_LIST:
            {
                cypher_list *list = (cypher_list*)node;
                ast_list_free(list->items);
            }
            break;
            
        default:
            /* Other node types don't have special cleanup needs */
            break;
    }
    
    free(node);
}

ast_list* ast_list_create(void)
{
    ast_list *list = calloc(1, sizeof(ast_list));
    if (!list) {
        return NULL;
    }
    
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    
    return list;
}

void ast_list_free(ast_list *list)
{
    if (!list) {
        return;
    }
    
    CYPHER_DEBUG("ast_list_free called with list %p, count=%d", (void*)list, list->count);
    
    /* Guard against double-free by checking count */
    if (list->count < 0) {
        /* Already freed - ignore silently to prevent crash */
        CYPHER_DEBUG("List already freed, ignoring");
        return;
    }
    
    /* Free all nodes in the list */
    for (int i = 0; i < list->count; i++) {
        CYPHER_DEBUG("Freeing list item %d at %p", i, (void*)list->items[i]);
        ast_node_free(list->items[i]);
    }
    
    free(list->items);
    
    /* Mark as freed */
    list->count = -1;
    list->items = NULL;
    
    free(list);
}

void ast_list_append(ast_list *list, ast_node *node)
{
    if (!list || !node) {
        return;
    }
    
    /* Grow array if needed */
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        ast_node **new_items = realloc(list->items, new_capacity * sizeof(ast_node*));
        if (!new_items) {
            return; /* Memory allocation failed */
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->count++] = node;
}

/* Node creation functions */

cypher_query* make_cypher_query(ast_list *clauses)
{
    cypher_query *query = (cypher_query*)ast_node_create(AST_NODE_QUERY, -1, sizeof(cypher_query));
    if (!query) {
        return NULL;
    }
    
    query->clauses = clauses;
    return query;
}

cypher_match* make_cypher_match(ast_list *pattern, ast_node *where, bool optional)
{
    cypher_match *match = (cypher_match*)ast_node_create(AST_NODE_MATCH, -1, sizeof(cypher_match));
    if (!match) {
        return NULL;
    }
    
    match->pattern = pattern;
    match->where = where;
    match->optional = optional;
    return match;
}

cypher_return* make_cypher_return(bool distinct, ast_list *items, ast_list *order_by, ast_node *skip, ast_node *limit)
{
    cypher_return *ret = (cypher_return*)ast_node_create(AST_NODE_RETURN, -1, sizeof(cypher_return));
    if (!ret) {
        return NULL;
    }
    
    ret->distinct = distinct;
    ret->items = items;
    ret->order_by = order_by;
    ret->skip = skip;
    ret->limit = limit;
    return ret;
}

cypher_create* make_cypher_create(ast_list *pattern)
{
    cypher_create *create = (cypher_create*)ast_node_create(AST_NODE_CREATE, -1, sizeof(cypher_create));
    if (!create) {
        return NULL;
    }
    
    create->pattern = pattern;
    return create;
}

cypher_return_item* make_return_item(ast_node *expr, char *alias)
{
    cypher_return_item *item = (cypher_return_item*)ast_node_create(AST_NODE_RETURN_ITEM, -1, sizeof(cypher_return_item));
    if (!item) {
        return NULL;
    }
    
    item->expr = expr;
    item->alias = alias ? strdup(alias) : NULL;
    return item;
}

cypher_node_pattern* make_node_pattern(char *variable, char *label, ast_node *properties)
{
    CYPHER_DEBUG("make_node_pattern called with variable=%p, label=%p, properties=%p", 
           (void*)variable, (void*)label, (void*)properties);
    if (variable) CYPHER_DEBUG("  variable='%s'", variable);
    if (label) CYPHER_DEBUG("  label='%s'", label);
    
    cypher_node_pattern *pattern = (cypher_node_pattern*)ast_node_create(AST_NODE_NODE_PATTERN, -1, sizeof(cypher_node_pattern));
    if (!pattern) {
        return NULL;
    }
    
    CYPHER_DEBUG("Created NODE_PATTERN at %p", (void*)pattern);
    
    pattern->variable = variable ? strdup(variable) : NULL;
    pattern->label = label ? strdup(label) : NULL;
    pattern->properties = properties;
    
    CYPHER_DEBUG("Set variable=%p, label=%p, properties=%p", 
           (void*)pattern->variable, (void*)pattern->label, (void*)pattern->properties);
    
    return pattern;
}

cypher_rel_pattern* make_rel_pattern(char *variable, char *type, ast_node *properties, bool left_arrow, bool right_arrow)
{
    cypher_rel_pattern *pattern = (cypher_rel_pattern*)ast_node_create(AST_NODE_REL_PATTERN, -1, sizeof(cypher_rel_pattern));
    if (!pattern) {
        return NULL;
    }
    
    pattern->variable = variable ? strdup(variable) : NULL;
    pattern->type = type ? strdup(type) : NULL;
    pattern->properties = properties;
    pattern->left_arrow = left_arrow;
    pattern->right_arrow = right_arrow;
    pattern->varlen = NULL;
    return pattern;
}

cypher_path* make_path(ast_list *elements)
{
    cypher_path *path = (cypher_path*)ast_node_create(AST_NODE_PATH, -1, sizeof(cypher_path));
    if (!path) {
        return NULL;
    }
    
    path->elements = elements;
    return path;
}

cypher_literal* make_integer_literal(int value, int location)
{
    cypher_literal *literal = (cypher_literal*)ast_node_create(AST_NODE_LITERAL, location, sizeof(cypher_literal));
    if (!literal) {
        return NULL;
    }
    
    literal->literal_type = LITERAL_INTEGER;
    literal->value.integer = value;
    return literal;
}

cypher_literal* make_decimal_literal(double value, int location)
{
    cypher_literal *literal = (cypher_literal*)ast_node_create(AST_NODE_LITERAL, location, sizeof(cypher_literal));
    if (!literal) {
        return NULL;
    }
    
    literal->literal_type = LITERAL_DECIMAL;
    literal->value.decimal = value;
    return literal;
}

cypher_literal* make_string_literal(char *value, int location)
{
    cypher_literal *literal = (cypher_literal*)ast_node_create(AST_NODE_LITERAL, location, sizeof(cypher_literal));
    if (!literal) {
        return NULL;
    }
    
    literal->literal_type = LITERAL_STRING;
    literal->value.string = value ? strdup(value) : NULL;
    return literal;
}

cypher_literal* make_boolean_literal(bool value, int location)
{
    cypher_literal *literal = (cypher_literal*)ast_node_create(AST_NODE_LITERAL, location, sizeof(cypher_literal));
    if (!literal) {
        return NULL;
    }
    
    literal->literal_type = LITERAL_BOOLEAN;
    literal->value.boolean = value;
    return literal;
}

cypher_literal* make_null_literal(int location)
{
    cypher_literal *literal = (cypher_literal*)ast_node_create(AST_NODE_LITERAL, location, sizeof(cypher_literal));
    if (!literal) {
        return NULL;
    }
    
    literal->literal_type = LITERAL_NULL;
    return literal;
}

cypher_identifier* make_identifier(char *name, int location)
{
    cypher_identifier *id = (cypher_identifier*)ast_node_create(AST_NODE_IDENTIFIER, location, sizeof(cypher_identifier));
    if (!id) {
        return NULL;
    }
    
    id->name = name ? strdup(name) : NULL;
    return id;
}

cypher_parameter* make_parameter(char *name, int location)
{
    cypher_parameter *param = (cypher_parameter*)ast_node_create(AST_NODE_PARAMETER, location, sizeof(cypher_parameter));
    if (!param) {
        return NULL;
    }
    
    param->name = name ? strdup(name) : NULL;
    return param;
}

cypher_property* make_property(ast_node *expr, char *property_name, int location)
{
    cypher_property *prop = (cypher_property*)ast_node_create(AST_NODE_PROPERTY, location, sizeof(cypher_property));
    if (!prop) {
        return NULL;
    }
    
    prop->expr = expr;
    prop->property_name = property_name ? strdup(property_name) : NULL;
    return prop;
}

cypher_function_call* make_function_call(char *function_name, ast_list *args, bool distinct, int location)
{
    cypher_function_call *func = (cypher_function_call*)ast_node_create(AST_NODE_FUNCTION_CALL, location, sizeof(cypher_function_call));
    if (!func) {
        return NULL;
    }
    
    func->function_name = function_name ? strdup(function_name) : NULL;
    func->args = args;
    func->distinct = distinct;
    return func;
}

cypher_map* make_map(ast_list *pairs, int location)
{
    cypher_map *map = (cypher_map*)ast_node_create(AST_NODE_MAP, location, sizeof(cypher_map));
    if (!map) {
        return NULL;
    }
    
    map->pairs = pairs;
    return map;
}

cypher_map_pair* make_map_pair(char *key, ast_node *value, int location)
{
    cypher_map_pair *pair = (cypher_map_pair*)ast_node_create(AST_NODE_MAP, location, sizeof(cypher_map));
    if (!pair) {
        return NULL;
    }
    
    pair->key = key ? strdup(key) : NULL;
    pair->value = value;
    return pair;
}

cypher_list* make_list(ast_list *items, int location)
{
    cypher_list *list = (cypher_list*)ast_node_create(AST_NODE_LIST, location, sizeof(cypher_list));
    if (!list) {
        return NULL;
    }
    
    list->items = items;
    return list;
}

/* Utility functions */

const char* ast_node_type_name(ast_node_type type)
{
    switch (type) {
        case AST_NODE_UNKNOWN:        return "UNKNOWN";
        case AST_NODE_QUERY:          return "QUERY";
        case AST_NODE_SINGLE_QUERY:   return "SINGLE_QUERY";
        case AST_NODE_MATCH:          return "MATCH";
        case AST_NODE_RETURN:         return "RETURN";
        case AST_NODE_CREATE:         return "CREATE";
        case AST_NODE_WHERE:          return "WHERE";
        case AST_NODE_WITH:           return "WITH";
        case AST_NODE_SET:            return "SET";
        case AST_NODE_DELETE:         return "DELETE";
        case AST_NODE_REMOVE:         return "REMOVE";
        case AST_NODE_MERGE:          return "MERGE";
        case AST_NODE_UNWIND:         return "UNWIND";
        case AST_NODE_PATTERN:        return "PATTERN";
        case AST_NODE_PATH:           return "PATH";
        case AST_NODE_NODE_PATTERN:   return "NODE_PATTERN";
        case AST_NODE_REL_PATTERN:    return "REL_PATTERN";
        case AST_NODE_EXPR:           return "EXPR";
        case AST_NODE_LITERAL:        return "LITERAL";
        case AST_NODE_IDENTIFIER:     return "IDENTIFIER";
        case AST_NODE_PARAMETER:      return "PARAMETER";
        case AST_NODE_PROPERTY:       return "PROPERTY";
        case AST_NODE_FUNCTION_CALL:  return "FUNCTION_CALL";
        case AST_NODE_LIST:           return "LIST";
        case AST_NODE_MAP:            return "MAP";
        case AST_NODE_RETURN_ITEM:    return "RETURN_ITEM";
        case AST_NODE_ORDER_BY:       return "ORDER_BY";
        case AST_NODE_SKIP:           return "SKIP";
        case AST_NODE_LIMIT:          return "LIMIT";
        default:                      return "UNKNOWN";
    }
}

void ast_node_print(ast_node *node, int indent)
{
    if (!node) {
        return;
    }
    
    /* Temporary recursion protection for debugging */
    if (indent > 5) {
        for (int i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("... (recursion detected)\n");
        return;
    }
    
    /* Print indentation */
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    printf("%s", ast_node_type_name(node->type));
    
    /* Print type-specific information */
    switch (node->type) {
        case AST_NODE_LITERAL:
            {
                cypher_literal *literal = (cypher_literal*)node;
                switch (literal->literal_type) {
                    case LITERAL_INTEGER:
                        printf(" = %d", literal->value.integer);
                        break;
                    case LITERAL_DECIMAL:
                        printf(" = %f", literal->value.decimal);
                        break;
                    case LITERAL_STRING:
                        printf(" = \"%s\"", literal->value.string ? literal->value.string : "");
                        break;
                    case LITERAL_BOOLEAN:
                        printf(" = %s", literal->value.boolean ? "true" : "false");
                        break;
                    case LITERAL_NULL:
                        printf(" = null");
                        break;
                }
            }
            break;
            
        case AST_NODE_IDENTIFIER:
            {
                cypher_identifier *id = (cypher_identifier*)node;
                printf(" = %s", id->name ? id->name : "");
            }
            break;
            
        case AST_NODE_PARAMETER:
            {
                cypher_parameter *param = (cypher_parameter*)node;
                printf(" = $%s", param->name ? param->name : "");
            }
            break;
            
        default:
            break;
    }
    
    printf("\n");
    
    /* Print children (basic version - can be expanded) */
    switch (node->type) {
        case AST_NODE_QUERY:
            {
                cypher_query *query = (cypher_query*)node;
                if (query->clauses) {
                    for (int i = 0; i < query->clauses->count; i++) {
                        ast_node_print(query->clauses->items[i], indent + 1);
                    }
                }
            }
            break;
            
        case AST_NODE_MATCH:
            {
                cypher_match *match = (cypher_match*)node;
                if (match->pattern) {
                    for (int i = 0; i < match->pattern->count; i++) {
                        ast_node_print(match->pattern->items[i], indent + 1);
                    }
                }
                if (match->where) {
                    ast_node_print(match->where, indent + 1);
                }
            }
            break;
            
        case AST_NODE_RETURN:
            {
                cypher_return *ret = (cypher_return*)node;
                if (ret->items) {
                    for (int i = 0; i < ret->items->count; i++) {
                        ast_node_print(ret->items->items[i], indent + 1);
                    }
                }
            }
            break;
            
        case AST_NODE_RETURN_ITEM:
            {
                cypher_return_item *item = (cypher_return_item*)node;
                if (item->expr) {
                    ast_node_print(item->expr, indent + 1);
                }
            }
            break;
            
        case AST_NODE_PATH:
            {
                cypher_path *path = (cypher_path*)node;
                if (path->elements) {
                    for (int i = 0; i < path->elements->count; i++) {
                        ast_node_print(path->elements->items[i], indent + 1);
                    }
                }
            }
            break;
            
        case AST_NODE_NODE_PATTERN:
            {
                cypher_node_pattern *pattern = (cypher_node_pattern*)node;
                if (pattern->variable) {
                    printf(" var=%s", pattern->variable);
                }
                if (pattern->label) {
                    printf(" label=%s", pattern->label);
                }
                if (pattern->properties) {
                    ast_node_print(pattern->properties, indent + 1);
                }
            }
            break;
            
        default:
            /* Add more cases as needed */
            break;
    }
}