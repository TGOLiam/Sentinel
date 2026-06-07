#include "worker_pool.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


listener_t listener_new(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return (listener_t){-1, {0}, 0};

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int bind_result = bind(socket_fd,(struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bind_result < 0) {
        close(socket_fd);
        return (listener_t){-1, {0}, 0};
    }


    int listen_result = listen(socket_fd, CONNECTIONS_REQ_MAX);
    if (listen_result < 0) {
        close(socket_fd);
        return (listener_t){-1, {0}, 0};
    }

    socklen_t addr_len = sizeof(server_addr);

    return (listener_t){socket_fd, server_addr, addr_len};
}

// Note: This function will block until a connection is accepted
connection_t* listener_accept(listener_t* listener) {
    if (!listener || listener->socket_fd < 0) return NULL;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int incoming_fd = accept(listener->socket_fd,
                    (struct sockaddr*)&client_addr,
                    &addr_len);
    if (incoming_fd < 0) return NULL;

    connection_t* conn = malloc(sizeof(connection_t));
    if (!conn) {
        fprintf(stderr, "Failed to allocate connection\n");
        close(incoming_fd);
        return NULL;
    }

    conn->ip = client_addr.sin_addr;
    conn->client_fd = incoming_fd;
    conn->upstream_fd = -1;
    conn->state = STATE_READ_REQUEST;
    conn->buffer_len = 0;

    return conn;
}

void listener_close(listener_t* listener) {
    if (!listener || listener->socket_fd < 0) return;
    close(listener->socket_fd);
}

void connection_close(connection_t* conn) {
    if (!conn || conn->client_fd < 0) return;
    close(conn->client_fd);
    close(conn->upstream_fd);
    free(conn);
}

void connection_handler(connection_t *conn, worker_t *worker) {
	int fd = conn->client_fd;

	char buffer[1024];
	ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read <= 0) {
		if (bytes_read == 0)
			printf("[thread %lu] connection closed\n", pthread_self());
		else
			perror("recv");
		connection_close(conn);
		return;
	}

	buffer[bytes_read] = '\0';
	pthread_t tid = pthread_self();

	char ipbuf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &conn->ip, ipbuf, sizeof(ipbuf));

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len-1] == '\n') {
		buffer[len-1] = '\0';
	}
	printf("[thread %lu] received from %s:%d: %s\n", tid, ipbuf, conn->client_fd, buffer);

	char response[1024 + 6];
	snprintf(response, sizeof(response), "Echo: %s", buffer);
	send(fd, response, strlen(response), 0);

	connection_close(conn);
}

void connection_submit(worker_pool_t *pool, connection_t *conn) {
	if (worker_pool_submit(pool, conn) < 0) {
		connection_close(conn);
	}
}