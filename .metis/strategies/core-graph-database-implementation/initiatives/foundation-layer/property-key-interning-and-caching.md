---
id: property-key-interning-and-caching
level: task
title: "Property key interning and caching"
created_at: 2025-07-19T23:01:49.615886+00:00
updated_at: 2025-07-19T23:01:49.615886+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Property key interning and caching

## Parent Initiative

[[foundation-layer]]

## Objective

Implement the property key interning system that converts repeated string property keys into compact integer IDs for storage optimization. This system must include an in-memory LRU cache for frequently accessed keys to minimize database lookups during property operations.

The interning system is critical for space efficiency in typed EAV storage and query performance, as it reduces index size by 40%+ compared to direct string storage while enabling fast property key resolution.

## Acceptance Criteria

- [ ] **Key Interning**: Property keys are automatically converted to integer IDs and stored in property_keys table
- [ ] **Cache Implementation**: LRU cache with configurable size (default 1000 entries) for frequently accessed keys
- [ ] **Cache Hit Rate**: Achieve >90% cache hit rate for typical workloads with repeated property keys
- [ ] **Lookup Performance**: Key-to-ID resolution completes in <0.01ms for cached keys
- [ ] **Automatic Insertion**: New property keys are automatically inserted and assigned IDs
- [ ] **Thread Safety**: Cache operations are thread-safe for concurrent access
- [ ] **Memory Management**: Cache memory usage stays within configured bounds
- [ ] **Space Savings**: Demonstrate 40%+ reduction in property table index size vs direct string storage

## Implementation Notes

**Cache Structure:**

```c
#define KEY_CACHE_SIZE 1000

typedef struct key_cache_entry {
    char *key;
    int key_id;
    struct key_cache_entry *next;
    struct key_cache_entry *prev;
} key_cache_entry_t;

typedef struct {
    key_cache_entry_t *head;
    key_cache_entry_t *tail;
    key_cache_entry_t *entries[KEY_CACHE_SIZE];
    int current_size;
    sqlite3_stmt *lookup_stmt;
    sqlite3_stmt *insert_stmt;
    pthread_mutex_t cache_mutex;
} property_key_cache_t;
```

**Core Functions:**
```c
// Initialize cache with prepared statements
int init_property_key_cache(property_key_cache_t *cache, sqlite3 *db);

// Get key ID, inserting if not exists
int intern_property_key(property_key_cache_t *cache, const char *key);

// Reverse lookup: ID to key string
const char* resolve_property_key(property_key_cache_t *cache, int key_id);

// Cache management
void evict_lru_entry(property_key_cache_t *cache);
void move_to_head(property_key_cache_t *cache, key_cache_entry_t *entry);
```

**Lookup Algorithm:**
```c
int intern_property_key(property_key_cache_t *cache, const char *key) {
    pthread_mutex_lock(&cache->cache_mutex);
    
    // 1. Check cache first
    uint32_t hash = hash_string(key) % KEY_CACHE_SIZE;
    key_cache_entry_t *entry = cache->entries[hash];
    while (entry && strcmp(entry->key, key) != 0) {
        entry = entry->next;
    }
    
    if (entry) {
        move_to_head(cache, entry);
        pthread_mutex_unlock(&cache->cache_mutex);
        return entry->key_id;
    }
    
    // 2. Database lookup
    sqlite3_bind_text(cache->lookup_stmt, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(cache->lookup_stmt) == SQLITE_ROW) {
        int key_id = sqlite3_column_int(cache->lookup_stmt, 0);
        cache_key_entry(cache, key, key_id);
        sqlite3_reset(cache->lookup_stmt);
        pthread_mutex_unlock(&cache->cache_mutex);
        return key_id;
    }
    sqlite3_reset(cache->lookup_stmt);
    
    // 3. Insert new key
    sqlite3_bind_text(cache->insert_stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_step(cache->insert_stmt);
    int key_id = sqlite3_last_insert_rowid(sqlite3_db_handle(cache->insert_stmt));
    sqlite3_reset(cache->insert_stmt);
    
    cache_key_entry(cache, key, key_id);
    pthread_mutex_unlock(&cache->cache_mutex);
    return key_id;
}
```

**LRU Cache Management:**
- Hash table for O(1) lookup
- Doubly-linked list for O(1) LRU operations
- Thread-safe with mutex protection
- Automatic eviction when cache reaches capacity
- Memory cleanup on database close

**Performance Considerations:**
- Prepared statements for database operations
- String hashing for cache distribution
- Minimize memory allocations in hot path
- Cache warming for common property keys
- Metrics collection for hit/miss ratios

## Status Updates

*To be added during implementation*