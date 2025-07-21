---
id: property-management-system
level: task
title: "Property management system"
created_at: 2025-07-19T23:02:37.014455+00:00
updated_at: 2025-07-19T23:02:37.014455+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Property management system

## Parent Initiative

[[foundation-layer]]

## Objective

Implement a unified property management system that handles type validation, conversion, and storage operations across all typed EAV tables. This system provides a consistent interface for property operations while abstracting the complexity of managing multiple typed property tables.

The property management system must provide type safety, efficient batch operations, and automatic type inference to simplify the developer experience while maintaining the performance benefits of typed storage.

## Acceptance Criteria

- [ ] **Type Safety**: Automatic type validation and conversion for all property values
- [ ] **Unified Interface**: Single API for property operations across nodes and edges
- [ ] **Batch Operations**: Efficient bulk property setting with transaction management
- [ ] **Type Inference**: Automatic type detection from value types (int, string, float, bool)
- [ ] **Property Queries**: Support for property-based filtering and search operations
- [ ] **Memory Management**: Proper cleanup and memory handling for property values
- [ ] **Error Handling**: Clear error messages for type mismatches and invalid operations
- [ ] **Performance**: Property operations maintain sub-5ms performance for single operations

## Implementation Notes

**Property Value System:**

```c
typedef enum {
    PROP_INT,
    PROP_TEXT,
    PROP_REAL,
    PROP_BOOL,
    PROP_NULL
} property_type_t;

typedef struct {
    property_type_t type;
    union {
        int64_t int_val;
        char *text_val;
        double real_val;
        int bool_val;
    } value;
} property_value_t;

typedef struct {
    char *key;
    property_value_t value;
} property_pair_t;

typedef struct {
    property_pair_t *properties;
    size_t count;
    size_t capacity;
} property_set_t;
```

**Unified Property API:**

```c
// Single property operations
int graphqlite_set_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value);
int graphqlite_get_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value);
int graphqlite_delete_property(graphqlite_db_t *db, entity_type_t entity_type,
                              int64_t entity_id, const char *key);

// Batch property operations
int graphqlite_set_properties(graphqlite_db_t *db, entity_type_t entity_type,
                             int64_t entity_id, property_set_t *properties);
property_set_t* graphqlite_get_all_properties(graphqlite_db_t *db, entity_type_t entity_type,
                                             int64_t entity_id);

// Property queries
int64_t* graphqlite_find_entities_by_property(graphqlite_db_t *db, entity_type_t entity_type,
                                             const char *key, property_value_t *value,
                                             size_t *count);

// Convenience functions for common types
int graphqlite_set_int_property(graphqlite_db_t *db, entity_type_t entity_type,
                               int64_t entity_id, const char *key, int64_t value);
int graphqlite_set_text_property(graphqlite_db_t *db, entity_type_t entity_type,
                                int64_t entity_id, const char *key, const char *value);
int graphqlite_set_real_property(graphqlite_db_t *db, entity_type_t entity_type,
                                int64_t entity_id, const char *key, double value);
int graphqlite_set_bool_property(graphqlite_db_t *db, entity_type_t entity_type,
                                int64_t entity_id, const char *key, bool value);
```

**Type Inference and Validation:**

```c
property_type_t infer_property_type(const char *value_str) {
    if (!value_str) return PROP_NULL;
    
    // Try boolean first (true/false)
    if (strcasecmp(value_str, "true") == 0 || strcasecmp(value_str, "false") == 0) {
        return PROP_BOOL;
    }
    
    // Try integer
    char *endptr;
    long long int_val = strtoll(value_str, &endptr, 10);
    if (*endptr == '\0' && errno != ERANGE) {
        return PROP_INT;
    }
    
    // Try real number
    double real_val = strtod(value_str, &endptr);
    if (*endptr == '\0' && errno != ERANGE) {
        return PROP_REAL;
    }
    
    // Default to text
    return PROP_TEXT;
}

int validate_property_value(property_value_t *prop) {
    switch (prop->type) {
        case PROP_INT:
            // No additional validation needed for int64_t
            return 1;
        case PROP_TEXT:
            if (!prop->value.text_val) return 0;
            // Check for reasonable string length
            if (strlen(prop->value.text_val) > MAX_PROPERTY_TEXT_LENGTH) return 0;
            return 1;
        case PROP_REAL:
            // Check for NaN and infinity
            if (isnan(prop->value.real_val) || isinf(prop->value.real_val)) return 0;
            return 1;
        case PROP_BOOL:
            // Boolean stored as 0 or 1
            return (prop->value.bool_val == 0 || prop->value.bool_val == 1);
        case PROP_NULL:
            return 1;
        default:
            return 0;
    }
}
```

**Property Storage Implementation:**

```c
int graphqlite_set_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value) {
    if (!db || !key || !value || entity_id <= 0) return SQLITE_ERROR;
    
    // Validate property value
    if (!validate_property_value(value)) {
        return SQLITE_ERROR;
    }
    
    // Intern property key
    int key_id = intern_property_key(db->key_cache, key);
    if (key_id < 0) return SQLITE_ERROR;
    
    // Select appropriate prepared statement based on type and entity
    sqlite3_stmt *stmt;
    const char *table_prefix = (entity_type == ENTITY_NODE) ? "node_props" : "edge_props";
    
    switch (value->type) {
        case PROP_INT:
            stmt = get_prepared_statement(db, table_prefix, "int", "set");
            sqlite3_bind_int64(stmt, 1, entity_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_int64(stmt, 3, value->value.int_val);
            break;
        case PROP_TEXT:
            stmt = get_prepared_statement(db, table_prefix, "text", "set");
            sqlite3_bind_int64(stmt, 1, entity_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_text(stmt, 3, value->value.text_val, -1, SQLITE_STATIC);
            break;
        case PROP_REAL:
            stmt = get_prepared_statement(db, table_prefix, "real", "set");
            sqlite3_bind_int64(stmt, 1, entity_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_double(stmt, 3, value->value.real_val);
            break;
        case PROP_BOOL:
            stmt = get_prepared_statement(db, table_prefix, "bool", "set");
            sqlite3_bind_int64(stmt, 1, entity_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_int(stmt, 3, value->value.bool_val ? 1 : 0);
            break;
        case PROP_NULL:
            // Delete property if setting to NULL
            return graphqlite_delete_property(db, entity_type, entity_id, key);
        default:
            return SQLITE_ERROR;
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}
```

**Property Set Management:**

```c
property_set_t* create_property_set(void) {
    property_set_t *set = malloc(sizeof(property_set_t));
    set->properties = malloc(sizeof(property_pair_t) * INITIAL_PROPERTY_CAPACITY);
    set->count = 0;
    set->capacity = INITIAL_PROPERTY_CAPACITY;
    return set;
}

int add_property_to_set(property_set_t *set, const char *key, property_value_t *value) {
    if (set->count >= set->capacity) {
        set->capacity *= 2;
        set->properties = realloc(set->properties, 
                                sizeof(property_pair_t) * set->capacity);
    }
    
    set->properties[set->count].key = strdup(key);
    set->properties[set->count].value = *value;
    
    // Deep copy text values
    if (value->type == PROP_TEXT && value->value.text_val) {
        set->properties[set->count].value.value.text_val = strdup(value->value.text_val);
    }
    
    set->count++;
    return 0;
}

void free_property_set(property_set_t *set) {
    if (!set) return;
    
    for (size_t i = 0; i < set->count; i++) {
        free(set->properties[i].key);
        if (set->properties[i].value.type == PROP_TEXT) {
            free(set->properties[i].value.value.text_val);
        }
    }
    
    free(set->properties);
    free(set);
}
```

**Performance Optimizations:**
- Use prepared statements for all property operations
- Batch property operations use single transaction
- Property queries leverage key-value indexes for fast lookups
- Memory pooling for frequent property_value_t allocations
- Lazy loading for property sets to minimize memory usage
- Type-specific query optimization based on property type

## Status Updates

*To be added during implementation*