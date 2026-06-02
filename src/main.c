
#include "common.h"
#include "task_queue.h"
#include "thread_pool.h"
#include "listener.h"


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

	char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &conn->ip, ipbuf, sizeof(ipbuf));

	// Remove trailing newline
	size_t len = strlen(buffer);
	if (len > 0 && buffer[len-1] == '\n') {
		buffer[len-1] = '\0';
	}
	printf("[thread %lu] received from %s:%d: %s\n", tid, ipbuf, conn->fd, buffer);

	// Simulate some work by sleeping for a short time
	//usleep(100000); // Sleep for 100ms

	char response[1024];
	snprintf(response, sizeof(response), "Echo: %s", buffer);
	send(fd, response, strlen(response), 0);

	connection_close(conn);
}

int main(void) {
	int cores = sysconf(_SC_NPROCESSORS_ONLN) - 1;
	if (cores < 1) cores = 1;

	thread_pool_t* pool = thread_pool_new(cores, CONNECTIONS_REQ_MAX);
	if (!pool) {
		fprintf(stderr, "thread_pool_new failed\n");
		return 1;
	}
	
	listener_t listener = listener_new(8000);


	int epfd = epoll_create1(0);
	if (epfd < 0) {
		fprintf(stderr, "Failed to create epoll instance\n");
		thread_pool_shutdown(pool);
		listener_close(&listener);
		return 1;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = &listener;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listener.socket_fd, &ev);

	while(1){
	//for (int i = 0; i < 10; i++) {
		struct epoll_event events[MAX_EVENTS];
		int n_fds = epoll_wait(epfd, events, MAX_EVENTS, -1);
		
		if (n_fds < 0) {
			fprintf(stderr, "Failed to wait on epoll\n");
			break;
		}
		else if (n_fds == 0) {
			continue; // Timeout, no events
		}

		for (int i = 0; i < n_fds; i++) {
			// new connection
			if (events[i].data.ptr == &listener) {
				connection_t* conn = listener_accept(&listener);
				if (!conn) {
					fprintf(stderr, "Failed to accept connections.\n");
					continue;
				}
				
				// Add new connection to epoll
				struct epoll_event conn_ev;
				conn_ev.events = EPOLLIN;
				conn_ev.data.ptr = conn;
				epoll_ctl(epfd, EPOLL_CTL_ADD, conn->fd, &conn_ev);
			}
			
			// data on existing connection
			else {
				connection_t* conn = (connection_t*) events[i].data.ptr;

				// Remove the connection from epoll 
				epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);

				task_t connection_handler_task = {
					.fn = connection_handler,
					.args = conn
				};

				if (thread_pool_submit(pool, connection_handler_task) < 0) {
					// send a http 503 response to the client 
					// before closing the connection
					// reply503()
					connection_close(conn);
				}

			}
		}
	}

	thread_pool_shutdown(pool);
	listener_close(&listener);

	return 0;
}


