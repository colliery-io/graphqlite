/*
 * Cypher Schema Manager
 * Implements proven EAV design from GraphQLite archive
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "executor/cypher_schema.h"
#include "parser/cypher_debug.h"

/* Hash function - djb2 algorithm */
static unsigned long hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* DDL Constants - Table Creation SQL */

const char* CYPHER_SCHEMA_DDL_NODES = 
    "CREATE TABLE IF NOT EXISTS nodes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT"
    ")";

const char* CYPHER_SCHEMA_DDL_EDGES = 
    "CREATE TABLE IF NOT EXISTS edges ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  type TEXT NOT NULL"
    ")";

const char* CYPHER_SCHEMA_DDL_PROPERTY_KEYS = 
    "CREATE TABLE IF NOT EXISTS property_keys ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  key TEXT UNIQUE NOT NULL"
    ")";

const char* CYPHER_SCHEMA_DDL_NODE_LABELS = 
    "CREATE TABLE IF NOT EXISTS node_labels ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  label TEXT NOT NULL,"
    "  PRIMARY KEY (node_id, label)"
    ")";

const char* CYPHER_SCHEMA_DDL_NODE_PROPS_INT = 
    "CREATE TABLE IF NOT EXISTS node_props_int ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL,"
    "  PRIMARY KEY (node_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_NODE_PROPS_TEXT = 
    "CREATE TABLE IF NOT EXISTS node_props_text ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value TEXT NOT NULL,"
    "  PRIMARY KEY (node_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_NODE_PROPS_REAL = 
    "CREATE TABLE IF NOT EXISTS node_props_real ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value REAL NOT NULL,"
    "  PRIMARY KEY (node_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_NODE_PROPS_BOOL = 
    "CREATE TABLE IF NOT EXISTS node_props_bool ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
    "  PRIMARY KEY (node_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_INT = 
    "CREATE TABLE IF NOT EXISTS edge_props_int ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL,"
    "  PRIMARY KEY (edge_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_TEXT = 
    "CREATE TABLE IF NOT EXISTS edge_props_text ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value TEXT NOT NULL,"
    "  PRIMARY KEY (edge_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_REAL = 
    "CREATE TABLE IF NOT EXISTS edge_props_real ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value REAL NOT NULL,"
    "  PRIMARY KEY (edge_id, key_id)"
    ")";

const char* CYPHER_SCHEMA_DDL_EDGE_PROPS_BOOL = 
    "CREATE TABLE IF NOT EXISTS edge_props_bool ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
    "  PRIMARY KEY (edge_id, key_id)"
    ")";

/* Index Creation SQL */

const char* CYPHER_SCHEMA_INDEX_EDGES_SOURCE = 
    "CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id, type)";

const char* CYPHER_SCHEMA_INDEX_EDGES_TARGET =
    "CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id, type)";

const char* CYPHER_SCHEMA_INDEX_EDGES_TYPE =
    "CREATE INDEX IF NOT EXISTS idx_edges_type ON edges(type)";

const char* CYPHER_SCHEMA_INDEX_NODE_LABELS = 
    "CREATE INDEX IF NOT EXISTS idx_node_labels_label ON node_labels(label, node_id)";

const char* CYPHER_SCHEMA_INDEX_PROPERTY_KEYS = 
    "CREATE INDEX IF NOT EXISTS idx_property_keys_key ON property_keys(key)";

const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_INT = 
    "CREATE INDEX IF NOT EXISTS idx_node_props_int_key_value ON node_props_int(key_id, value, node_id)";

const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_TEXT = 
    "CREATE INDEX IF NOT EXISTS idx_node_props_text_key_value ON node_props_text(key_id, value, node_id)";

const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_REAL = 
    "CREATE INDEX IF NOT EXISTS idx_node_props_real_key_value ON node_props_real(key_id, value, node_id)";

const char* CYPHER_SCHEMA_INDEX_NODE_PROPS_BOOL = 
    "CREATE INDEX IF NOT EXISTS idx_node_props_bool_key_value ON node_props_bool(key_id, value, node_id)";

const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_INT = 
    "CREATE INDEX IF NOT EXISTS idx_edge_props_int_key_value ON edge_props_int(key_id, value, edge_id)";

const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_TEXT = 
    "CREATE INDEX IF NOT EXISTS idx_edge_props_text_key_value ON edge_props_text(key_id, value, edge_id)";

const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_REAL = 
    "CREATE INDEX IF NOT EXISTS idx_edge_props_real_key_value ON edge_props_real(key_id, value, edge_id)";

const char* CYPHER_SCHEMA_INDEX_EDGE_PROPS_BOOL = 
    "CREATE INDEX IF NOT EXISTS idx_edge_props_bool_key_value ON edge_props_bool(key_id, value, edge_id)";

/* Property key cache implementation */

property_key_cache* create_property_key_cache(sqlite3 *db, int slot_count)
{
    UNUSED_PARAMETER(db);
    
    property_key_cache *cache = calloc(1, sizeof(property_key_cache));
    if (!cache) {
        return NULL;
    }
    
    cache->slot_count = slot_count;
    cache->slots = calloc(slot_count, sizeof(property_key_entry*));
    if (!cache->slots) {
        free(cache);
        return NULL;
    }
    
    /* Initialize statements to NULL - will be prepared when schema is initialized */
    cache->lookup_stmt = NULL;
    cache->insert_stmt = NULL;
    
    CYPHER_DEBUG("Created property key cache with %d slots", slot_count);
    
    return cache;
}

/* Prepare cache statements after schema initialization */
int prepare_property_key_cache_statements(property_key_cache *cache, sqlite3 *db)
{
    if (!cache || !db) {
        return -1;
    }
    
    /* Skip if already prepared */
    if (cache->lookup_stmt || cache->insert_stmt) {
        return 0;
    }
    
    /* Prepare lookup statement */
    const char *lookup_sql = "SELECT id FROM property_keys WHERE key = ?";
    int rc = sqlite3_prepare_v2(db, lookup_sql, -1, &cache->lookup_stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Prepare insert statement */
    const char *insert_sql = "INSERT INTO property_keys (key) VALUES (?)";
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &cache->insert_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(cache->lookup_stmt);
        cache->lookup_stmt = NULL;
        return -1;
    }
    
    return 0;
}

void free_property_key_cache(property_key_cache *cache)
{
    if (!cache) {
        return;
    }
    
    /* Free all cached entries - for now just free individual entries */
    /* TODO: Implement proper linked list structure */
    for (int i = 0; i < cache->slot_count; i++) {
        property_key_entry *entry = cache->slots[i];
        if (entry) {
            free(entry->key_string);
            free(entry);
        }
    }
    
    if (cache->lookup_stmt) {
        sqlite3_finalize(cache->lookup_stmt);
    }
    if (cache->insert_stmt) {
        sqlite3_finalize(cache->insert_stmt);
    }
    free(cache->slots);
    free(cache);
    
    CYPHER_DEBUG("Freed property key cache");
}

/* Schema manager implementation */

cypher_schema_manager* cypher_schema_create_manager(sqlite3 *db)
{
    cypher_schema_manager *manager = calloc(1, sizeof(cypher_schema_manager));
    if (!manager) {
        return NULL;
    }
    
    manager->db = db;
    manager->schema_initialized = false;
    
    /* Create property key cache with 1024 slots (proven size) */
    manager->key_cache = create_property_key_cache(db, 1024);
    if (!manager->key_cache) {
        free(manager);
        return NULL;
    }
    
    CYPHER_DEBUG("Created schema manager %p", (void*)manager);
    
    return manager;
}

void cypher_schema_free_manager(cypher_schema_manager *manager)
{
    if (!manager) {
        return;
    }
    
    CYPHER_DEBUG("Freeing schema manager %p", (void*)manager);
    
    free_property_key_cache(manager->key_cache);
    free(manager);
}

/* Execute DDL statement with error handling */
static int execute_ddl(sqlite3 *db, const char *sql, const char *description)
{
    char *error_message;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error_message);
    
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to execute %s: %s", description, error_message);
        sqlite3_free(error_message);
        return -1;
    }
    
    CYPHER_DEBUG("Successfully executed %s", description);
    return 0;
}

int cypher_schema_create_tables(cypher_schema_manager *manager)
{
    if (!manager || !manager->db) {
        return -1;
    }
    
    CYPHER_DEBUG("Creating database tables");
    
    /* Create core tables */
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_NODES, "nodes table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_EDGES, "edges table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_PROPERTY_KEYS, "property_keys table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_NODE_LABELS, "node_labels table") < 0) return -1;
    
    /* Create node property tables */
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_NODE_PROPS_INT, "node_props_int table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_NODE_PROPS_TEXT, "node_props_text table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_NODE_PROPS_REAL, "node_props_real table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_NODE_PROPS_BOOL, "node_props_bool table") < 0) return -1;
    
    /* Create edge property tables */
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_EDGE_PROPS_INT, "edge_props_int table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_EDGE_PROPS_TEXT, "edge_props_text table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_EDGE_PROPS_REAL, "edge_props_real table") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_DDL_EDGE_PROPS_BOOL, "edge_props_bool table") < 0) return -1;
    
    return 0;
}

int cypher_schema_create_indexes(cypher_schema_manager *manager)
{
    if (!manager || !manager->db) {
        return -1;
    }
    
    CYPHER_DEBUG("Creating database indexes");
    
    /* Create edge indexes */
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGES_SOURCE, "edges source index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGES_TARGET, "edges target index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGES_TYPE, "edges type index") < 0) return -1;
    
    /* Create label and property key indexes */
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_NODE_LABELS, "node labels index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_PROPERTY_KEYS, "property keys index") < 0) return -1;
    
    /* Create property-first indexes */
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_NODE_PROPS_INT, "node props int index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_NODE_PROPS_TEXT, "node props text index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_NODE_PROPS_REAL, "node props real index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_NODE_PROPS_BOOL, "node props bool index") < 0) return -1;
    
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGE_PROPS_INT, "edge props int index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGE_PROPS_TEXT, "edge props text index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGE_PROPS_REAL, "edge props real index") < 0) return -1;
    if (execute_ddl(manager->db, CYPHER_SCHEMA_INDEX_EDGE_PROPS_BOOL, "edge props bool index") < 0) return -1;
    
    return 0;
}

int cypher_schema_initialize(cypher_schema_manager *manager)
{
    if (!manager) {
        return -1;
    }
    
    CYPHER_DEBUG("Initializing schema");
    
    /* Create all tables */
    if (cypher_schema_create_tables(manager) < 0) {
        return -1;
    }
    
    /* Create all indexes */
    if (cypher_schema_create_indexes(manager) < 0) {
        return -1;
    }

    /* Run ANALYZE to update query planner statistics if needed */
    /* Check if statistics already exist to avoid expensive re-analysis */
    sqlite3_stmt *check_stmt;
    bool needs_analyze = true;
    int rc = sqlite3_prepare_v2(manager->db,
        "SELECT 1 FROM sqlite_stat1 WHERE tbl = 'edges' LIMIT 1", -1, &check_stmt, NULL);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            needs_analyze = false;
            CYPHER_DEBUG("Statistics already exist, skipping ANALYZE");
        }
        sqlite3_finalize(check_stmt);
    }

    if (needs_analyze) {
        rc = sqlite3_exec(manager->db, "ANALYZE", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("ANALYZE failed: %s", sqlite3_errmsg(manager->db));
            /* Non-fatal - continue without statistics */
        } else {
            CYPHER_DEBUG("ANALYZE completed successfully");
        }
    }

    /* Now prepare the property key cache statements */
    if (prepare_property_key_cache_statements(manager->key_cache, manager->db) < 0) {
        return -1;
    }
    
    manager->schema_initialized = true;
    CYPHER_DEBUG("Schema initialization complete");
    
    return 0;
}

bool cypher_schema_is_initialized(cypher_schema_manager *manager)
{
    return manager && manager->schema_initialized;
}

/* Property type inference */
property_type cypher_schema_infer_property_type(const char *value_str)
{
    if (!value_str || strlen(value_str) == 0) {
        return PROP_TYPE_TEXT;
    }
    
    /* Check for boolean */
    if (strcmp(value_str, "true") == 0 || strcmp(value_str, "false") == 0) {
        return PROP_TYPE_BOOLEAN;
    }
    
    /* Check for integer */
    char *endptr;
    strtol(value_str, &endptr, 10);
    if (*endptr == '\0') {
        return PROP_TYPE_INTEGER;
    }
    
    /* Check for real */
    strtod(value_str, &endptr);
    if (*endptr == '\0') {
        return PROP_TYPE_REAL;
    }
    
    /* Default to text */
    return PROP_TYPE_TEXT;
}

const char* cypher_schema_property_type_name(property_type type)
{
    switch (type) {
        case PROP_TYPE_INTEGER: return "INTEGER";
        case PROP_TYPE_TEXT:    return "TEXT";
        case PROP_TYPE_REAL:    return "REAL";
        case PROP_TYPE_BOOLEAN: return "BOOLEAN";
        default:                return "UNKNOWN";
    }
}

/* Stub implementations for property and node operations */
/* TODO: Implement these in next phase */

int cypher_schema_get_property_key_id(cypher_schema_manager *manager, const char *key)
{
    if (!manager || !manager->key_cache || !key) {
        return -1;
    }
    
    property_key_cache *cache = manager->key_cache;
    
    /* Calculate hash slot */
    unsigned long hash = hash_string(key);
    int slot = hash % cache->slot_count;
    
    /* Check cache first */
    property_key_entry *entry = cache->slots[slot];
    if (entry && entry->key_string && strcmp(entry->key_string, key) == 0) {
        /* Cache hit */
        cache->cache_hits++;
        entry->usage_count++;
        entry->last_used = time(NULL);
        CYPHER_DEBUG("Property key cache hit for '%s' -> id %d", key, entry->key_id);
        return entry->key_id;
    }
    
    /* Cache miss - query database */
    cache->cache_misses++;
    
    if (!cache->lookup_stmt) {
        CYPHER_DEBUG("Property key lookup statement not prepared");
        return -1;
    }
    
    sqlite3_bind_text(cache->lookup_stmt, 1, key, -1, SQLITE_STATIC);
    
    int key_id = -1;
    if (sqlite3_step(cache->lookup_stmt) == SQLITE_ROW) {
        key_id = sqlite3_column_int(cache->lookup_stmt, 0);
        
        /* Add to cache */
        if (entry) {
            /* Replace existing entry */
            free(entry->key_string);
        } else {
            /* Create new entry */
            entry = malloc(sizeof(property_key_entry));
            if (!entry) {
                sqlite3_reset(cache->lookup_stmt);
                return key_id;
            }
            cache->slots[slot] = entry;
        }
        
        entry->key_id = key_id;
        entry->key_string = strdup(key);
        entry->last_used = time(NULL);
        entry->usage_count = 1;
        
        CYPHER_DEBUG("Property key '%s' found in DB -> id %d", key, key_id);
    }
    
    sqlite3_reset(cache->lookup_stmt);
    return key_id;
}

int cypher_schema_ensure_property_key(cypher_schema_manager *manager, const char *key)
{
    if (!manager || !manager->key_cache || !key) {
        return -1;
    }
    
    /* First try to get existing key */
    int key_id = cypher_schema_get_property_key_id(manager, key);
    if (key_id >= 0) {
        return key_id;
    }
    
    /* Key doesn't exist - create it */
    property_key_cache *cache = manager->key_cache;
    
    if (!cache->insert_stmt) {
        CYPHER_DEBUG("Property key insert statement not prepared");
        return -1;
    }
    
    sqlite3_bind_text(cache->insert_stmt, 1, key, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(cache->insert_stmt);
    sqlite3_reset(cache->insert_stmt);
    
    if (rc != SQLITE_DONE) {
        CYPHER_DEBUG("Failed to insert property key '%s': %s", key, sqlite3_errmsg(manager->db));
        return -1;
    }
    
    /* Get the new key ID */
    key_id = (int)sqlite3_last_insert_rowid(manager->db);
    cache->key_insertions++;
    
    /* Add to cache */
    unsigned long hash = hash_string(key);
    int slot = hash % cache->slot_count;
    
    property_key_entry *entry = cache->slots[slot];
    if (entry) {
        /* Replace existing entry */
        free(entry->key_string);
    } else {
        /* Create new entry */
        entry = malloc(sizeof(property_key_entry));
        if (!entry) {
            return key_id; /* Still return the ID even if caching fails */
        }
        cache->slots[slot] = entry;
    }
    
    entry->key_id = key_id;
    entry->key_string = strdup(key);
    entry->last_used = time(NULL);
    entry->usage_count = 1;
    
    CYPHER_DEBUG("Created new property key '%s' -> id %d", key, key_id);
    
    return key_id;
}

const char* cypher_schema_get_property_key_name(cypher_schema_manager *manager, int key_id)
{
    if (!manager || !manager->key_cache || key_id < 0) {
        return NULL;
    }
    
    property_key_cache *cache = manager->key_cache;
    
    /* Search cache first */
    for (int i = 0; i < cache->slot_count; i++) {
        property_key_entry *entry = cache->slots[i];
        if (entry && entry->key_id == key_id) {
            entry->usage_count++;
            entry->last_used = time(NULL);
            cache->cache_hits++;
            return entry->key_string;
        }
    }
    
    /* Not in cache - this function doesn't populate cache for reverse lookups */
    /* since they're typically less common than forward lookups */
    cache->cache_misses++;
    
    /* For now, return NULL - could implement DB lookup if needed */
    CYPHER_DEBUG("Property key name lookup for id %d not in cache", key_id);
    return NULL;
}

int cypher_schema_create_node(cypher_schema_manager *manager)
{
    if (!manager || !manager->db) {
        return -1;
    }
    
    /* Insert into nodes table */
    const char *sql = "INSERT INTO nodes DEFAULT VALUES";
    char *error_message;
    int rc = sqlite3_exec(manager->db, sql, NULL, NULL, &error_message);
    
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to create node: %s", error_message);
        sqlite3_free(error_message);
        return -1;
    }
    
    /* Get the new node ID */
    int node_id = (int)sqlite3_last_insert_rowid(manager->db);
    CYPHER_DEBUG("Created node with id %d", node_id);
    
    return node_id;
}

int cypher_schema_add_node_label(cypher_schema_manager *manager, int node_id, const char *label)
{
    if (!manager || !manager->db || !label || node_id < 0) {
        return -1;
    }
    
    /* Insert into node_labels table */
    const char *sql = "INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(manager->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare label insert statement: %s", sqlite3_errmsg(manager->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        CYPHER_DEBUG("Failed to add label '%s' to node %d: %s", label, node_id, sqlite3_errmsg(manager->db));
        return -1;
    }
    
    CYPHER_DEBUG("Added label '%s' to node %d", label, node_id);
    return 0;
}

int cypher_schema_set_node_property(cypher_schema_manager *manager, 
                                   int node_id, const char *key, 
                                   property_type type, const void *value)
{
    if (!manager || !manager->db || !key || !value || node_id < 0) {
        return -1;
    }
    
    /* Get or create property key ID */
    int key_id = cypher_schema_ensure_property_key(manager, key);
    if (key_id < 0) {
        return -1;
    }
    
    /* Clean up the property from all other type tables to avoid COALESCE conflicts */
    const char *cleanup_tables[] = {"node_props_text", "node_props_int", "node_props_real", "node_props_bool"};
    const char *cleanup_sql = "DELETE FROM %s WHERE node_id = ? AND key_id = ?";
    
    for (int i = 0; i < 4; i++) {
        char cleanup_query[256];
        snprintf(cleanup_query, sizeof(cleanup_query), cleanup_sql, cleanup_tables[i]);
        
        sqlite3_stmt *cleanup_stmt;
        int rc = sqlite3_prepare_v2(manager->db, cleanup_query, -1, &cleanup_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(cleanup_stmt, 1, node_id);
            sqlite3_bind_int(cleanup_stmt, 2, key_id);
            sqlite3_step(cleanup_stmt);
            sqlite3_finalize(cleanup_stmt);
        }
    }
    
    /* Determine the appropriate table and SQL based on type */
    const char *table_name;
    const char *sql_template = "INSERT INTO %s (node_id, key_id, value) VALUES (?, ?, ?)";
    char sql[256];
    
    switch (type) {
        case PROP_TYPE_INTEGER:
            table_name = "node_props_int";
            break;
        case PROP_TYPE_TEXT:
            table_name = "node_props_text";
            break;
        case PROP_TYPE_REAL:
            table_name = "node_props_real";
            break;
        case PROP_TYPE_BOOLEAN:
            table_name = "node_props_bool";
            break;
        default:
            return -1;
    }
    
    snprintf(sql, sizeof(sql), sql_template, table_name);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(manager->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare property insert statement: %s", sqlite3_errmsg(manager->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, node_id);
    sqlite3_bind_int(stmt, 2, key_id);
    
    /* Bind value based on type */
    switch (type) {
        case PROP_TYPE_INTEGER:
            sqlite3_bind_int(stmt, 3, *(const int*)value);
            break;
        case PROP_TYPE_TEXT:
            sqlite3_bind_text(stmt, 3, (const char*)value, -1, SQLITE_STATIC);
            break;
        case PROP_TYPE_REAL:
            sqlite3_bind_double(stmt, 3, *(const double*)value);
            break;
        case PROP_TYPE_BOOLEAN:
            sqlite3_bind_int(stmt, 3, *(const int*)value ? 1 : 0);
            break;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        CYPHER_DEBUG("Failed to set property '%s' on node %d: %s", key, node_id, sqlite3_errmsg(manager->db));
        return -1;
    }
    
    CYPHER_DEBUG("Set property '%s' on node %d (type %s)", key, node_id, cypher_schema_property_type_name(type));
    return 0;
}

/* Edge operations */

int cypher_schema_create_edge(cypher_schema_manager *manager, 
                             int source_id, int target_id, const char *type)
{
    if (!manager || !manager->db || !type) {
        return -1;
    }
    
    if (source_id < 0 || target_id < 0) {
        CYPHER_DEBUG("Invalid node IDs for edge creation: source=%d, target=%d", source_id, target_id);
        return -1;
    }
    
    const char *sql = "INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(manager->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare edge insert statement: %s", sqlite3_errmsg(manager->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, source_id);
    sqlite3_bind_int(stmt, 2, target_id);
    sqlite3_bind_text(stmt, 3, type, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        CYPHER_DEBUG("Failed to insert edge: %s", sqlite3_errmsg(manager->db));
        return -1;
    }
    
    int edge_id = (int)sqlite3_last_insert_rowid(manager->db);
    CYPHER_DEBUG("Created edge %d: %d -[:%s]-> %d", edge_id, source_id, type, target_id);
    
    return edge_id;
}

int cypher_schema_delete_edge(cypher_schema_manager *manager, int edge_id)
{
    if (!manager || !manager->db || edge_id < 0) {
        return -1;
    }
    
    const char *sql = "DELETE FROM edges WHERE id = ?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(manager->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare edge delete statement: %s", sqlite3_errmsg(manager->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, edge_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        CYPHER_DEBUG("Failed to delete edge %d: %s", edge_id, sqlite3_errmsg(manager->db));
        return -1;
    }
    
    CYPHER_DEBUG("Deleted edge %d", edge_id);
    return 0;
}

int cypher_schema_set_edge_property(cypher_schema_manager *manager,
                                   int edge_id, const char *key,
                                   property_type type, const void *value)
{
    if (!manager || !manager->db || !key || !value || edge_id < 0) {
        return -1;
    }
    
    /* Get or create property key ID */
    int key_id = cypher_schema_ensure_property_key(manager, key);
    if (key_id < 0) {
        return -1;
    }
    
    /* Determine the appropriate table and SQL based on type */
    const char *table_name;
    const char *sql_template = "INSERT OR REPLACE INTO %s (edge_id, key_id, value) VALUES (?, ?, ?)";
    char sql[256];
    
    switch (type) {
        case PROP_TYPE_INTEGER:
            table_name = "edge_props_int";
            break;
        case PROP_TYPE_TEXT:
            table_name = "edge_props_text";
            break;
        case PROP_TYPE_REAL:
            table_name = "edge_props_real";
            break;
        case PROP_TYPE_BOOLEAN:
            table_name = "edge_props_bool";
            break;
        default:
            return -1;
    }
    
    snprintf(sql, sizeof(sql), sql_template, table_name);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(manager->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare edge property insert statement: %s", sqlite3_errmsg(manager->db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, edge_id);
    sqlite3_bind_int(stmt, 2, key_id);
    
    /* Bind value based on type */
    switch (type) {
        case PROP_TYPE_INTEGER:
            sqlite3_bind_int(stmt, 3, *(const int*)value);
            break;
        case PROP_TYPE_TEXT:
            sqlite3_bind_text(stmt, 3, (const char*)value, -1, SQLITE_STATIC);
            break;
        case PROP_TYPE_REAL:
            sqlite3_bind_double(stmt, 3, *(const double*)value);
            break;
        case PROP_TYPE_BOOLEAN:
            sqlite3_bind_int(stmt, 3, *(const int*)value ? 1 : 0);
            break;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        CYPHER_DEBUG("Failed to set property '%s' on edge %d: %s", key, edge_id, sqlite3_errmsg(manager->db));
        return -1;
    }
    
    CYPHER_DEBUG("Set property '%s' on edge %d (type %s)", key, edge_id, cypher_schema_property_type_name(type));
    return 0;
}

/* Statistics */
void property_key_cache_stats(property_key_cache *cache, 
                             long *hits, long *misses, long *insertions)
{
    if (!cache) {
        return;
    }
    
    if (hits) *hits = cache->cache_hits;
    if (misses) *misses = cache->cache_misses;
    if (insertions) *insertions = cache->key_insertions;
}