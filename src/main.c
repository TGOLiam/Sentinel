#include "worker_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
	int cores = sysconf(_SC_NPROCESSORS_ONLN) - 1;
	if (cores < 1) cores = 1;

	listener_t listener = listener_new(8000);
	if (listener.socket_fd < 0) {
		fprintf(stderr, "Failed to create listener\n");
		return 1;
	}

	worker_pool_t *pool = worker_pool_new(cores);
	if (!pool) {
		fprintf(stderr, "Failed to create worker pool\n");
		listener_close(&listener);
		return 1;
	}

	while (1) {
		connection_t *conn = listener_accept(&listener);
		if (!conn) {
			fprintf(stderr, "Failed to accept connection\n");
			continue;
		}
		connection_submit(pool, conn);
	}

	worker_pool_shutdown(pool);
	worker_pool_wait(pool);
	listener_close(&listener);
	return 0;
}
