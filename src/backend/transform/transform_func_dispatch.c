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
    {"elementId",       transform_id_function},
    {"elementid",       transform_id_function},
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
    {"stDev",           transform_stdev_function},
    {"stdev",           transform_stdev_function},
    {"stDevP",          transform_stdev_function},
    {"stdevp",          transform_stdev_function},
    {"percentileCont",  transform_percentile_function},
    {"percentilecont",  transform_percentile_function},
    {"percentileDisc",  transform_percentile_function},
    {"percentiledisc",  transform_percentile_function},

    /* String functions - simple transforms */
    {"toUpper",         transform_string_function},
    {"toLower",         transform_string_function},
    {"trim",            transform_string_function},
    {"ltrim",           transform_string_function},
    {"rtrim",           transform_string_function},
    {"btrim",           transform_string_function},
    {"size",            transform_string_function},
    {"isEmpty",         transform_isempty_function},
    {"isempty",         transform_isempty_function},
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

    /* Math functions - two argument */
    {"atan2",           transform_atan2_function},

    /* Math functions - computed from primitives */
    {"cot",             transform_math_function},
    {"degrees",         transform_math_function},
    {"radians",         transform_math_function},
    {"haversin",        transform_math_function},
    {"isNaN",           transform_math_function},
    {"isnan",           transform_math_function},
    {"sinh",            transform_math_function},
    {"cosh",            transform_math_function},
    {"tanh",            transform_math_function},
    {"coth",            transform_math_function},

    /* Math functions - special handling */
    {"round",           transform_round_function},

    /* Math functions - no args */
    {"rand",            transform_noarg_function},
    {"random",          transform_noarg_function},
    {"pi",              transform_noarg_function},
    {"e",               transform_noarg_function},

    /* Scalar utility functions */
    {"nullIf",          transform_nullif_function},
    {"nullif",          transform_nullif_function},
    {"valueType",       transform_valuetype_function},
    {"valuetype",       transform_valuetype_function},
    {"char_length",     transform_string_function},
    {"character_length", transform_string_function},

    /* Type conversion functions */
    {"coalesce",        transform_coalesce_function},
    {"toString",        transform_tostring_function},
    {"toInteger",       transform_type_conversion_function},
    {"toFloat",         transform_type_conversion_function},
    {"toBoolean",       transform_type_conversion_function},

    /* Safe type conversion (OrNull variants) */
    {"toIntegerOrNull",  transform_type_conversion_ornull_function},
    {"tointegerornull",  transform_type_conversion_ornull_function},
    {"toFloatOrNull",    transform_type_conversion_ornull_function},
    {"tofloatornull",    transform_type_conversion_ornull_function},
    {"toBooleanOrNull",  transform_type_conversion_ornull_function},
    {"tobooleanornull",  transform_type_conversion_ornull_function},
    {"toStringOrNull",   transform_type_conversion_ornull_function},
    {"tostringornull",   transform_type_conversion_ornull_function},

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
    {"localtime",       transform_time_function},
    {"duration",        transform_duration_function},

    /* Temporal construction from epoch */
    {"datetimeFromEpoch",       transform_datetime_from_epoch_function},
    {"datetimefromepoch",       transform_datetime_from_epoch_function},
    {"datetime.fromEpoch",      transform_datetime_from_epoch_function},
    {"datetimeFromEpochMillis",  transform_datetime_from_epoch_function},
    {"datetimefromepochmillis",  transform_datetime_from_epoch_function},
    {"datetime.fromEpochMillis", transform_datetime_from_epoch_function},

    /* Temporal truncation */
    {"date.truncate",           transform_date_truncate_function},
    {"dateTruncate",            transform_date_truncate_function},
    {"datetime.truncate",       transform_date_truncate_function},
    {"datetimeTruncate",        transform_date_truncate_function},

    /* Duration utility functions */
    {"duration.between",        transform_duration_between_function},
    {"durationBetween",         transform_duration_between_function},
    {"duration.inSeconds",      transform_duration_in_function},
    {"durationInSeconds",       transform_duration_in_function},
    {"duration.inDays",         transform_duration_in_function},
    {"durationInDays",          transform_duration_in_function},
    {"duration.inMonths",       transform_duration_in_function},
    {"durationInMonths",        transform_duration_in_function},

    /* Temporal arithmetic */
    {"dateAdd",                 transform_date_add_function},
    {"dateadd",                 transform_date_add_function},
    {"dateSub",                 transform_date_add_function},
    {"datesub",                 transform_date_add_function},

    {"randomUUID",      transform_randomuuid_function},
    {"randomuuid",      transform_randomuuid_function},

    /* Spatial functions */
    {"point",                   transform_point_function},
    {"point.distance",          transform_point_distance_function},
    {"pointDistance",            transform_point_distance_function},
    {"distance",                transform_point_distance_function},
    {"point.withinBBox",        transform_point_within_bbox_function},
    {"pointWithinBBox",         transform_point_within_bbox_function},

    /* JSON functions */
    {"json_get",        transform_json_get_function},
    {"jsonGet",         transform_json_get_function},
    {"json_keys",       transform_json_keys_function},
    {"jsonKeys",        transform_json_keys_function},
    {"json_type",       transform_json_type_function},
    {"jsonType",        transform_json_type_function},

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
