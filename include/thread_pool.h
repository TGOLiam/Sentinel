#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "task_queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// Forward declaration to resolve circular dependency
typedef struct thread_pool_t thread_pool_t;

typedef struct {
    thread_pool_t *pool;
    int            thread_id;
    task_queue_t   *local_tq;
    pthread_mutex_t local_m;
    pthread_cond_t local_cv;
} thread_worker_t;

typedef struct thread_pool_t {
    task_queue_t    *global_tq;

    thread_worker_t  **worker_tid;

    pthread_mutex_t  global_m;
    pthread_cond_t   global_cv;

    int              shutdown;
    pthread_t       *tid;
    int              thread_count;
} thread_pool_t;



thread_pool_t *thread_pool_new(int thread_count, int queue_capacity);
int            thread_pool_submit(thread_pool_t *pool, task_t t);
void           thread_pool_shutdown(thread_pool_t *pool);
void	       thread_pool_join(thread_pool_t* pool);
void           thread_pool_worker_cleanup(thread_worker_t *worker);


#endif
