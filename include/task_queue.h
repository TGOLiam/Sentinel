#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "common.h"

typedef struct {
    task_t *tl;
    int     head;
    int     tail;
    int     count;
    int     capacity;
} task_queue_t;

task_queue_t *task_queue_new(int capacity);
int           task_queue_enqueue(task_queue_t *self, task_t t);
task_t        task_queue_dequeue(task_queue_t *self);
void          task_queue_free(task_queue_t *self);

#endif
