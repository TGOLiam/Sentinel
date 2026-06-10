# Project Sentinel — AGENTS.md

Learning-first project. Do not write code or modify files unless the user says **"Eureka"**.

## Build & run

```sh
make                          # gcc -Wall -Wextra -g -std=c11 -I./include
make run                      # ./build/lb (port 8000, hardcoded)
make clean                    # rm -rf build/
```

Test: `./scripts/nctest -n 100 -c 10` (prebuilt Go binary) or `go run scripts/nctest.go -n 100 -c 10`.
Debug workers: `./scripts/htop.sh` (attaches htop to the `lb` PID).

## Architecture (per-worker epoll reactor)

```
main.c                         blocking accept loop
  └─ listener_accept()         accept(2) → connection_t
       └─ connection_submit()  worker_pool_submit()
              ├─ epoll_ctl(ADD, client_fd, EPOLLIN | EPOLLONESHOT)
              └─ write(wake_fd)  → unblocks target worker's epoll_wait
```

- Each `worker_t` owns its own `epfd`. No shared epoll.
- `EPOLLONESHOT` — fires once, handler closes the fd. Re-arm via `EPOLL_CTL_MOD`.
- `epoll_event.data.ptr == NULL` = wake_fd event; otherwise it's `connection_t*`.
- Inter-thread signaling via `eventfd` (write to wake, read to drain in `worker_drain_wake`).
- Synchronization: `assign_mutex` protects round-robin counter only.
- Worker count: `sysconf(_SC_NPROCESSORS_ONLN) - 1` (min 1).

## Source layout

| File | Role |
|---|---|
| `include/listener.h` | `connection_t`, `connection_state_t` (6-state enum), `listener_t`, decls |
| `include/worker_pool.h` | `worker_t` (epfd, wake_fd, id), `worker_pool_t`, pool API |
| `src/main.c` | Entrypoint, accept loop, pool lifecycle |
| `src/listener.c` | Accept, echo handler, `connection_close`, `connection_submit` |
| `src/worker_pool.c` | Worker init/destroy, epoll loop, round-robin, shutdown |
| `archive/` | Retired task-queue / thread-pool implementations (not compiled) |
| `scripts/nctest` / `.go` | Go concurrent TCP load tester |
| `scripts/htop.sh` | Attach htop to running `lb` process |

`compile_commands.json` at root — used by LSP tooling.

## State machine (`connection_state_t`)

Only `STATE_READ_REQUEST` is wired. Remaining states are reverse-proxy scaffolding:

```
STATE_READ_REQUEST → STATE_CONNECT_UPSTREAM → STATE_FORWARD_REQUEST
  → STATE_READ_RESPONSE → STATE_SEND_RESPONSE → STATE_DONE
```

## Known quirks

- No `epoll_ctl(EPOLL_CTL_DEL)` before `close(fd)` — potential fd-recycle / use-after-free race in `connection_close`.
- Accepted fds are **not** `O_NONBLOCK` — echo handler relies on `EPOLLONESHOT` guaranteeing readability on first dispatch, but any subsequent `recv` in the same handler can block the worker thread.
- `connection_handler` receives a `worker_t*` but ignores it (expected — the state machine will need it for `arm_fd`/`add_fd`).
- Port `8000` hardcoded in `main.c`. `PORT 1424` in `listener.h` is unused.
