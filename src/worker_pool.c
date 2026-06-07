#include "worker_pool.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

static void *worker_thread(void *arg);

static void worker_drain_wake(worker_t *w) {
	uint64_t val;
	read(w->wake_fd, &val, sizeof(val));
}

static void worker_destroy(worker_t *w) {
	if (w->epfd >= 0) close(w->epfd);
	if (w->wake_fd >= 0) close(w->wake_fd);
}

static int worker_init(worker_t *w, worker_pool_t *pool, int id) {
	w->id = id;
	w->pool = pool;
	w->epfd = -1;
	w->wake_fd = -1;

	w->epfd = epoll_create1(0);
	if (w->epfd < 0) { perror("epoll_create1"); return -1; }

	w->wake_fd = eventfd(0, EFD_NONBLOCK);
	if (w->wake_fd < 0) { perror("eventfd"); worker_destroy(w); return -1; }

	struct epoll_event ev = {.events = EPOLLIN, .data.ptr = NULL};
	if (epoll_ctl(w->epfd, EPOLL_CTL_ADD, w->wake_fd, &ev) < 0) {
		perror("epoll_ctl"); worker_destroy(w); return -1;
	}

	if (pthread_create(&w->thread, NULL, worker_thread, w) != 0) {
		perror("pthread_create"); worker_destroy(w); return -1;
	}
	return 0;
}

static void *worker_thread(void *arg) {
	worker_t *w = (worker_t *)arg;
	worker_pool_t *pool = w->pool;
	struct epoll_event events[MAX_EVENTS];

	while (!pool->shutdown) {
		int n = epoll_wait(w->epfd, events, MAX_EVENTS, -1);
		if (n < 0) {
			if (errno == EINTR) continue;
			perror("epoll_wait");
			break;
		}
		for (int i = 0; i < n; i++) {
			connection_t *conn = events[i].data.ptr;
			if (!conn) {
				worker_drain_wake(w);
			} else {
				connection_handler(conn, w);
			}
		}
	}
	return NULL;
}

worker_pool_t *worker_pool_new(int thread_count) {
	worker_pool_t *pool = malloc(sizeof(*pool));
	if (!pool) return NULL;

	pool->workers = calloc(thread_count, sizeof(worker_t));
	if (!pool->workers) { free(pool); return NULL; }

	pool->count = 0;
	pool->shutdown = 0;
	pool->next_worker = 0;
	pthread_mutex_init(&pool->assign_mutex, NULL);

	int created = 0;
	for (int i = 0; i < thread_count; i++) {
		if (worker_init(&pool->workers[i], pool, i) < 0) break;
		created++;
	}

	if (created < thread_count) {
		pool->count = created;
		worker_pool_shutdown(pool);
		worker_pool_wait(pool);
		return NULL;
	}

	pool->count = thread_count;
	return pool;
}

int worker_pool_submit(worker_pool_t *pool, connection_t *conn) {
	pthread_mutex_lock(&pool->assign_mutex);
	worker_t *w = &pool->workers[pool->next_worker % pool->count];
	pool->next_worker++;
	pthread_mutex_unlock(&pool->assign_mutex);

	struct epoll_event ev = {
		.events = EPOLLIN | EPOLLONESHOT,
		.data.ptr = conn
	};
	if (epoll_ctl(w->epfd, EPOLL_CTL_ADD, conn->client_fd, &ev) < 0) {
		perror("epoll_ctl submit");
		return -1;
	}

	uint64_t val = 1;
	if (write(w->wake_fd, &val, sizeof(val)) < 0) {
		perror("write wake_fd");
		return -1;
	}
	return 0;
}

void worker_pool_shutdown(worker_pool_t *pool) {
	pool->shutdown = 1;
	for (int i = 0; i < pool->count; i++) {
		uint64_t val = 1;
		write(pool->workers[i].wake_fd, &val, sizeof(val));
	}
}

void worker_pool_wait(worker_pool_t *pool) {
	for (int i = 0; i < pool->count; i++) {
		pthread_join(pool->workers[i].thread, NULL);
		close(pool->workers[i].epfd);
		close(pool->workers[i].wake_fd);
	}
	pthread_mutex_destroy(&pool->assign_mutex);
	free(pool->workers);
	free(pool);
}
