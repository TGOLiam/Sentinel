# Project Sentinel

A small C-based threaded network worker demo (binary: build/lb).

Build

- Requires: gcc, make
- Build: `make`
- Binary: `./build/lb`

Run & Test

- Start the binary: `./build/lb`
- Quick load test: `./test.sh` (requires `openssl` and `nc`)

Architecture

- main.c — program startup and orchestration
- socket.* — network accept/read layer
- task_queue.* — queueing of incoming work
- thread_pool.* — worker threads that process queued tasks

Design notes: lightweight threaded worker model. Accept connections, enqueue work, workers handle processing.

Development

- Source: `src/`
- Headers: `include/`
- Build artifacts: `build/`

Next steps: add more tests, CI, and README details as needed.
