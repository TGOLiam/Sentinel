#include "task_queue.h"

#include <stdlib.h>
#include <stdio.h>

task_queue_t *task_queue_new(int capacity) {
    task_queue_t *tq = malloc(sizeof(task_queue_t));
    if (!tq) return NULL;

    tq->tl = malloc(capacity * sizeof(task_t));
    if (!tq->tl) {
        fprintf(stderr, "Failed to allocate task queue\n");
        free(tq);
        return NULL;
    }

    tq->head     = 0;
    tq->tail     = 0;
    tq->count    = 0;
    tq->capacity = capacity;
    return tq;
}

int task_queue_enqueue(task_queue_t *self, task_t t) {
    if (!self || !t.fn) {
        fprintf(stderr, "Invalid task or task queue\n");
        return -1;
    }
    if (self->count == self->capacity) {
        fprintf(stderr, "Task queue is full. Cannot enqueue task.\n");
        return -1;
    }

    self->tl[self->tail] = t;
    self->tail           = (self->tail + 1) % self->capacity;
    self->count++;
    return 0;
}

task_t task_queue_dequeue(task_queue_t *self) {
    if (!self || self->count == 0)
        return (task_t){0};

    task_t t   = self->tl[self->head];
    self->head = (self->head + 1) % self->capacity;
    self->count--;
    return t;
}

void task_queue_free(task_queue_t *self) {
    if (!self) return;
    free(self->tl);
    free(self);
}

int task_queue_is_full(task_queue_t *self) {
    return self && self->count == self->capacity;
}