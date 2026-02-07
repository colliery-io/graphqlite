/*
 * Executor Helper Functions
 * Common utilities used across executor modules
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "executor/executor_internal.h"
#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"

/* Helper to lookup a parameter value from JSON */
int get_param_value(const char *params_json, const char *param_name,
                    property_type *out_type, void *out_value, size_t value_size)
{
    if (!params_json || !param_name || !out_type || !out_value) {
        return -1;
    }

    /* Skip whitespace */
    const char *p = params_json;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (*p != '{') return -1;
    p++;

    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        /* Parse key */
        if (*p != '"') return -1;
        p++;
        const char *key_start = p;
        while (*p && *p != '"') p++;
        if (!*p) return -1;
        size_t key_len = p - key_start;
        p++;

        /* Check if this is our parameter */
        bool is_match = (strlen(param_name) == key_len && strncmp(param_name, key_start, key_len) == 0);

        while (*p && *p != ':') p++;
        if (!*p) return -1;
        p++;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        if (is_match) {
            /* Parse the value */
            if (*p == '"') {
                /* String value */
                p++;
                char *str_val = (char*)out_value;
                size_t i = 0;
                while (*p && *p != '"' && i < value_size - 1) {
                    if (*p == '\\' && *(p+1)) {
                        p++;
                        switch (*p) {
                            case 'n': str_val[i++] = '\n'; break;
                            case 't': str_val[i++] = '\t'; break;
                            case 'r': str_val[i++] = '\r'; break;
                            case '"': str_val[i++] = '"'; break;
                            case '\\': str_val[i++] = '\\'; break;
                            default: str_val[i++] = *p; break;
                        }
                    } else {
                        str_val[i++] = *p;
                    }
                    p++;
                }
                str_val[i] = '\0';
                *out_type = PROP_TYPE_TEXT;
                return 0;
            } else if (*p == 't') {
                *out_type = PROP_TYPE_BOOLEAN;
                *(int*)out_value = 1;
                return 0;
            } else if (*p == 'f') {
                *out_type = PROP_TYPE_BOOLEAN;
                *(int*)out_value = 0;
                return 0;
            } else if (*p == 'n') {
                return -2;  /* null - special return code */
            } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
                const char *num_start = p;
                bool is_float = false;
                if (*p == '-') p++;
                while (*p >= '0' && *p <= '9') p++;
                if (*p == '.') { is_float = true; p++; while (*p >= '0' && *p <= '9') p++; }
                if (*p == 'e' || *p == 'E') { is_float = true; p++; if (*p == '+' || *p == '-') p++; while (*p >= '0' && *p <= '9') p++; }

                if (is_float) {
                    *out_type = PROP_TYPE_REAL;
                    *(double*)out_value = strtod(num_start, NULL);
                } else {
                    *out_type = PROP_TYPE_INTEGER;
                    *(int64_t*)out_value = strtoll(num_start, NULL, 10);
                }
                return 0;
            } else if (*p == '[' || *p == '{') {
                /* Array or object value - store as JSON text */
                const char *json_start = p;
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[' || *p == '{') depth++;
                    else if (*p == ']' || *p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p++;
                            p++;
                        }
                    }
                    if (*p) p++;
                }
                size_t json_len = p - json_start;
                char *json_val = (char*)out_value;
                if (json_len < value_size) {
                    memcpy(json_val, json_start, json_len);
                    json_val[json_len] = '\0';
                } else {
                    memcpy(json_val, json_start, value_size - 1);
                    json_val[value_size - 1] = '\0';
                }
                *out_type = PROP_TYPE_TEXT;
                return 0;
            }
            return -1;
        } else {
            /* Skip this value */
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
                if (*p) p++;
            } else if (*p == '[' || *p == '{') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[' || *p == '{') depth++;
                    else if (*p == ']' || *p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p++;
                            p++;
                        }
                    }
                    if (*p) p++;
                }
            } else if (*p == 't' || *p == 'f') {
                while (*p && *p != ',' && *p != '}') p++;
            } else if (*p == 'n') {
                p += 4;
            } else {
                while (*p && *p != ',' && *p != '}') p++;
            }
        }
    }

    return -1;  /* Parameter not found */
}

/* Helper to bind parameters from JSON to a prepared statement */
int bind_params_from_json(sqlite3_stmt *stmt, const char *params_json)
{
    if (!params_json || !stmt) {
        return 0;  /* No params to bind */
    }

    /* Skip whitespace */
    const char *p = params_json;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (*p != '{') {
        return -1;  /* Not a JSON object */
    }
    p++;

    while (*p) {
        /* Skip whitespace */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        if (*p == '}') break;  /* End of object */
        if (*p == ',') { p++; continue; }  /* Next key-value pair */

        /* Parse key (must be quoted string) */
        if (*p != '"') return -1;
        p++;
        const char *key_start = p;
        while (*p && *p != '"') p++;
        if (!*p) return -1;
        size_t key_len = p - key_start;
        p++;  /* Skip closing quote */

        /* Build parameter name with : prefix */
        char param_name[256];
        if (key_len >= sizeof(param_name) - 1) return -1;
        param_name[0] = ':';
        memcpy(param_name + 1, key_start, key_len);
        param_name[key_len + 1] = '\0';

        /* Skip to colon */
        while (*p && *p != ':') p++;
        if (!*p) return -1;
        p++;  /* Skip colon */

        /* Skip whitespace before value */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        /* Find parameter index */
        int idx = sqlite3_bind_parameter_index(stmt, param_name);
        if (idx == 0) {
            /* Parameter not used in query, skip value */
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
                if (*p) p++;
            } else if (*p == '[' || *p == '{') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[' || *p == '{') depth++;
                    else if (*p == ']' || *p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p++;
                            p++;
                        }
                    }
                    if (*p) p++;
                }
            } else if (*p == 't' || *p == 'f') {
                while (*p && *p != ',' && *p != '}') p++;
            } else if (*p == 'n') {
                p += 4;  /* null */
            } else {
                while (*p && *p != ',' && *p != '}') p++;
            }
            continue;
        }

        /* Parse and bind value */
        if (*p == '"') {
            /* String value */
            p++;
            const char *val_start = p;
            char *unescaped = malloc(strlen(p) + 1);
            char *out = unescaped;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    switch (*p) {
                        case 'n': *out++ = '\n'; break;
                        case 't': *out++ = '\t'; break;
                        case 'r': *out++ = '\r'; break;
                        case '\\': *out++ = '\\'; break;
                        case '"': *out++ = '"'; break;
                        default: *out++ = *p; break;
                    }
                    p++;
                } else {
                    *out++ = *p++;
                }
            }
            *out = '\0';
            if (*p) p++;  /* Skip closing quote */
            sqlite3_bind_text(stmt, idx, unescaped, -1, SQLITE_TRANSIENT);
            free(unescaped);
        } else if (*p == 't') {
            /* true */
            sqlite3_bind_int(stmt, idx, 1);
            p += 4;
        } else if (*p == 'f') {
            /* false */
            sqlite3_bind_int(stmt, idx, 0);
            p += 5;
        } else if (*p == 'n') {
            /* null */
            sqlite3_bind_null(stmt, idx);
            p += 4;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            /* Number */
            const char *num_start = p;
            bool is_float = false;
            if (*p == '-') p++;
            while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') { is_float = true; p++; while (*p >= '0' && *p <= '9') p++; }
            if (*p == 'e' || *p == 'E') { is_float = true; p++; if (*p == '+' || *p == '-') p++; while (*p >= '0' && *p <= '9') p++; }

            if (is_float) {
                double val = strtod(num_start, NULL);
                sqlite3_bind_double(stmt, idx, val);
            } else {
                long long val = strtoll(num_start, NULL, 10);
                sqlite3_bind_int64(stmt, idx, val);
            }
        } else if (*p == '[' || *p == '{') {
            /* Array or object value - bind as JSON text */
            const char *json_start = p;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '[' || *p == '{') depth++;
                else if (*p == ']' || *p == '}') depth--;
                else if (*p == '"') {
                    p++;
                    while (*p && *p != '"') {
                        if (*p == '\\' && *(p+1)) p++;
                        p++;
                    }
                }
                if (*p) p++;
            }
            size_t json_len = p - json_start;
            char *json_val = malloc(json_len + 1);
            if (!json_val) return -1;
            memcpy(json_val, json_start, json_len);
            json_val[json_len] = '\0';
            sqlite3_bind_text(stmt, idx, json_val, -1, SQLITE_TRANSIENT);
            free(json_val);
        } else {
            return -1;  /* Unknown value type */
        }
    }

    return 0;
}
