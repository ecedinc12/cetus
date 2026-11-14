#!/usr/bin/env bash
# Block until a dispatched worker finishes, then print its finished event.
# Event-driven: it waits on the worker's event stream (tail -f uses inotify/
# kqueue), so there is no ps/grep polling and no sleep loop.
#
# Usage:
#   tools/dispatch/watch.sh <worker-name> [--timeout SECONDS]
#
# Exit code is the worker's own exit code once known (124 if it timed out);
# 65 if the worker never started, 70 if the wait itself timed out.
set -uo pipefail

watch_timeout=""

usage() {
  sed -n '2,9p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit 64
}

name=""
while [ $# -gt 0 ]; do
  case "$1" in
    --timeout) [ $# -ge 2 ] || usage; watch_timeout="$2"; shift 2 ;;
    -h|--help) usage ;;
    -*) echo "watch.sh: unknown option '$1'" >&2; usage ;;
    *) [ -n "$name" ] && usage; name="$1"; shift ;;
  esac
done

[ -n "$name" ] || usage

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
events="$root/logs/workers/${name}.events.jsonl"

if [ ! -f "$events" ]; then
  echo "watch.sh: no events for worker '$name' (has it been dispatched?)" >&2
  exit 65
fi

# Read the exit code out of a finished event without jq.
exit_code_of() {
  local line="$1" code
  code="$(printf '%s' "$line" | sed -n 's/.*"exit":\(-\{0,1\}[0-9][0-9]*\).*/\1/p')"
  printf '%s' "${code:-0}"
}

finished_line="$(grep -m1 '"event":"worker_finished"' "$events" || true)"

if [ -z "$finished_line" ]; then
  if [ -n "$watch_timeout" ]; then
    finished_line="$(timeout "$watch_timeout" tail -f -n +1 "$events" \
      | grep -m1 '"event":"worker_finished"' || true)"
    if [ -z "$finished_line" ]; then
      echo "watch.sh: timed out after ${watch_timeout}s waiting for '$name'" >&2
      exit 70
    fi
  else
    finished_line="$(tail -f -n +1 "$events" | grep -m1 '"event":"worker_finished"' || true)"
  fi
fi

printf '%s\n' "$finished_line"
code="$(exit_code_of "$finished_line")"
case "$finished_line" in
  *'"timedOut":true'*) exit 124 ;;
  *) exit "$code" ;;
esac
