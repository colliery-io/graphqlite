#ifndef GQL_DEBUG_H
#define GQL_DEBUG_H

#include <stdio.h>

/* 
 * Debug output macros following sqlite-vec's minimalist approach.
 * Only outputs when GRAPHQLITE_DEBUG is defined during compilation.
 */

#ifdef GRAPHQLITE_DEBUG
#define GQL_DEBUG(fmt, ...) printf("[GQL_DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define GQL_DEBUG(fmt, ...) ((void)0)
#endif

/* Utility macro to suppress unused parameter warnings */
#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(x) (void)(x)
#endif

#endif /* GQL_DEBUG_H */