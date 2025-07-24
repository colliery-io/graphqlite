#ifndef CYPHER_DEBUG_H
#define CYPHER_DEBUG_H

#include <stdio.h>

/* 
 * Debug output macros for the Cypher parser system.
 * Only outputs when GRAPHQLITE_DEBUG is defined during compilation.
 */

#ifdef GRAPHQLITE_DEBUG
#define CYPHER_DEBUG(fmt, ...) do { \
    printf("[CYPHER_DEBUG] " fmt "\n", ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
#define CYPHER_DEBUG(fmt, ...) ((void)0)
#endif

/* Utility macro to suppress unused parameter warnings */
#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(x) (void)(x)
#endif

#endif /* CYPHER_DEBUG_H */