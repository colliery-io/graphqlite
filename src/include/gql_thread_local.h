/*
 * gql_thread_local.h — thread-local storage qualifier.
 *
 * A handful of transform/executor helpers return pointers into static
 * scratch buffers (alias names, temporary identifiers). Making those
 * buffers thread-local keeps two connections on different threads from
 * overwriting each other's scratch text (perf review, "shared static
 * buffers in the transform layer").
 */
#ifndef GQL_THREAD_LOCAL_H
#define GQL_THREAD_LOCAL_H

#if defined(_MSC_VER)
#define GQL_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define GQL_THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define GQL_THREAD_LOCAL _Thread_local
#else
#define GQL_THREAD_LOCAL
#endif

#endif
