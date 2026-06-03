#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "task_queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    task_queue_t    *tq;
    pthread_mutex_t  m;
    pthread_cond_t   cv;
    int              shutdown;
    pthread_t       *tid;
    int              thread_count;
} thread_pool_t;

thread_pool_t *thread_pool_new(int thread_count, int queue_capacity);
int            thread_pool_submit(thread_pool_t *pool, task_t t);
void           thread_pool_shutdown(thread_pool_t *pool);
void	       thread_pool_join(thread_pool_t* pool);

#endif
