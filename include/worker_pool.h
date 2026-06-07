#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include "listener.h"
#include <pthread.h>

typedef struct worker_pool_t worker_pool_t;

typedef struct worker_t {
    pthread_t          thread;
    int                epfd;
    int                wake_fd;
    int                id;
    worker_pool_t     *pool;
} worker_t;

typedef struct worker_pool_t {
    worker_t          *workers;
    int                count;
    volatile int       shutdown;
    int                next_worker;
    pthread_mutex_t    assign_mutex;
} worker_pool_t;

worker_pool_t *worker_pool_new(int thread_count);
int  worker_pool_submit(worker_pool_t *pool, connection_t *conn);
void worker_pool_shutdown(worker_pool_t *pool);
void worker_pool_wait(worker_pool_t *pool);

#endif
