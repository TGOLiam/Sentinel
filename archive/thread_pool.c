#include "task_queue.h"
#include "thread_pool.h"

static void *thread_pool_worker(void *arg) {
    thread_worker_t *worker = (thread_worker_t *)arg;
    thread_pool_t *pool = worker->pool;
    task_queue_t *local_tq = worker->local_tq;

    while(1){
        task_t t = {0};

        pthread_mutex_lock(&worker->local_m);
        if(!task_queue_is_empty(local_tq)){
            t = task_queue_dequeue(local_tq); 
            pthread_mutex_unlock(&worker->local_m);
            if (t.fn) t.fn(t.args, worker);
            continue;
        }
        pthread_mutex_unlock(&worker->local_m);

        pthread_mutex_lock(&pool->global_m);
        if(!task_queue_is_empty(pool->global_tq)){
            t = task_queue_dequeue(pool->global_tq);
            pthread_mutex_unlock(&pool->global_m);
            if (t.fn) t.fn(t.args, worker);
            continue;
        }
        pthread_mutex_unlock(&pool->global_m);

        pthread_mutex_lock(&worker->local_m);
        while (task_queue_is_empty(local_tq) && !pool->shutdown) {
            pthread_cond_wait(&worker->local_cv, &worker->local_m);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&worker->local_m);
            break;
        }
    }
}

int thread_pool_submit(thread_pool_t *pool, task_t t) {

    // Choose a worker thread to submit the task to (round-robin or random)


    return 0;
}

void thread_pool_worker_cleanup(thread_worker_t *worker) {
    task_queue_free(worker->local_tq);
    pthread_mutex_destroy(&worker->local_m);
    pthread_cond_destroy(&worker->local_cv);

    if (worker->epfd >= 0) {
        close(worker->epfd);
    }

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

        worker->epfd = -1; // Initialize epfd to an invalid value, can be set later if needed

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