/*
 * graph_algo_traversal.c
 *
 * BFS and DFS Graph Traversal Algorithms
 *
 * BFS (Breadth-First Search): Explores nodes level by level using a queue.
 * DFS (Depth-First Search): Explores as deep as possible before backtracking using a stack.
 *
 * Both return nodes with their depth and traversal order.
 *
 * Complexity: O(V + E) for both algorithms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "executor/graph_algorithms.h"

/* Queue for BFS */
typedef struct {
    int *data;
    int *depths;
    int front;
    int rear;
    int capacity;
} bfs_queue;

static bfs_queue* bfs_queue_create(int capacity) {
    bfs_queue *q = malloc(sizeof(bfs_queue));
    if (!q) return NULL;
    q->data = malloc(capacity * sizeof(int));
    q->depths = malloc(capacity * sizeof(int));
    if (!q->data || !q->depths) {
        free(q->data);
        free(q->depths);
        free(q);
        return NULL;
    }
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
    return q;
}

static void bfs_queue_free(bfs_queue *q) {
    if (q) {
        free(q->data);
        free(q->depths);
        free(q);
    }
}

static void bfs_queue_push(bfs_queue *q, int node, int depth) {
    q->data[q->rear] = node;
    q->depths[q->rear] = depth;
    q->rear++;
}

static int bfs_queue_pop(bfs_queue *q, int *depth) {
    int node = q->data[q->front];
    *depth = q->depths[q->front];
    q->front++;
    return node;
}

static int bfs_queue_empty(bfs_queue *q) {
    return q->front >= q->rear;
}

/* Stack for DFS */
typedef struct {
    int *data;
    int *depths;
    int top;
    int capacity;
} dfs_stack;

static dfs_stack* dfs_stack_create(int capacity) {
    dfs_stack *s = malloc(sizeof(dfs_stack));
    if (!s) return NULL;
    s->data = malloc(capacity * sizeof(int));
    s->depths = malloc(capacity * sizeof(int));
    if (!s->data || !s->depths) {
        free(s->data);
        free(s->depths);
        free(s);
        return NULL;
    }
    s->top = 0;
    s->capacity = capacity;
    return s;
}

static void dfs_stack_free(dfs_stack *s) {
    if (s) {
        free(s->data);
        free(s->depths);
        free(s);
    }
}

static void dfs_stack_push(dfs_stack *s, int node, int depth) {
    s->data[s->top] = node;
    s->depths[s->top] = depth;
    s->top++;
}

static int dfs_stack_pop(dfs_stack *s, int *depth) {
    s->top--;
    *depth = s->depths[s->top];
    return s->data[s->top];
}

static int dfs_stack_empty(dfs_stack *s) {
    return s->top <= 0;
}

/* BFS Implementation */
graph_algo_result* execute_bfs(sqlite3 *db, const char *start_id, int max_depth) {
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

    /* Load graph */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Find start node */
    int start = -1;
    for (int i = 0; i < n; i++) {
        if (graph->user_ids[i] && strcmp(graph->user_ids[i], start_id) == 0) {
            start = i;
            break;
        }
    }

    if (start == -1) {
        csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    /* BFS traversal */
    int *visited = calloc(n, sizeof(int));
    int *order = malloc(n * sizeof(int));
    int *depths = malloc(n * sizeof(int));
    int count = 0;

    if (!visited || !order || !depths) {
        free(visited);
        free(order);
        free(depths);
        csr_graph_free(graph);
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    bfs_queue *queue = bfs_queue_create(n);
    if (!queue) {
        free(visited);
        free(order);
        free(depths);
        csr_graph_free(graph);
        result->error_message = strdup("Queue creation failed");
        return result;
    }

    bfs_queue_push(queue, start, 0);
    visited[start] = 1;

    while (!bfs_queue_empty(queue)) {
        int depth;
        int current = bfs_queue_pop(queue, &depth);

        /* Check max depth */
        if (max_depth >= 0 && depth > max_depth) {
            continue;
        }

        order[count] = current;
        depths[count] = depth;
        count++;

        /* Add neighbors to queue */
        for (int j = graph->row_ptr[current]; j < graph->row_ptr[current + 1]; j++) {
            int neighbor = graph->col_idx[j];
            if (!visited[neighbor]) {
                visited[neighbor] = 1;
                bfs_queue_push(queue, neighbor, depth + 1);
            }
        }
    }

    /* Build JSON result */
    size_t buf_size = 256 + count * 150;
    char *json = malloc(buf_size);
    if (!json) {
        bfs_queue_free(queue);
        free(visited);
        free(order);
        free(depths);
        csr_graph_free(graph);
        result->error_message = strdup("JSON buffer allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t pos = 1;

    for (int i = 0; i < count; i++) {
        int node = order[i];
        const char *user_id = graph->user_ids[node] ? graph->user_ids[node] : "";

        if (i > 0) json[pos++] = ',';

        int written = snprintf(json + pos, buf_size - pos,
            "{\"node_id\":%d,\"user_id\":\"%s\",\"depth\":%d,\"order\":%d}",
            graph->node_ids[node], user_id, depths[i], i);

        if (written < 0 || (size_t)written >= buf_size - pos) {
            buf_size *= 2;
            char *new_json = realloc(json, buf_size);
            if (!new_json) {
                free(json);
                bfs_queue_free(queue);
                free(visited);
                free(order);
                free(depths);
                csr_graph_free(graph);
                result->error_message = strdup("JSON buffer reallocation failed");
                return result;
            }
            json = new_json;
            written = snprintf(json + pos, buf_size - pos,
                "{\"node_id\":%d,\"user_id\":\"%s\",\"depth\":%d,\"order\":%d}",
                graph->node_ids[node], user_id, depths[i], i);
        }
        pos += written;
    }

    json[pos++] = ']';
    json[pos] = '\0';

    /* Cleanup */
    bfs_queue_free(queue);
    free(visited);
    free(order);
    free(depths);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}

/* DFS Implementation */
graph_algo_result* execute_dfs(sqlite3 *db, const char *start_id, int max_depth) {
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

    /* Load graph */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Find start node */
    int start = -1;
    for (int i = 0; i < n; i++) {
        if (graph->user_ids[i] && strcmp(graph->user_ids[i], start_id) == 0) {
            start = i;
            break;
        }
    }

    if (start == -1) {
        csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    /* DFS traversal */
    int *visited = calloc(n, sizeof(int));
    int *order = malloc(n * sizeof(int));
    int *depths = malloc(n * sizeof(int));
    int count = 0;

    if (!visited || !order || !depths) {
        free(visited);
        free(order);
        free(depths);
        csr_graph_free(graph);
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    dfs_stack *stack = dfs_stack_create(n * 2);  /* Extra space for deep graphs */
    if (!stack) {
        free(visited);
        free(order);
        free(depths);
        csr_graph_free(graph);
        result->error_message = strdup("Stack creation failed");
        return result;
    }

    dfs_stack_push(stack, start, 0);

    while (!dfs_stack_empty(stack)) {
        int depth;
        int current = dfs_stack_pop(stack, &depth);

        if (visited[current]) continue;
        visited[current] = 1;

        /* Check max depth */
        if (max_depth >= 0 && depth > max_depth) {
            continue;
        }

        order[count] = current;
        depths[count] = depth;
        count++;

        /* Add neighbors to stack (reverse order for consistent traversal) */
        for (int j = graph->row_ptr[current + 1] - 1; j >= graph->row_ptr[current]; j--) {
            int neighbor = graph->col_idx[j];
            if (!visited[neighbor]) {
                dfs_stack_push(stack, neighbor, depth + 1);
            }
        }
    }

    /* Build JSON result */
    size_t buf_size = 256 + count * 150;
    char *json = malloc(buf_size);
    if (!json) {
        dfs_stack_free(stack);
        free(visited);
        free(order);
        free(depths);
        csr_graph_free(graph);
        result->error_message = strdup("JSON buffer allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t pos = 1;

    for (int i = 0; i < count; i++) {
        int node = order[i];
        const char *user_id = graph->user_ids[node] ? graph->user_ids[node] : "";

        if (i > 0) json[pos++] = ',';

        int written = snprintf(json + pos, buf_size - pos,
            "{\"node_id\":%d,\"user_id\":\"%s\",\"depth\":%d,\"order\":%d}",
            graph->node_ids[node], user_id, depths[i], i);

        if (written < 0 || (size_t)written >= buf_size - pos) {
            buf_size *= 2;
            char *new_json = realloc(json, buf_size);
            if (!new_json) {
                free(json);
                dfs_stack_free(stack);
                free(visited);
                free(order);
                free(depths);
                csr_graph_free(graph);
                result->error_message = strdup("JSON buffer reallocation failed");
                return result;
            }
            json = new_json;
            written = snprintf(json + pos, buf_size - pos,
                "{\"node_id\":%d,\"user_id\":\"%s\",\"depth\":%d,\"order\":%d}",
                graph->node_ids[node], user_id, depths[i], i);
        }
        pos += written;
    }

    json[pos++] = ']';
    json[pos] = '\0';

    /* Cleanup */
    dfs_stack_free(stack);
    free(visited);
    free(order);
    free(depths);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
