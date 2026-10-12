#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdbool.h>
#include <pthread.h>


typedef void (*ThreadPoolTask)(void *ctx);

// Persistent workers that sleep on a barrier between dispatches, so no threads are created per frame
typedef struct {
    pthread_t *workers;
    int worker_count;
    pthread_barrier_t start_barrier;
    pthread_barrier_t end_barrier;
    // Written by the calling thread before start_barrier, the barrier makes them visible to the workers
    ThreadPoolTask task;
    void *ctx;
    bool quit;
} ThreadPool;

bool thread_pool_init(ThreadPool *pool, int thread_count);
void thread_pool_free(ThreadPool *pool);
void thread_pool_run(ThreadPool *pool, ThreadPoolTask task, void *ctx);

#endif /* THREAD_POOL_H */
