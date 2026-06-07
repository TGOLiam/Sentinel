# Project Sentinel

A threaded TCP echo server in C built on a custom worker-pool architecture. Each worker owns its own epoll instance; the main thread only accepts connections and hands them off.

## Build

```
make
```

Requires: `gcc`, `make`. Output: `build/lb`.

## Run & Test

```
./build/lb                  # listens on port 8000
./scripts/nctest -n 100 -c 10    # 100 requests, 10 concurrent conns
```

The echo server prepends `Echo: ` to every line it receives. `nctest` reports min/avg/max/p50/p90 latency.

## Architecture

```
main.c                        # blocking accept loop
  │
  └── listener_accept()       # accept(2), wrap in connection_t
        │
        └── connection_submit(pool, conn)
              │
              └── worker_pool_submit()
                    │
                    └── epoll_ctl(ADD, client_fd) + write(eventfd)
                          │        │
                    worker N's epfd  └─ unblocks epoll_wait
```

- **`worker_pool_t`** — round-robins connections across N workers. Each worker has one epoll fd and one eventfd (wake_fd).
- **`worker_thread`** — `epoll_wait` loop. Distinguishes wake_fd events (`.data.ptr == NULL`) from client fd events (`.data.ptr == connection_t *`). Each client fd is registered with `EPOLLONESHOT` — fires once, then the handler closes it.
- **`connection_handler`** — reads a line, echoes it back, closes. Placeholder for the proxy state machine.
- **Synchronization** — `assign_mutex` protects round-robin counter only. Inter-thread signaling uses eventfd — the write to wake_fd is what unblocks a worker in `epoll_wait` when a new fd is added by the main thread.

## Connection lifecycle

```
accept(2)   → connection_t { client_fd, state=STATE_READ_REQUEST }
                    ↓
            epoll_ctl(ADD, client_fd) on worker N's epfd
                    ↓
            worker's epoll_wait returns client_fd event (EPOLLIN)
                    ↓
            connection_handler(conn, worker)
              → recv() → send() → close(client_fd) → free(conn)
```

## Configuration

Defined in `include/listener.h`:

| Constant | Value | Purpose |
|---|---|---|
| `PORT` | 1424 | (unused — actual port is hardcoded 8000 in main.c) |
| `CONNECTIONS_REQ_MAX` | 100000 | `listen()` backlog |
| `MAX_EVENTS` | 100000 | max events per `epoll_wait` call |

Worker count defaults to `sysconf(_SC_NPROCESSORS_ONLN) - 1` (min 1).

## Project structure

```
include/
  listener.h      — connection_t, listener_t, state machine enum, connection_handler
  worker_pool.h   — worker_pool_t, worker_t, pool API
src/
  main.c          — blocking accept loop, pool lifecycle
  listener.c      — accept, echo handler, submit
  worker_pool.c   — worker init/destroy, epoll loop, round-robin, shutdown
scripts/
  nctest          — Go-based concurrent TCP load tester
  nctest.go       — source for the above
```

## State machine (proxy scaffolding)

Defined in `connection_state_t` but only `STATE_READ_REQUEST` is wired. The remaining states (`STATE_CONNECT_UPSTREAM`, `STATE_FORWARD_REQUEST`, `STATE_READ_RESPONSE`, `STATE_SEND_RESPONSE`, `STATE_DONE`) are placeholders for the intended reverse proxy.
