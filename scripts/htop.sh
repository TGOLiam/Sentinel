#!/usr/bin/env bash
PID=$(pgrep -x lb)
if [ -z "$PID" ]; then
  echo "lb not running"
  exit 1
fi
htop -p "$PID"
