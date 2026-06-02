#!/usr/bin/env bash
set -uo pipefail

# nc_test.sh - simple looped netcat tester
# Usage:
#   scripts/nc_test.sh [HOST] [PORT] [ITERATIONS] [NO_DELAY]
#   scripts/nc_test.sh [PORT] [ITERATIONS] [NO_DELAY]  # backwards-compatible
#
# Arguments:
#   HOST        IPv4 address or hostname to connect to (default: localhost)
#   PORT        TCP port to connect to (default: 8000)
#   ITERATIONS  number of times to send a payload (default: infinite)
#   NO_DELAY    optional positional arg OR env var to disable random delays
#
# Examples:
#   scripts/nc_test.sh                     # run forever against localhost:8000
#   scripts/nc_test.sh 192.0.2.1 9000      # run forever against 192.0.2.1:9000
#   scripts/nc_test.sh 9000                # run forever against localhost:9000
#   scripts/nc_test.sh 192.0.2.1 9000 10   # send 10 payloads to 192.0.2.1:9000
#   NO_DELAY=1 scripts/nc_test.sh          # disable the random delay via env var

# Parse positional args. Supports either:
#  - scripts/nc_test.sh HOST PORT ITERATIONS NO_DELAY
#  - scripts/nc_test.sh PORT ITERATIONS NO_DELAY  (backwards-compatible)
if [[ "$1" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
  HOST="$1"
  PORT="${2:-8000}"
  ITERS="${3:-}"
  NO_DELAY="${4:-${NO_DELAY:-0}}"
else
  HOST="localhost"
  PORT="${1:-8000}"
  ITERS="${2:-}"
  NO_DELAY="${3:-${NO_DELAY:-0}}"
fi
# Disable random delays by setting the positional arg to '1', 'nodelay', or 'no-delay', or by exporting NO_DELAY=1 in the environment.
if [ "$NO_DELAY" = "nodelay" ] || [ "$NO_DELAY" = "no-delay" ] || [ "$NO_DELAY" = "1" ]; then
  NO_DELAY=1
else
  NO_DELAY=0
fi

count=0
# collect per-iteration latencies (milliseconds)
declare -a LATENCIES=()
while true; do
  # 4 random base64 characters
  PAYLOAD=$(tr -dc 'A-Za-z0-9+/' < /dev/urandom | head -c4)

  ts=$(date +%T)

  # measure latency in milliseconds using GNU date (+%s%3N)
  start_ms=$(date +%s%3N)
  echo "$PAYLOAD" | nc "$HOST" "$PORT" >/dev/null 2>&1
  rc=$?
  end_ms=$(date +%s%3N)
  latency=$((end_ms - start_ms))
  LATENCIES+=("$latency")

  if [ "$rc" -eq 0 ]; then
    STATUS=OK
  else
    STATUS=FAIL
  fi

  echo "[$ts] $HOST:$PORT -> $PAYLOAD ($STATUS) ${latency}ms"

  # iterations support
  if [ -n "$ITERS" ]; then
    count=$((count+1))
    if [ "$count" -ge "$ITERS" ]; then
      break
    fi
  fi

  # random delay between 100ms and 200ms (skip if NO_DELAY=1)
  if [ "${NO_DELAY:-0}" != "1" ]; then
    ms=$((RANDOM % 101 + 100))
    sleep_time=$(awk -v ms="$ms" 'BEGIN{printf "%.3f", ms/1000}')
    sleep "$sleep_time"
  fi
done

# If running with a fixed number of iterations, print latency summary
if [ -n "$ITERS" ]; then
  N=${#LATENCIES[@]}
  if [ "$N" -gt 0 ]; then
    sum=0
    min=99999999
    max=0
    for v in "${LATENCIES[@]}"; do
      sum=$((sum + v))
      if [ "$v" -lt "$min" ]; then min=$v; fi
      if [ "$v" -gt "$max" ]; then max=$v; fi
    done
    avg=$(awk -v s="$sum" -v n="$N" 'BEGIN{printf "%.2f", s/n}')
    sorted=$(printf "%s\n" "${LATENCIES[@]}" | sort -n)

    # median (p50)
    median=$(printf "%s\n" "$sorted" | awk -v n="$N" '{
      a[NR]=$1
    }
    END{
      if(n==1){print a[1];}
      else if(n%2==1) print a[(n+1)/2];
      else printf "%.0f", (a[n/2]+a[n/2+1])/2;
    }')

    p90_idx=$(( (N * 90 + 99) / 100 ))
    p90=$(printf "%s\n" "$sorted" | sed -n "${p90_idx}p")

    echo ""
    echo "Latency summary for $ITERS iterations (ms): min=${min} avg=${avg} max=${max} p50=${median} p90=${p90}"
  fi
fi
