#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

// Forward declaration
static int lookup_property_key_unlocked(property_key_cache_t *cache, const char *key);

// ============================================================================
// Hash Function for Property Key Cache
// ============================================================================

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % KEY_CACHE_SIZE;
}

// ============================================================================
// Property Key Cache Implementation
// ============================================================================

property_key_cache_t* create_property_key_cache(sqlite3 *db) {
    if (!db) {
        return NULL;
    }
    
    property_key_cache_t *cache = malloc(sizeof(property_key_cache_t));
    if (!cache) {
        return NULL;
    }
    
    // Initialize cache structure
    memset(cache, 0, sizeof(property_key_cache_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&cache->cache_mutex, NULL) != 0) {
        free(cache);
        return NULL;
    }
    
    // Prepare lookup statement
    const char *lookup_sql = "SELECT id FROM property_keys WHERE key = ?";
    int rc = sqlite3_prepare_v2(db, lookup_sql, -1, &cache->lookup_stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_destroy(&cache->cache_mutex);
        free(cache);
        return NULL;
    }
    
    // Prepare insert statement
    const char *insert_sql = "INSERT OR IGNORE INTO property_keys (key) VALUES (?)";
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &cache->insert_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(cache->lookup_stmt);
        pthread_mutex_destroy(&cache->cache_mutex);
        free(cache);
        return NULL;
    }
    
    return cache;
}

void destroy_property_key_cache(property_key_cache_t *cache) {
    if (!cache) {
        return;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Free all cache entries
    for (int i = 0; i < KEY_CACHE_SIZE; i++) {
        key_cache_entry_t *entry = cache->entries[i];
        while (entry) {
            key_cache_entry_t *next = (key_cache_entry_t*)entry; // Use for linked list if needed
            free(entry->key_string);
            free(entry);
            entry = NULL; // Only one entry per slot in this implementation
        }
    }
    
    // Finalize prepared statements
    if (cache->lookup_stmt) {
        sqlite3_finalize(cache->lookup_stmt);
    }
    if (cache->insert_stmt) {
        sqlite3_finalize(cache->insert_stmt);
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    pthread_mutex_destroy(&cache->cache_mutex);
    
    free(cache);
}

int lookup_property_key(property_key_cache_t *cache, const char *key) {
    if (!cache || !key) {
        return -1;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Check cache first
    unsigned int hash = hash_string(key);
    key_cache_entry_t *entry = cache->entries[hash];
    
    if (entry && strcmp(entry->key_string, key) == 0) {
        // Cache hit
        entry->last_used = time(NULL);
        entry->usage_count++;
        cache->cache_hits++;
        
        int key_id = entry->key_id;
        pthread_mutex_unlock(&cache->cache_mutex);
        return key_id;
    }
    
    // Cache miss - lookup in database
    cache->cache_misses++;
    
    sqlite3_bind_text(cache->lookup_stmt, 1, key, -1, SQLITE_STATIC);
    
    int key_id = -1;
    int rc = sqlite3_step(cache->lookup_stmt);
    if (rc == SQLITE_ROW) {
        key_id = sqlite3_column_int(cache->lookup_stmt, 0);
        
        // Update cache
        if (entry) {
            // Replace existing entry
            free(entry->key_string);
        } else {
            // Create new entry
            entry = malloc(sizeof(key_cache_entry_t));
            if (entry) {
                cache->entries[hash] = entry;
                cache->current_size++;
            }
        }
        
        if (entry) {
            entry->key_id = key_id;
            entry->key_string = strdup(key);
            entry->last_used = time(NULL);
            entry->usage_count = 1;
        }
    }
    
    sqlite3_reset(cache->lookup_stmt);
    pthread_mutex_unlock(&cache->cache_mutex);
    
    return key_id;
}

int intern_property_key(property_key_cache_t *cache, const char *key) {
    if (!cache || !key) {
        return -1;
    }
    
    // First try to lookup existing key
    int key_id = lookup_property_key(cache, key);
    if (key_id > 0) {
        return key_id;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Insert new key
    sqlite3_bind_text(cache->insert_stmt, 1, key, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(cache->insert_stmt);
    sqlite3_reset(cache->insert_stmt);
    
    if (rc == SQLITE_DONE) {
        // Get the inserted key ID
        key_id = lookup_property_key_unlocked(cache, key);
        
        if (key_id > 0) {
            // Update cache with new key
            unsigned int hash = hash_string(key);
            key_cache_entry_t *entry = cache->entries[hash];
            
            if (entry) {
                // Replace existing entry
                free(entry->key_string);
            } else {
                // Create new entry
                entry = malloc(sizeof(key_cache_entry_t));
                if (entry) {
                    cache->entries[hash] = entry;
                    cache->current_size++;
                }
            }
            
            if (entry) {
                entry->key_id = key_id;
                entry->key_string = strdup(key);
                entry->last_used = time(NULL);
                entry->usage_count = 1;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    
    return key_id;
}

// Internal function for use when mutex is already held
static int lookup_property_key_unlocked(property_key_cache_t *cache, const char *key) {
    sqlite3_bind_text(cache->lookup_stmt, 1, key, -1, SQLITE_STATIC);
    
    int key_id = -1;
    int rc = sqlite3_step(cache->lookup_stmt);
    if (rc == SQLITE_ROW) {
        key_id = sqlite3_column_int(cache->lookup_stmt, 0);
    }
    
    sqlite3_reset(cache->lookup_stmt);
    return key_id;
}

// ============================================================================
// Cache Statistics and Management
// ============================================================================

void get_property_key_cache_stats(property_key_cache_t *cache, 
                                 uint64_t *hits, uint64_t *misses, 
                                 size_t *size, double *hit_ratio) {
    if (!cache) {
        return;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    if (hits) *hits = cache->cache_hits;
    if (misses) *misses = cache->cache_misses;
    if (size) *size = cache->current_size;
    
    if (hit_ratio) {
        uint64_t total = cache->cache_hits + cache->cache_misses;
        *hit_ratio = total > 0 ? (double)cache->cache_hits / total : 0.0;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
}

int cleanup_property_key_cache(property_key_cache_t *cache, time_t max_age) {
    if (!cache) {
        return 0;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    time_t now = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < KEY_CACHE_SIZE; i++) {
        key_cache_entry_t *entry = cache->entries[i];
        if (entry && (now - entry->last_used) > max_age) {
            free(entry->key_string);
            free(entry);
            cache->entries[i] = NULL;
            cache->current_size--;
            cleaned++;
        }
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    
    return cleaned;
}

// ============================================================================
// Property Value Utilities
// ============================================================================

property_type_t infer_property_type(const char *value_str) {
    if (!value_str) {
        return PROP_NULL;
    }
    
    // Try boolean first (true/false)
    if (strcasecmp(value_str, "true") == 0 || strcasecmp(value_str, "false") == 0) {
        return PROP_BOOL;
    }
    
    // Try integer
    char *endptr;
    errno = 0;
    long long int_val = strtoll(value_str, &endptr, 10);
    if (*endptr == '\0' && errno != ERANGE) {
        return PROP_INT;
    }
    
    // Try real number
    errno = 0;
    double real_val = strtod(value_str, &endptr);
    if (*endptr == '\0' && errno != ERANGE) {
        return PROP_REAL;
    }
    
    // Default to text
    return PROP_TEXT;
}

int validate_property_value(property_value_t *prop) {
    if (!prop) {
        return 0;
    }
    
    switch (prop->type) {
        case PROP_INT:
            // No additional validation needed for int64_t
            return 1;
            
        case PROP_TEXT:
            if (!prop->value.text_val) {
                return 0;
            }
            // Check for reasonable string length (1MB max)
            if (strlen(prop->value.text_val) > 1024 * 1024) {
                return 0;
            }
            return 1;
            
        case PROP_REAL:
            // Check for NaN and infinity
            if (isnan(prop->value.real_val) || isinf(prop->value.real_val)) {
                return 0;
            }
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

// ============================================================================
// Property Set Management
// ============================================================================

property_set_t* create_property_set(void) {
    property_set_t *set = malloc(sizeof(property_set_t));
    if (!set) {
        return NULL;
    }
    
    set->properties = malloc(sizeof(property_pair_t) * 8); // Initial capacity of 8
    if (!set->properties) {
        free(set);
        return NULL;
    }
    
    set->count = 0;
    set->capacity = 8;
    
    return set;
}

int add_property_to_set(property_set_t *set, const char *key, property_value_t *value) {
    if (!set || !key || !value) {
        return -1;
    }
    
    // Validate property value
    if (!validate_property_value(value)) {
        return -1;
    }
    
    // Expand capacity if needed
    if (set->count >= set->capacity) {
        size_t new_capacity = set->capacity * 2;
        property_pair_t *new_properties = realloc(set->properties, 
                                                 sizeof(property_pair_t) * new_capacity);
        if (!new_properties) {
            return -1;
        }
        set->properties = new_properties;
        set->capacity = new_capacity;
    }
    
    // Add property
    property_pair_t *prop = &set->properties[set->count];
    prop->key = strdup(key);
    if (!prop->key) {
        return -1;
    }
    
    prop->value = *value;
    
    // Deep copy text values
    if (value->type == PROP_TEXT && value->value.text_val) {
        prop->value.value.text_val = strdup(value->value.text_val);
        if (!prop->value.value.text_val) {
            free(prop->key);
            return -1;
        }
    }
    
    set->count++;
    return 0;
}

void free_property_set(property_set_t *set) {
    if (!set) {
        return;
    }
    
    for (size_t i = 0; i < set->count; i++) {
        free(set->properties[i].key);
        if (set->properties[i].value.type == PROP_TEXT) {
            free(set->properties[i].value.value.text_val);
        }
    }
    
    free(set->properties);
    free(set);
}