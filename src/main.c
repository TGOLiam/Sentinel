
#include "common.h"
#include "task_queue.h"
#include "thread_pool.h"

void task(void) {
    pthread_t tid = pthread_self();
    printf("[thread %lu] executing task\n", tid);
}

int main(void) {
	int cores = sysconf(_SC_NPROCESSORS_ONLN);
   	thread_pool_t* pool = thread_pool_new(cores, 100);

	for (int i = 0; i < 100; i++)
		thread_pool_submit(pool, &task);

	sleep(5);	

	thread_pool_shutdown(pool);
	thread_pool_free(pool);
	return 0;
}


