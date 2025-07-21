#ifndef GRAPHQLITE_INTERNAL_H
#define GRAPHQLITE_INTERNAL_H

#include <sqlite3.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Data Types
// ============================================================================

typedef enum {
    ENTITY_NODE,
    ENTITY_EDGE
} entity_type_t;

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

// ============================================================================
// Property Key Interning
// ============================================================================

#define KEY_CACHE_SIZE 1024

typedef struct {
    int key_id;
    char *key_string;
    time_t last_used;
    uint64_t usage_count;
} key_cache_entry_t;

typedef struct property_key_cache {
    key_cache_entry_t *entries[KEY_CACHE_SIZE];
    sqlite3_stmt *lookup_stmt;
    sqlite3_stmt *insert_stmt;
    pthread_mutex_t cache_mutex;
    uint64_t cache_hits;
    uint64_t cache_misses;
    size_t current_size;
} property_key_cache_t;

// ============================================================================
// Prepared Statement Management
// ============================================================================

typedef enum {
    // Node operations
    STMT_CREATE_NODE,
    STMT_DELETE_NODE,
    STMT_GET_NODE,
    STMT_NODE_EXISTS,
    
    // Edge operations
    STMT_CREATE_EDGE,
    STMT_DELETE_EDGE,
    STMT_GET_OUTGOING_EDGES,
    STMT_GET_INCOMING_EDGES,
    STMT_GET_OUTGOING_NEIGHBORS,
    STMT_GET_INCOMING_NEIGHBORS,
    STMT_GET_OUTGOING_EDGES_BY_TYPE,
    STMT_GET_INCOMING_EDGES_BY_TYPE,
    STMT_GET_OUTGOING_NEIGHBORS_BY_TYPE,
    STMT_GET_INCOMING_NEIGHBORS_BY_TYPE,
    
    // Property operations
    STMT_SET_NODE_PROP_INT,
    STMT_SET_NODE_PROP_TEXT,
    STMT_SET_NODE_PROP_REAL,
    STMT_SET_NODE_PROP_BOOL,
    STMT_GET_NODE_PROP_INT,
    STMT_GET_NODE_PROP_TEXT,
    STMT_GET_NODE_PROP_REAL,
    STMT_GET_NODE_PROP_BOOL,
    STMT_DEL_NODE_PROP_INT,
    STMT_DEL_NODE_PROP_TEXT,
    STMT_DEL_NODE_PROP_REAL,
    STMT_DEL_NODE_PROP_BOOL,
    
    // Edge property operations
    STMT_SET_EDGE_PROP_INT,
    STMT_SET_EDGE_PROP_TEXT,
    STMT_SET_EDGE_PROP_REAL,
    STMT_SET_EDGE_PROP_BOOL,
    STMT_GET_EDGE_PROP_INT,
    STMT_GET_EDGE_PROP_TEXT,
    STMT_GET_EDGE_PROP_REAL,
    STMT_GET_EDGE_PROP_BOOL,
    STMT_DEL_EDGE_PROP_INT,
    STMT_DEL_EDGE_PROP_TEXT,
    STMT_DEL_EDGE_PROP_REAL,
    STMT_DEL_EDGE_PROP_BOOL,
    
    // Label operations
    STMT_ADD_NODE_LABEL,
    STMT_REMOVE_NODE_LABEL,
    STMT_GET_NODE_LABELS,
    STMT_FIND_NODES_BY_LABEL,
    
    // Property key management
    STMT_INTERN_PROPERTY_KEY,
    STMT_LOOKUP_PROPERTY_KEY,
    
    STMT_COUNT
} statement_type_t;

typedef struct {
    sqlite3_stmt *stmt;
    char *sql;
    statement_type_t type;
    uint64_t usage_count;
    time_t last_used;
    bool is_reusable;
} prepared_statement_entry_t;

typedef struct {
    uint64_t total_executions;
    uint64_t total_execution_time_us;
    uint64_t preparation_count;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double average_execution_time_us;
} statement_stats_t;

typedef struct statement_manager {
    // Fixed statements (always prepared)
    sqlite3_stmt *fixed_statements[STMT_COUNT];
    
    // Dynamic statement cache
    prepared_statement_entry_t *dynamic_cache;
    size_t dynamic_cache_size;
    size_t dynamic_cache_capacity;
    
    // Cache management
    pthread_mutex_t cache_mutex;
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    // Performance statistics
    statement_stats_t stats[STMT_COUNT];
    
    // Configuration
    size_t max_dynamic_statements;
    time_t statement_ttl_seconds;
} statement_manager_t;

// ============================================================================
// Transaction Management
// ============================================================================

typedef enum {
    TX_STATE_NONE,
    TX_STATE_ACTIVE,
    TX_STATE_COMMITTED,
    TX_STATE_ABORTED
} transaction_state_t;

typedef struct {
    transaction_state_t state;
    int nesting_level;
    bool auto_transaction;
    char *savepoint_name;
    pthread_mutex_t tx_mutex;
} transaction_context_t;

// ============================================================================
// Mode Management
// ============================================================================

typedef enum {
    GRAPHQLITE_MODE_INTERACTIVE,
    GRAPHQLITE_MODE_BULK_IMPORT,
    GRAPHQLITE_MODE_MAINTENANCE,
    GRAPHQLITE_MODE_READONLY
} graphqlite_mode_t;

typedef struct {
    // ACID settings
    bool synchronous_mode;
    bool foreign_keys;
    bool journal_mode_wal;
    
    // Performance settings
    int cache_size;
    int page_size;
    bool temp_store_memory;
    
    // Transaction settings
    bool auto_commit;
    int lock_timeout;
    
    // Concurrency settings
    int max_connections;
    bool read_uncommitted;
} interactive_mode_config_t;

typedef struct {
    // Performance settings
    bool synchronous_off;
    bool journal_mode_memory;
    bool temp_store_memory;
    int large_cache_size;
    int large_page_size;
    
    // Indexing settings
    bool defer_foreign_keys;
    bool defer_index_updates;
    
    // Batch settings
    size_t batch_size;
    size_t memory_limit;
    
    // Import tracking
    bool integrity_check_on_complete;
    bool auto_analyze_on_complete;
} bulk_import_config_t;

typedef struct {
    // Batch buffers
    size_t nodes_in_batch;
    size_t edges_in_batch;
    size_t properties_in_batch;
    
    // Memory tracking
    size_t current_memory_usage;
    size_t memory_limit;
    
    // Transaction state
    bool transaction_active;
    size_t operations_in_transaction;
    size_t transaction_limit;
} bulk_import_state_t;

typedef struct {
    uint64_t nodes_imported;
    uint64_t edges_imported;
    uint64_t properties_imported;
    uint64_t total_import_time_us;
    uint64_t transactions_committed;
    uint64_t memory_flushes;
    double average_throughput_per_second;
} bulk_import_stats_t;

typedef struct mode_manager {
    graphqlite_mode_t current_mode;
    graphqlite_mode_t previous_mode;
    
    // Mode-specific configurations
    interactive_mode_config_t interactive_config;
    bulk_import_config_t bulk_config;
    
    // Transition state
    bool transition_in_progress;
    pthread_mutex_t mode_mutex;
    
    // Saved state for rollback
    void *saved_pragma_state;
    size_t saved_state_size;
} mode_manager_t;

// ============================================================================
// Main Database Structure
// ============================================================================

typedef struct graphqlite_db {
    // Core SQLite connection
    sqlite3 *sqlite_db;
    char *db_path;
    
    // Component managers
    property_key_cache_t *key_cache;
    statement_manager_t stmt_manager;
    transaction_context_t tx_state;
    mode_manager_t mode_manager;
    
    // Bulk import state
    bulk_import_state_t bulk_state;
    bulk_import_config_t bulk_config;
    bulk_import_stats_t bulk_stats;
    
    // Connection state
    bool is_open;
    int open_flags;
    
    // Error handling
    int last_error_code;
    char *last_error_message;
    
    // Performance tracking
    uint64_t active_operations;
    pthread_mutex_t operations_mutex;
} graphqlite_db_t;

// ============================================================================
// Function Declarations
// ============================================================================

// Database lifecycle
graphqlite_db_t* graphqlite_open(const char *path);
int graphqlite_close(graphqlite_db_t *db);
int graphqlite_create_schema(graphqlite_db_t *db);

// Property key interning
int intern_property_key(property_key_cache_t *cache, const char *key);
int lookup_property_key(property_key_cache_t *cache, const char *key);
property_key_cache_t* create_property_key_cache(sqlite3 *db);
void destroy_property_key_cache(property_key_cache_t *cache);

// Statement management
int initialize_statement_manager(graphqlite_db_t *db);
void cleanup_statement_manager(graphqlite_db_t *db);
sqlite3_stmt* get_prepared_statement(graphqlite_db_t *db, statement_type_t type);
sqlite3_stmt* get_or_prepare_dynamic_statement(graphqlite_db_t *db, const char *sql);

// Transaction management
int graphqlite_begin_transaction(graphqlite_db_t *db);
int graphqlite_commit_transaction(graphqlite_db_t *db);
int graphqlite_rollback_transaction(graphqlite_db_t *db);
bool graphqlite_in_transaction(graphqlite_db_t *db);

// Node operations
int64_t graphqlite_create_node(graphqlite_db_t *db);
int graphqlite_delete_node(graphqlite_db_t *db, int64_t node_id);
bool graphqlite_node_exists(graphqlite_db_t *db, int64_t node_id);
int graphqlite_create_nodes_batch(graphqlite_db_t *db, size_t count, int64_t *result_ids);

// Node label operations
int graphqlite_add_node_label(graphqlite_db_t *db, int64_t node_id, const char *label);
int graphqlite_remove_node_label(graphqlite_db_t *db, int64_t node_id, const char *label);
char** graphqlite_get_node_labels(graphqlite_db_t *db, int64_t node_id, size_t *count);
void graphqlite_free_labels(char **labels, size_t count);
int64_t* graphqlite_find_nodes_by_label(graphqlite_db_t *db, const char *label, size_t *count);

// Edge operations
int64_t graphqlite_create_edge(graphqlite_db_t *db, int64_t source_id, 
                              int64_t target_id, const char *type);
int graphqlite_delete_edge(graphqlite_db_t *db, int64_t edge_id);
bool graphqlite_edge_exists(graphqlite_db_t *db, int64_t edge_id);
int64_t* graphqlite_get_incoming_edges(graphqlite_db_t *db, int64_t node_id, 
                                      const char *type, size_t *count);
int64_t* graphqlite_get_outgoing_edges(graphqlite_db_t *db, int64_t node_id, 
                                      const char *type, size_t *count);
int64_t graphqlite_get_edge_source(graphqlite_db_t *db, int64_t edge_id);
int64_t graphqlite_get_edge_target(graphqlite_db_t *db, int64_t edge_id);
char* graphqlite_get_edge_type(graphqlite_db_t *db, int64_t edge_id);

// Property operations
int graphqlite_set_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value);
int graphqlite_get_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value);
int graphqlite_delete_property(graphqlite_db_t *db, entity_type_t entity_type,
                              int64_t entity_id, const char *key);
int graphqlite_set_properties(graphqlite_db_t *db, entity_type_t entity_type,
                             int64_t entity_id, property_set_t *properties);

// Mode management
int graphqlite_switch_to_interactive_mode(graphqlite_db_t *db);
int graphqlite_switch_to_bulk_import_mode(graphqlite_db_t *db);
int graphqlite_switch_to_readonly_mode(graphqlite_db_t *db);
int graphqlite_switch_to_maintenance_mode(graphqlite_db_t *db);
graphqlite_mode_t graphqlite_get_current_mode(graphqlite_db_t *db);

// Mode transition functions
int apply_interactive_mode_config(graphqlite_db_t *db, interactive_mode_config_t *config);
int apply_bulk_import_config(graphqlite_db_t *db, bulk_import_config_t *config);
int perform_mode_transition(graphqlite_db_t *db, graphqlite_mode_t target_mode);
bool graphqlite_is_mode_transition_safe(graphqlite_db_t *db, graphqlite_mode_t target_mode);
int finalize_current_mode(graphqlite_db_t *db);
int finalize_interactive_mode(graphqlite_db_t *db);
int finalize_readonly_mode(graphqlite_db_t *db);
int finalize_maintenance_mode(graphqlite_db_t *db);
int complete_bulk_import(graphqlite_db_t *db);
int perform_integrity_check(graphqlite_db_t *db);

// Bulk import functions
int bulk_insert_nodes_raw(graphqlite_db_t *db, size_t count, const char **label_arrays[], 
                         size_t label_counts[], property_set_t *property_sets[], int64_t *result_ids);
int bulk_insert_labels(graphqlite_db_t *db, size_t count, int64_t node_ids[], 
                      const char **label_arrays[], size_t label_counts[]);
int bulk_insert_properties(graphqlite_db_t *db, entity_type_t entity_type, size_t count, 
                          int64_t entity_ids[], property_set_t *property_sets[]);

// Property set management
property_set_t* create_property_set(void);
int add_property_to_set(property_set_t *set, const char *key, property_value_t *value);
void free_property_set(property_set_t *set);
void free_property_value(property_value_t *value);

// Utility functions
property_type_t infer_property_type(const char *value_str);
int validate_property_value(property_value_t *prop);

#ifdef __cplusplus
}
#endif

#endif // GRAPHQLITE_INTERNAL_H