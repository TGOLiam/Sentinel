#include "listener.h"
#include "worker_pool.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

listener_t listener_new(int port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0)
    return (listener_t){-1, {0}, 0};

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  int bind_result =
      bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
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
connection_t *listener_accept(listener_t *listener) {
  if (!listener || listener->socket_fd < 0)
    return NULL;

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int incoming_fd =
      accept(listener->socket_fd, (struct sockaddr *)&client_addr, &addr_len);
  if (incoming_fd < 0)
    return NULL;

  connection_t *conn = malloc(sizeof(connection_t));
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

void listener_close(listener_t *listener) {
  if (!listener || listener->socket_fd < 0)
    return;
  close(listener->socket_fd);
}

void connection_close(connection_t *conn) {
  if (!conn || conn->client_fd < 0)
    return;
  close(conn->client_fd);
  close(conn->upstream_fd);
  free(conn);
}

void connection_handler(connection_t *conn, worker_t *worker) {
  switch (conn->state) {
  case STATE_READ_REQUEST: {
    int n = recv(conn->client_fd, conn->buffer, sizeof(conn->buffer) - 1, 0);

    if (n == 0) {
      printf("Connection closed.");
      conn->state = STATE_DONE;
      break;
    }

    if (n < 0) {
      perror("recv");
      conn->state = STATE_DONE;
      break;
    }

    conn->buffer[n] = '\0';
    conn->buffer_len = n;
    pthread_t tid = pthread_self();

    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &conn->ip, ipbuf, sizeof(ipbuf));

    printf("[thread %lu] received from %s:%d: %s\n", tid, ipbuf,
           conn->client_fd, conn->buffer);

    conn->state = STATE_CONNECT_UPSTREAM;
    // FALL THROUGH
  }

  case STATE_CONNECT_UPSTREAM: {
    conn->upstream_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    // pickServer(), but pick one for now
    struct sockaddr_in upstream_addr;
    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_port = htons(PORT);
    upstream_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(conn->upstream_fd, (struct sockaddr *)&upstream_addr,
                sizeof(upstream_addr)) < 0) {
      if (errno != EINPROGRESS) {
        conn->state = STATE_DONE;
        break;
      }
    }

    struct epoll_event ev = {.events = EPOLLOUT | EPOLLONESHOT,
                             .data.ptr = conn};
    epoll_ctl(worker->epfd, EPOLL_CTL_ADD, conn->upstream_fd, &ev);

    conn->state = STATE_FORWARD_REQUEST;
    break;
  }

  case STATE_FORWARD_REQUEST: {
    ssize_t n = send(conn->upstream_fd, conn->buffer, strlen(conn->buffer), 0);

    if (n <= 0) {
      conn->state = STATE_DONE;
      break;
    }

    conn->state = STATE_READ_RESPONSE;

    struct epoll_event ev = {.events = EPOLLIN | EPOLLONESHOT,
                             .data.ptr = conn};
    epoll_ctl(worker->epfd, EPOLL_CTL_MOD, conn->upstream_fd, &ev);

    break;
  }

  case STATE_READ_RESPONSE: {
    ssize_t n = recv(conn->upstream_fd, conn->buffer, sizeof(conn->buffer), 0);

    if (n <= 0) {
      conn->state = STATE_DONE;
      break;
    }

    conn->buffer[n] = '\0';
    conn->buffer_len = n;

    conn->state = STATE_SEND_RESPONSE;
    struct epoll_event ev = {.events = EPOLLOUT | EPOLLONESHOT,
                             .data.ptr = conn};
    epoll_ctl(worker->epfd, EPOLL_CTL_MOD, conn->client_fd, &ev);

    break;
  }

  case STATE_SEND_RESPONSE: {
    send(conn->client_fd, conn->buffer, conn->buffer_len, 0);

    conn->state = STATE_DONE;
    break;
  }
  default:
    break;
  }

  if (conn->state == STATE_DONE)
    connection_close(conn);
}

void connection_submit(worker_pool_t *pool, connection_t *conn) {
  if (worker_pool_submit(pool, conn) < 0) {
    connection_close(conn);
  }
}
