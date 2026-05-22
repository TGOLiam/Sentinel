#include "task_queue.h"
#include <stdlib.h>

task_queue_t *task_queue_new(int capacity) {
    task_queue_t *tq = malloc(sizeof(task_queue_t));
    if (!tq) return NULL;

    tq->tl = malloc(capacity * sizeof(task_t));
    if (!tq->tl) {
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
    if (!self || self->count == self->capacity)
        return -1;

    self->tl[self->tail] = t;
    self->tail           = (self->tail + 1) % self->capacity;
    self->count++;
    return 0;
}

task_t task_queue_dequeue(task_queue_t *self) {
    if (!self || self->count == 0)
        return NULL;

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
