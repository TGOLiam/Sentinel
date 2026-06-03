#include "listener.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


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

    int fd = accept(listener->socket_fd,
                    (struct sockaddr*)&client_addr,
                    &addr_len);
    if (fd < 0) return NULL;

    connection_t* conn = malloc(sizeof(connection_t));
    if (!conn) {
        fprintf(stderr, "Failed to allocate connection\n");
        close(fd);
        return NULL;
    }

    conn->fd = fd;
    conn->ip = client_addr.sin_addr;
    return conn;
}

void listener_close(listener_t* listener) {
    if (!listener || listener->socket_fd < 0) return;
    close(listener->socket_fd);
}

void connection_close(connection_t* conn) {
    if (!conn || conn->fd < 0) return;
    close(conn->fd);
    free(conn);
}

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
