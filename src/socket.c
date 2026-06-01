#include "common.h"
#include "socket.h"


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

    char ipbuf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf))) {
        close(fd);
        return NULL;
    }

    connection_t *conn = malloc(sizeof(*conn));
    if (!conn) {
        close(fd);
        return NULL;
    }

    conn->fd = fd;
    conn->ip = strdup(ipbuf); // store a const char* pointer to owned memory
    if (!conn->ip) {
        close(fd);
        free(conn);
        return NULL;
    }

    return conn;
}

