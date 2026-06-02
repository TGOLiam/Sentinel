#include "common.h"
#include "listener.h"


listener_t listener_new(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return (listener_t){-1, {{0}}, 0};

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    int bind_result = bind(socket_fd,(struct sockaddr*)&server_addr, sizeof(server_addr));
    

    int listen_result = listen(socket_fd, CONNECTIONS_REQ_MAX);

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