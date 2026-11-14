#!/usr/bin/env bash
# Self-test for the dispatch harness. Uses a stub `pi` on PATH, so it costs no
# tokens and needs no provider credentials. Proves:
#   1. a normal run emits worker_started + worker_finished and exits with the
#      worker's code,
#   2. writing src/ or tests/ is allowed; writing docs/, data/ or a root
#      Markdown file is a policy violation that fails the dispatch with 78,
#   3. the model and thinking flags are fixed (no --model override),
#   4. watch.sh returns the finished event without polling (started mid-run),
#   5. the built-in timeout fires and is recorded.
#
# Usage: tools/dispatch/selftest.sh
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
workers="$root/logs/workers"
stub_dir="$(mktemp -d)"
failures=0

cleanup() {
  rm -rf "$stub_dir" "$root/src/selftest-scratch" "$root/tests/selftest-scratch"
  rm -f "$root"/docs/selftest-violation.md* \
        "$root"/data/selftest-violation.tif* \
        "$root"/assets/selftest-violation.tif* \
        "$root"/selftest-violation.md*
  rm -f "$workers"/selftest-*.log "$workers"/selftest-*.status \
        "$workers"/selftest-*.events.jsonl
}
trap cleanup EXIT

cat >"$stub_dir/pi" <<'STUB'
#!/usr/bin/env bash
# Stub worker engine. Records the args it was called with, optionally writes a
# file (relative to the repo root, which is the caller's cwd), then exits.
printf '%s\n' "$*" >"${STUB_ARGS_OUT:-/dev/null}"
if [ -n "${STUB_WRITE:-}" ]; then
  mkdir -p "$(dirname "$STUB_WRITE")"
  printf 'stub output\n' >"$STUB_WRITE"
  printf 'scratch\n' >"${STUB_WRITE}.second" 2>/dev/null || true
fi
sleep "${STUB_SLEEP:-0}"
echo "stub pi ran"
exit "${STUB_EXIT:-0}"
STUB
chmod +x "$stub_dir/pi"

brief="$stub_dir/brief.md"
printf 'Stub brief used by selftest.sh. Do nothing.\n' >"$brief"

check() {
  local label="$1" ok="$2"
  if [ "$ok" = "0" ]; then
    printf 'PASS  %s\n' "$label"
  else
    printf 'FAIL  %s\n' "$label"
    failures=$((failures + 1))
  fi
}

# --- 1. normal run -----------------------------------------------------------
args_out="$stub_dir/args.txt"
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_ARGS_OUT="$args_out" \
  "$root/tools/dispatch/worker.sh" selftest-ok "$brief" >/dev/null
check "worker exit 0 is propagated" "$?"

events="$workers/selftest-ok.events.jsonl"
grep -q '"event":"worker_started"' "$events"; check "started event written" "$?"
grep -q '"event":"worker_finished"' "$events"; check "finished event written" "$?"
grep -q '"model":"deepseek-v4.1-flash"' "$events"; check "model is deepseek-v4.1-flash" "$?"
grep -q -- '--thinking off' "$args_out"; check "thinking is off" "$?"
grep -q '"policyViolations":0' "$events"; check "no violations for a clean run" "$?"
grep -q '"timedOut":false' "$events"; check "no false timeout recorded" "$?"
grep -q 'worker=selftest-ok exit=0' "$workers/selftest-ok.status"
check "status line written" "$?"

# Failure codes propagate untouched.
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=3 \
  "$root/tools/dispatch/worker.sh" selftest-fail "$brief" >/dev/null 2>&1
check "worker exit 3 is propagated" "$([ $? -eq 3 ] && echo 0 || echo 1)"

# The model cannot be overridden.
"$root/tools/dispatch/worker.sh" --model gpt-5 selftest-model "$brief" >/dev/null 2>&1
check "--model is rejected (fixed policy)" "$([ $? -eq 64 ] && echo 0 || echo 1)"

# --- 2. worker contract ------------------------------------------------------
# Code is allowed.
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_WRITE="src/selftest-scratch/code.cpp" \
  "$root/tools/dispatch/worker.sh" selftest-code "$brief" >/dev/null 2>&1
check "writing a code path is allowed" "$?"
grep -q '"policyViolations":0' "$workers/selftest-code.events.jsonl"
check "no violation recorded for code" "$?"

# Tests are allowed.
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_WRITE="tests/selftest-scratch/test.cpp" \
  "$root/tools/dispatch/worker.sh" selftest-tests "$brief" >/dev/null 2>&1
check "writing a test path is allowed" "$?"

# Documentation is not.
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_WRITE="docs/selftest-violation.md" \
  "$root/tools/dispatch/worker.sh" selftest-docs "$brief" >/dev/null 2>&1
check "writing docs/ fails with exit 78" "$([ $? -eq 78 ] && echo 0 || echo 1)"
grep -q '"event":"worker_policy_violation"' "$workers/selftest-docs.events.jsonl"
check "violation event emitted" "$?"
grep -q 'docs/selftest-violation.md' "$workers/selftest-docs.events.jsonl"
check "violation names the path" "$?"

# Data assets are not.
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_WRITE="assets/selftest-violation.tif" \
  "$root/tools/dispatch/worker.sh" selftest-assets "$brief" >/dev/null 2>&1
check "writing assets/ fails with exit 78" "$([ $? -eq 78 ] && echo 0 || echo 1)"

PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_WRITE="data/selftest-violation.tif" \
  "$root/tools/dispatch/worker.sh" selftest-data "$brief" >/dev/null 2>&1
check "writing data/ fails with exit 78" "$([ $? -eq 78 ] && echo 0 || echo 1)"

# Root Markdown is not.
PATH="$stub_dir:$PATH" STUB_SLEEP=0 STUB_EXIT=0 STUB_WRITE="selftest-violation.md" \
  "$root/tools/dispatch/worker.sh" selftest-rootmd "$brief" >/dev/null 2>&1
check "writing a root .md fails with exit 78" "$([ $? -eq 78 ] && echo 0 || echo 1)"

# --- 3. event-driven wait ----------------------------------------------------
PATH="$stub_dir:$PATH" STUB_SLEEP=3 STUB_EXIT=0 \
  "$root/tools/dispatch/worker.sh" selftest-slow "$brief" >/dev/null 2>&1 &
worker_pid=$!
sleep 0.3
watched="$("$root/tools/dispatch/watch.sh" selftest-slow --timeout 60)"
watch_code=$?
wait "$worker_pid"
check "watch.sh returned the finished event" "$([ -n "$watched" ] && echo 0 || echo 1)"
check "watch.sh propagated exit 0" "$([ "$watch_code" -eq 0 ] && echo 0 || echo 1)"
case "$watched" in
  *'"event":"worker_finished"'*) ok=0 ;;
  *) ok=1 ;;
esac
check "watch.sh output is a finished event" "$ok"

# --- 4. timeout --------------------------------------------------------------
PATH="$stub_dir:$PATH" STUB_SLEEP=30 STUB_EXIT=0 \
  "$root/tools/dispatch/worker.sh" --timeout 1 selftest-timeout "$brief" >/dev/null 2>&1
check "timed-out worker exits 124" "$([ $? -eq 124 ] && echo 0 || echo 1)"
grep -q '"timedOut":true' "$workers/selftest-timeout.events.jsonl"
check "timedOut:true recorded" "$?"

"$root/tools/dispatch/watch.sh" selftest-timeout >/dev/null 2>&1
check "watch.sh exits 124 for a timed-out worker" "$([ $? -eq 124 ] && echo 0 || echo 1)"

# --- 5. missing worker -------------------------------------------------------
"$root/tools/dispatch/watch.sh" selftest-never-dispatched >/dev/null 2>&1
check "watch.sh exits 65 for an unknown worker" "$([ $? -eq 65 ] && echo 0 || echo 1)"

if [ "$failures" -eq 0 ]; then
  printf 'RESULT: OK\n'
  exit 0
fi

printf 'RESULT: %s FAILED\n' "$failures"
exit 1
