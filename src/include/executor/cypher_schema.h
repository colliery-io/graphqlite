#ifndef CYPHER_SCHEMA_H
#define CYPHER_SCHEMA_H

#include "graphqlite_sqlite.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Schema version — increment when table structure changes.
 * Stored in PRAGMA user_version for migration detection. */
#define GRAPHQLITE_SCHEMA_VERSION 1

/* Forward declarations */
typedef struct property_key_cache property_key_cache;

/* Property types for EAV storage */
typedef enum property_type {
    PROP_TYPE_INTEGER,
    PROP_TYPE_TEXT,
    PROP_TYPE_REAL,
    PROP_TYPE_BOOLEAN,
    PROP_TYPE_JSON
} property_type;

/* Type-safe container for property values.
 * Eliminates strict-aliasing violations from void* + pointer cast patterns.
 * Strings/JSON are dynamically allocated — caller must free as_str when done. */
typedef struct property_value {
    union {
        int64_t  as_int;
        double   as_real;
        int      as_bool;
    };
    char *as_str;       /* Dynamically allocated for TEXT/JSON, NULL for numeric types */
    size_t as_str_len;  /* Length of as_str (0 for numeric types) */
} property_value;

/* Initialize a property_value to safe defaults */
static inline void property_value_init(property_value *pv) {
    pv->as_int = 0;
    pv->as_str = NULL;
    pv->as_str_len = 0;
}

/* Free any dynamically allocated string in a property_value */
static inline void property_value_free(property_value *pv) {
    if (pv->as_str) {
        free(pv->as_str);
        pv->as_str = NULL;
        pv->as_str_len = 0;
    }
}

/* Schema manager - handles DDL and property key caching */
typedef struct cypher_schema_manager {
    sqlite3 *db;
    property_key_cache *key_cache;
    bool schema_initialized;

    /* Perf review F9: lazily prepared write statements, reset after every
     * use. A CREATE used to compile ~26 statements per node. They are
     * finalized by cypher_schema_release_statements(), which the executor
     * calls from its SQLITE_TRACE_CLOSE hook (so sqlite3_close() still
     * succeeds) and from cypher_schema_free_manager(). Property-table slots
     * are indexed by GQL_PROP_TABLE_* below. */
    sqlite3_stmt *ps_insert_node;
    sqlite3_stmt *ps_add_label;
    sqlite3_stmt *ps_create_edge;
    sqlite3_stmt *ps_key_lookup;
    sqlite3_stmt *ps_key_insert;
    sqlite3_stmt *ps_node_prop_ins[5];
    sqlite3_stmt *ps_node_prop_del[5];
    sqlite3_stmt *ps_edge_prop_ins[5];
} cypher_schema_manager;

/* Finalize every prepared statement held by the manager. */
void cypher_schema_release_statements(cypher_schema_manager *manager);

/* Property key cache entry */
typedef struct property_key_entry {
    int key_id;
    char *key_string;
    time_t last_used;
    int usage_count;
} property_key_entry;

/* Property key cache - based on proven archive design */
struct property_key_cache {
    property_key_entry **slots;     /* Hash table slots */
    int slot_count;                 /* Number of hash slots (typically 1024) */
    int total_entries;              /* Total cached entries */
    sqlite3_stmt *lookup_stmt;      /* Prepared statement for key lookup */
    sqlite3_stmt *insert_stmt;      /* Prepared statement for key insertion */
    
    /* Statistics */
    long cache_hits;
    long cache_misses;
    long key_insertions;
};

/* Schema manager functions */
cypher_schema_manager* cypher_schema_create_manager(sqlite3 *db);
void cypher_schema_free_manager(cypher_schema_manager *manager);

/* Schema operations */
int cypher_schema_initialize(cypher_schema_manager *manager);
int cypher_schema_create_tables(cypher_schema_manager *manager);
int cypher_schema_create_indexes(cypher_schema_manager *manager);
bool cypher_schema_is_initialized(cypher_schema_manager *manager);

/* Property key management */
int cypher_schema_get_property_key_id(cypher_schema_manager *manager, const char *key);
int cypher_schema_ensure_property_key(cypher_schema_manager *manager, const char *key);
const char* cypher_schema_get_property_key_name(cypher_schema_manager *manager, int key_id);

/* Property operations */
/* Perf review F9: entity_is_new = true skips the cleanup deletes across the
 * other typed tables (a node created by this statement has no rows there). */
int cypher_schema_set_node_property_ex(cypher_schema_manager *manager,
                                      int node_id, const char *key,
                                      property_type type, const void *value,
                                      bool entity_is_new);
int cypher_schema_set_node_property(cypher_schema_manager *manager, 
                                   int node_id, const char *key, 
                                   property_type type, const void *value);
int cypher_schema_get_node_property(cypher_schema_manager *manager,
                                   int node_id, const char *key,
                                   property_type *type, void **value);
int cypher_schema_delete_node_property(cypher_schema_manager *manager,
                                      int node_id, const char *key);

int cypher_schema_set_edge_property(cypher_schema_manager *manager,
                                   int edge_id, const char *key,
                                   property_type type, const void *value);
int cypher_schema_get_edge_property(cypher_schema_manager *manager,
                                   int edge_id, const char *key,
                                   property_type *type, void **value);
int cypher_schema_delete_edge_property(cypher_schema_manager *manager,
                                      int edge_id, const char *key);

/* Bulk property deletion (for SET n = {map} replace semantics) */
int cypher_schema_delete_all_node_properties(cypher_schema_manager *manager, int node_id);
int cypher_schema_delete_all_edge_properties(cypher_schema_manager *manager, int edge_id);

/* Node operations */
int cypher_schema_create_node(cypher_schema_manager *manager);
int cypher_schema_delete_node(cypher_schema_manager *manager, int node_id);
int cypher_schema_add_node_label(cypher_schema_manager *manager, int node_id, const char *label);
int cypher_schema_remove_node_label(cypher_schema_manager *manager, int node_id, const char *label);
bool cypher_schema_node_has_label(cypher_schema_manager *manager, int node_id, const char *label);

/* Edge operations */
int cypher_schema_create_edge(cypher_schema_manager *manager, 
                             int source_id, int target_id, const char *type);
int cypher_schema_delete_edge(cypher_schema_manager *manager, int edge_id);

/* Utility functions */
property_type cypher_schema_infer_property_type(const char *value_str);
const char* cypher_schema_property_type_name(property_type type);

/* Cache management */
property_key_cache* create_property_key_cache(sqlite3 *db, int slot_count);
void free_property_key_cache(property_key_cache *cache);
int prepare_property_key_cache_statements(property_key_cache *cache, sqlite3 *db);
void property_key_cache_stats(property_key_cache *cache, 
                             long *hits, long *misses, long *insertions);

/* DDL constants - table creation SQL */
extern const char* CYPHER_SCHEMA_DDL_NODES;
extern const char* CYPHER_SCHEMA_DDL_EDGES;
extern const char* CYPHER_SCHEMA_DDL_PROPERTY_KEYS;
extern const char* CYPHER_SCHEMA_DDL_NODE_LABELS;
extern const char* CYPHER_SCHEMA_DDL_NODE_PROPS_INT;
extern const char* CYPHER_SCHEMA_DDL_NODE_PROPS_TEXT;
extern const char* CYPHER_SCHEMA_DDL_NODE_PROPS_REAL;
extern const char* CYPHER_SCHEMA_DDL_NODE_PROPS_BOOL;
extern const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_INT;
extern const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_TEXT;
extern const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_REAL;
extern const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_BOOL;
extern const char* CYPHER_SCHEMA_DDL_NODE_PROPS_JSON;
extern const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_JSON;

/* Index creation SQL */
extern const char* CYPHER_SCHEMA_INDEX_EDGES_SOURCE;
extern const char* CYPHER_SCHEMA_INDEX_EDGES_TARGET;
extern const char* CYPHER_SCHEMA_INDEX_EDGES_TYPE;
extern const char* CYPHER_SCHEMA_INDEX_NODE_LABELS;
extern const char* CYPHER_SCHEMA_INDEX_PROPERTY_KEYS;
extern const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_INT;
extern const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_TEXT;
extern const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_REAL;
extern const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_BOOL;
extern const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_INT;
extern const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_TEXT;
extern const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_REAL;
extern const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_BOOL;
extern const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_JSON;
extern const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_JSON;

#endif /* CYPHER_SCHEMA_H */