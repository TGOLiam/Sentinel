#ifndef SOCKET_H
#define SOCKET_H

#define CONNECTIONS_REQ_MAX 100


#define PORT 1424
typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len;
} listener_t;

typedef struct {
	int fd;
	const char* ip;
} connection_t;

listener_t listener_new();
connection_t* listener_accept(listener_t* listener);

#endif