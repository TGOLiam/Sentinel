
#include "common.h"
#include "task_queue.h"
#include "thread_pool.h"
#include "socket.h"


void connection_handler(void* args) {
    connection_t* conn = (connection_t*) args;

	
	int fd = conn->fd;
	char buffer[1024];
	ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0) {
		perror("recv");
		return;
	} 

	buffer[bytes_read] = '\0';
	pthread_t tid = pthread_self();
	printf("[thread %lu] received from %s:%d: %s\n", tid, conn->ip, conn->fd, buffer);

	// Simulate Encryption work  
	for (volatile int i = 0; i < 100000000; i++);

	char response[1024];
	snprintf(response, sizeof(response), "Echo: %s", buffer);
	send(fd, response, strlen(response), 0);


	close(fd);
	free(conn);
}

int main(void) {
	int cores = sysconf(_SC_NPROCESSORS_ONLN) - 1;
   	thread_pool_t* pool = thread_pool_new(cores, CONNECTIONS_REQ_MAX);

	listener_t listener = listener_new();

	while(1){
		connection_t* conn = listener_accept(&listener);

		task_t task = {
			.fn = connection_handler,
			.args = conn
		};

		thread_pool_submit(pool, task);
		//usleep(100000);
	}

	thread_pool_shutdown(pool);
	return 0;
}


