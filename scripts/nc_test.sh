#!/usr/bin/env bash
set -uo pipefail

# nc_test.sh - looped nc tester using `echo "text" | nc localhost PORT`
# Usage: scripts/nc_test.sh [PORT] [ITERATIONS]
# Defaults: PORT=8000 ITERATIONS= (infinite)

PORT=${1:-8000}
ITERS=${2:-}

count=0
while true; do
  # 4 random base64 characters
  PAYLOAD=$(tr -dc 'A-Za-z0-9+/' < /dev/urandom | head -c4)

  ts=$(date +%T)

  # send payload using the explicit form requested: echo "text" | nc localhost PORT
  echo "$PAYLOAD" | nc localhost "$PORT" >/dev/null 2>&1
  rc=$?
  if [ "$rc" -eq 0 ]; then
    STATUS=OK
  else
    STATUS=FAIL
  fi

  echo "[$ts] localhost:$PORT -> $PAYLOAD ($STATUS)"

  # iterations support
  if [ -n "$ITERS" ]; then
    count=$((count+1))
    if [ "$count" -ge "$ITERS" ]; then
      exit 0
    fi
  fi

  # random delay between 100ms and 200ms
  ms=$((RANDOM % 101 + 100))
  sleep_time=$(awk -v ms="$ms" 'BEGIN{printf "%.3f", ms/1000}')
  sleep "$sleep_time"
done
