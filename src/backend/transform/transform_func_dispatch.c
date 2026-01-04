/*
 * transform_func_dispatch.c
 *    Table-driven function dispatch for Cypher function transformations
 *
 * This replaces the 280-line if-else chain in transform_function_call()
 * with a simple table lookup.
 */

#include <string.h>
#include <strings.h>

#include "transform/transform_func_dispatch.h"
#include "transform/transform_functions.h"

/*
 * Static dispatch table mapping function names to handlers.
 * Entries are checked in order, so more specific handlers should come first.
 * All lookups are case-insensitive.
 */
static const transform_func_entry dispatch_table[] = {
    /* Entity introspection functions */
    {"type",            transform_type_function},
    {"id",              transform_id_function},
    {"labels",          transform_labels_function},
    {"properties",      transform_properties_function},
    {"keys",            transform_keys_function},
    {"graph",           transform_graph_function},

    /* Aggregate functions */
    {"count",           transform_count_function},
    {"min",             transform_aggregate_function},
    {"max",             transform_aggregate_function},
    {"avg",             transform_aggregate_function},
    {"sum",             transform_aggregate_function},
    {"collect",         transform_collect_function},

    /* String functions - simple transforms */
    {"toUpper",         transform_string_function},
    {"toLower",         transform_string_function},
    {"trim",            transform_string_function},
    {"ltrim",           transform_string_function},
    {"rtrim",           transform_string_function},
    {"size",            transform_string_function},
    {"reverse",         transform_string_function},
    {"length",          transform_length_function},

    /* String functions - multi-arg */
    {"substring",       transform_substring_function},
    {"replace",         transform_replace_function},
    {"split",           transform_split_function},
    {"left",            transform_leftright_function},
    {"right",           transform_leftright_function},

    /* Pattern matching functions */
    {"startsWith",      transform_pattern_match_function},
    {"endsWith",        transform_pattern_match_function},
    {"contains",        transform_pattern_match_function},

    /* Math functions - single arg */
    {"abs",             transform_math_function},
    {"ceil",            transform_math_function},
    {"floor",           transform_math_function},
    {"sign",            transform_math_function},
    {"sqrt",            transform_math_function},
    {"log",             transform_math_function},
    {"log10",           transform_math_function},
    {"exp",             transform_math_function},
    {"sin",             transform_math_function},
    {"cos",             transform_math_function},
    {"tan",             transform_math_function},
    {"asin",            transform_math_function},
    {"acos",            transform_math_function},
    {"atan",            transform_math_function},

    /* Math functions - special handling */
    {"round",           transform_round_function},

    /* Math functions - no args */
    {"rand",            transform_noarg_function},
    {"random",          transform_noarg_function},
    {"pi",              transform_noarg_function},
    {"e",               transform_noarg_function},

    /* Type conversion functions */
    {"coalesce",        transform_coalesce_function},
    {"toString",        transform_tostring_function},
    {"toInteger",       transform_type_conversion_function},
    {"toFloat",         transform_type_conversion_function},
    {"toBoolean",       transform_type_conversion_function},

    /* Path functions */
    {"nodes",           transform_path_nodes_function},
    {"relationships",   transform_path_relationships_function},
    {"rels",            transform_path_relationships_function},
    {"startNode",       transform_startnode_function},
    {"endNode",         transform_endnode_function},

    /* List functions */
    {"head",            transform_list_function},
    {"tail",            transform_list_function},
    {"last",            transform_list_function},
    {"range",           transform_range_function},

    /* Date/time functions */
    {"timestamp",       transform_timestamp_function},
    {"date",            transform_date_function},
    {"time",            transform_time_function},
    {"datetime",        transform_datetime_function},
    {"localdatetime",   transform_datetime_function},
    {"randomUUID",      transform_randomuuid_function},
    {"randomuuid",      transform_randomuuid_function},

    /* Graph algorithm functions - PageRank */
    {"pageRank",        transform_pagerank_function},
    {"pagerank",        transform_pagerank_function},
    {"topPageRank",     transform_top_pagerank_function},
    {"toppagerank",     transform_top_pagerank_function},
    {"personalizedPageRank", transform_personalized_pagerank_function},
    {"personalizedpagerank", transform_personalized_pagerank_function},

    /* Graph algorithm functions - Community detection */
    {"labelPropagation", transform_label_propagation_function},
    {"labelpropagation", transform_label_propagation_function},
    {"communities",     transform_label_propagation_function},
    {"communityOf",     transform_community_of_function},
    {"communityof",     transform_community_of_function},
    {"communityMembers", transform_community_members_function},
    {"communitymembers", transform_community_members_function},
    {"communityCount",  transform_community_count_function},
    {"communitycount",  transform_community_count_function},

    /* Sentinel - must be last */
    {NULL, NULL}
};

/*
 * Look up a function handler by name.
 * Case-insensitive comparison.
 */
transform_func_handler lookup_function_handler(const char *function_name)
{
    if (!function_name) {
        return NULL;
    }

    for (int i = 0; dispatch_table[i].name != NULL; i++) {
        if (strcasecmp(dispatch_table[i].name, function_name) == 0) {
            return dispatch_table[i].handler;
        }
    }

    return NULL;
}

/*
 * Get the function dispatch table for introspection/testing.
 */
const transform_func_entry *get_function_dispatch_table(void)
{
    return dispatch_table;
}

/*
 * Get count of registered functions.
 */
int get_function_count(void)
{
    int count = 0;
    while (dispatch_table[count].name != NULL) {
        count++;
    }
    return count;
}
