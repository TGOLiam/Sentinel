#include "task_queue.h"
#include "thread_pool.h"
#include "listener.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>

int main(void) {
	int cores = sysconf(_SC_NPROCESSORS_ONLN) - 1;
	if (cores < 1) cores = 1;

	thread_pool_t* pool = thread_pool_new(cores, CONNECTIONS_REQ_MAX);
	if (!pool) {
		fprintf(stderr, "thread_pool_new failed\n");
		return 1;
	}

	listener_t listener = listener_new(8000);
	if (listener.socket_fd < 0) {
		fprintf(stderr, "Failed to create listener\n");
		thread_pool_shutdown(pool);
		return 1;
	}

	int epfd = epoll_create1(0);
	if (epfd < 0) {
		fprintf(stderr, "Failed to create epoll instance\n");
		thread_pool_shutdown(pool);
		listener_close(&listener);
		return 1;
	}

	struct epoll_event ev= {.events = EPOLLIN, .data.ptr = &listener};
	epoll_ctl(epfd, EPOLL_CTL_ADD, listener.socket_fd, &ev);

	struct epoll_event events[MAX_EVENTS];

	while(1){
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
				struct epoll_event conn_ev = {.events = EPOLLIN, .data.ptr = conn};
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
