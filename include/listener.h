#ifndef LISTENER_H
#define LISTENER_H

#define CONNECTIONS_REQ_MAX 100000
#define MAX_EVENTS 100000

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct worker_pool_t worker_pool_t;
typedef struct worker_t worker_t;

#define PORT 1424
typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len;
} listener_t;

typedef enum {
    STATE_READ_REQUEST,     // reading from client_fd
    STATE_CONNECT_UPSTREAM, // opening upstream_fd, connecting to backend
    STATE_FORWARD_REQUEST,  // writing client's data to upstream_fd
    STATE_READ_RESPONSE,    // reading from upstream_fd
    STATE_SEND_RESPONSE,    // writing backend's response to client_fd
    STATE_DONE              // close both fds, free conn_t
} connection_state_t;


typedef struct {
	struct in_addr ip;
 
    // Incoming client connection
    int client_fd;

    // Connection to backend server
    int upstream_fd;

    // State of the connection, used to determine what action to take when the connection is ready
    connection_state_t state;

    // Buffer for reading/writing data, can be used for both client and upstream communication
    char buffer[1024];
    size_t buffer_len;
} connection_t;

listener_t listener_new(int port);
connection_t* listener_accept(listener_t* listener);
void listener_close(listener_t* listener);
void connection_close(connection_t* conn);
void connection_handler(connection_t *conn, worker_t *worker);
void connection_submit(worker_pool_t *pool, connection_t *conn);

#endif
