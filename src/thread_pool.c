#include "task_queue.h"
#include "thread_pool.h"

static void *thread_pool_worker(void *arg) {
    thread_worker_t *worker = (thread_worker_t *)arg;
    thread_pool_t *pool = worker->pool;
    task_queue_t *local_tq = worker->local_tq;


    // Dequeues from own local task queue first, then global task queue if local is empty
    while (1) {
        task_t task = {0};

        // Check local queue first
        pthread_mutex_lock(&worker->local_m);
        if (!task_queue_is_empty(local_tq)) {
            task = task_queue_dequeue(local_tq);
            pthread_mutex_unlock(&worker->local_m);
            if (task.fn) task.fn(task.args);
            continue;
        }
        pthread_mutex_unlock(&worker->local_m);
        
        // Local queue is empty, check global queue
        pthread_mutex_lock(&pool->global_m);
        if(!task_queue_is_empty(pool->global_tq)){
            task = task_queue_dequeue(pool->global_tq);
            pthread_cond_signal(&pool->global_cv); // Signal any waiting submitters that space is available
            pthread_mutex_unlock(&pool->global_m);
            if (task.fn) task.fn(task.args);
            continue;
        }
        pthread_mutex_unlock(&pool->global_m);
        
        // Sleep if no tasks are available and check for shutdown signal
        pthread_mutex_lock(&worker->local_m);
        while (task_queue_is_empty(local_tq) && !pool->shutdown) {
            pthread_cond_wait(&worker->local_cv, &worker->local_m);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&worker->local_m);
            break;
        }
        pthread_mutex_unlock(&worker->local_m);
    }
    return NULL;
}

void thread_pool_worker_cleanup(thread_worker_t *worker) {
    task_queue_free(worker->local_tq);
    pthread_mutex_destroy(&worker->local_m);
    pthread_cond_destroy(&worker->local_cv);
    free(worker);
}

thread_pool_t *thread_pool_new(int thread_count, int queue_capacity) {
    // Initializes the thread pool structure 
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    
    // Initializes the global task queue and synchronization primitives
    pool->global_tq = task_queue_new(queue_capacity);
    pthread_mutex_init(&pool->global_m, NULL);
    pthread_cond_init(&pool->global_cv, NULL);

    // Initializes the shutdown flag, thread count, and allocates memory for thread IDs and worker structures
    pool->shutdown = 0;
    pool->thread_count = thread_count;
    pool->tid = malloc(sizeof(pthread_t) * thread_count);
    
    // Allocates memory for the worker structures    
    pool->worker_tid = malloc(sizeof(thread_worker_t*) * thread_count);
 
    // Creates worker threads and initializes their local task queues and synchronization primitives
    for (int i = 0; i < thread_count; i++) {
        // Initializes each worker's local task queue and starts the worker thread
        thread_worker_t *worker = malloc(sizeof(thread_worker_t));
        worker->pool = pool;
        worker->thread_id = i;
        worker->local_tq = task_queue_new(queue_capacity);
        
        // Initializes the worker's local mutex
        pthread_mutex_init(&worker->local_m, NULL);
        pthread_cond_init(&worker->local_cv, NULL);

        // Stores the worker in the pool's worker array
        pool->worker_tid[i] = worker;

        // Creates the worker thread
        if (pthread_create(&pool->tid[i], NULL, thread_pool_worker, worker) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            thread_pool_worker_cleanup(worker);
            continue;
        }
    }

    return pool;
}

// Sequential worker strategy (unbalanced):
// Fills worker 0's queue first, then 1, then 2, etc.
// This keeps early threads busy and avoids waking idle threads, saving scheduler overhead.
int thread_pool_submit(thread_pool_t *pool, task_t t) {
    // Try workers sequentially (0, 1, 2, ...) to favor keeping early threads busy
    // This naturally fills queues in order, reducing scheduler wake-ups
    int is_submitted = 0;

    for (int i = 0; i < pool->thread_count; i++) {
        thread_worker_t *target = pool->worker_tid[i];
        
        pthread_mutex_lock(&target->local_m);
        if (task_queue_is_full(target->local_tq)) {
            pthread_mutex_unlock(&target->local_m);
            continue; // Try next worker
        }
        else {
            task_queue_enqueue(target->local_tq, t);
            pthread_cond_signal(&target->local_cv); // Wake up the worker if it's waiting
            
            pthread_mutex_unlock(&target->local_m);
            is_submitted = 1;
            break; // Task submitted successfully
        }
    }
    
    if (!is_submitted) {
        // All local queues are full, fallback to global queue
        pthread_mutex_lock(&pool->global_m);
        
        while(task_queue_is_full(pool->global_tq) && !pool->shutdown) {
            pthread_cond_wait(&pool->global_cv, &pool->global_m);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->global_m);
            return -1; // Failed to submit task due to shutdown
        }

        task_queue_enqueue(pool->global_tq, t);
        pthread_cond_signal(&pool->global_cv); // Wake up a worker waiting on the global queue
        pthread_mutex_unlock(&pool->global_m);
    }

    
    return 0;
}


void thread_pool_shutdown(thread_pool_t *pool) {
    // Signals all worker threads to shutdown by setting the shutdown flag 
    // and broadcasting to all condition variables
    pthread_mutex_lock(&pool->global_m);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->global_cv);
    pthread_mutex_unlock(&pool->global_m);

    for (int i = 0; i < pool->thread_count; i++) {
        thread_worker_t *worker = pool->worker_tid[i];
        pthread_mutex_lock(&worker->local_m);
        pthread_cond_broadcast(&worker->local_cv);
        pthread_mutex_unlock(&worker->local_m);
    }

}

void thread_pool_join(thread_pool_t* pool) {
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->tid[i], NULL);
        thread_pool_worker_cleanup(pool->worker_tid[i]);
    }
    
    task_queue_free(pool->global_tq);
    pthread_mutex_destroy(&pool->global_m);
    pthread_cond_destroy(&pool->global_cv);
    free(pool->tid);
    free(pool->worker_tid);
    free(pool);
}

void thread_pool_cleanup(thread_pool_t *pool) {
    thread_pool_shutdown(pool);
    thread_pool_join(pool);
}