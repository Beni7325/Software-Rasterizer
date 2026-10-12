#include "thread_pool.h"

#include <stdlib.h>


static void *worker_main(void *arg) {

    ThreadPool *pool = arg;

    for (;;) {
        pthread_barrier_wait(&pool->start_barrier);

        if (pool->quit) {
            break;
        }

        pool->task(pool->ctx);

        pthread_barrier_wait(&pool->end_barrier);
    }

    return NULL;
}

bool thread_pool_init(ThreadPool *pool, int thread_count) {

    if (!pool || thread_count < 1) {
        return false;
    }

    int worker_count = thread_count - 1;

    pool->workers = malloc(sizeof(*pool->workers) * (worker_count > 0 ? worker_count : 1));
    if (!pool->workers) {
        return false;
    }

    pool->worker_count = worker_count;
    pool->task = NULL;
    pool->ctx = NULL;
    pool->quit = false;

    pthread_barrier_init(&pool->start_barrier, NULL, thread_count);
    pthread_barrier_init(&pool->end_barrier, NULL, thread_count);

    for (int i = 0; i < worker_count; ++i) {
        if (pthread_create(&pool->workers[i], NULL, worker_main, pool) != 0) {
            return false;
        }
    }

    return true;
}

void thread_pool_free(ThreadPool *pool) {

    if (!pool || !pool->workers) {
        return;
    }

    // Release the workers from the start barrier with quit set so they exit instead of running a task
    pool->quit = true;
    pthread_barrier_wait(&pool->start_barrier);

    for (int i = 0; i < pool->worker_count; ++i) {
        pthread_join(pool->workers[i], NULL);
    }

    pthread_barrier_destroy(&pool->start_barrier);
    pthread_barrier_destroy(&pool->end_barrier);

    free(pool->workers);
    pool->workers = NULL;
}

void thread_pool_run(ThreadPool *pool, ThreadPoolTask task, void *ctx) {

    pool->task = task;
    pool->ctx = ctx;

    pthread_barrier_wait(&pool->start_barrier);
    task(ctx);
    pthread_barrier_wait(&pool->end_barrier);
}
