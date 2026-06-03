#ifndef LISTENER_H
#define LISTENER_H

#define CONNECTIONS_REQ_MAX 100
#define MAX_EVENTS 1000

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#define PORT 1424
typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len;
} listener_t;

typedef struct {
	int fd;
	struct in_addr ip;
} connection_t;

listener_t listener_new(int port);
connection_t* listener_accept(listener_t* listener);
void listener_close(listener_t* listener);
void connection_close(connection_t* conn);
void connection_handler(void* args);
#endif
