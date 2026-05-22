
#include "common.h"
#include "task_queue.h"
#include "thread_pool.h"

void task(void) {
    pthread_t tid = pthread_self();
    printf("[thread %lu] executing task\n", tid);
}

int main(void) {
	int threads = sysconf(_SC_NPROCESSORS_ONLN);
   	thread_pool_t* pool = thread_pool_new(cores, 10);

	thread_pool_submit(

	return 0;
}


