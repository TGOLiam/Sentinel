#include "common.h"
#include "task_queue.h"
#include "thread_pool.h"



static void *thread_pool_worker(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->m);

        while (pool->tq->count == 0 && !pool->shutdown)
            pthread_cond_wait(&pool->cv, &pool->m);

        if (pool->shutdown && pool->tq->count == 0) {
            pthread_mutex_unlock(&pool->m);
            break;
        }

        task_t fn = task_queue_dequeue(pool->tq);
        pthread_mutex_unlock(&pool->m);

        if (fn) fn();
    }

    return NULL;
}

thread_pool_t *thread_pool_new(int thread_count, int queue_capacity) {
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    if (!pool) return NULL;

    pool->tq           = task_queue_new(queue_capacity);
    pool->shutdown     = 0;
    pool->thread_count = thread_count;
    pool->tid          = malloc(thread_count * sizeof(pthread_t));

    pthread_mutex_init(&pool->m, NULL);
    pthread_cond_init(&pool->cv, NULL);

    for (int i = 0; i < thread_count; i++)
        pthread_create(&pool->tid[i], NULL, thread_pool_worker, pool);

    return pool;
}

int thread_pool_submit(thread_pool_t *pool, task_t t) {
    pthread_mutex_lock(&pool->m);

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->m);
        return -1;
    }

    int ret = task_queue_enqueue(pool->tq, t);
    pthread_cond_signal(&pool->cv);
    pthread_mutex_unlock(&pool->m);
    return ret;
}

void thread_pool_shutdown(thread_pool_t *pool) {
    pthread_mutex_lock(&pool->m);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cv);
    pthread_mutex_unlock(&pool->m);

    for (int i = 0; i < pool->thread_count; i++)
        pthread_join(pool->tid[i], NULL);
}

void thread_pool_free(thread_pool_t *pool) {
    if (!pool) return;
    task_queue_free(pool->tq);
    pthread_mutex_destroy(&pool->m);
    pthread_cond_destroy(&pool->cv);
    free(pool->tid);
    free(pool);
}
