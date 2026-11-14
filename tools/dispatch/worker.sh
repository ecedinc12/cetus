#!/usr/bin/env bash
# Dispatch one worker subprocess to WRITE CODE ONLY, and emit lifecycle events.
#
# Workers are fixed to model deepseek-v4.1-flash with thinking off. They produce
# source and tests; they never author documentation, data assets or briefs.
# That contract is enforced after the run: any change to a forbidden path fails
# the dispatch with exit 78 (see §Worker contract below).
#
# Usage:
#   tools/dispatch/worker.sh [--timeout SECONDS] <worker-name> <brief-file>
#
# Outputs under logs/workers/<name>.*:
#   .log            full stdout+stderr of the worker
#   .status         one human line: worker=… exit=… seconds=… timed_out=… log=…
#   .events.jsonl   append-only lifecycle events (see below)
#
# Events (one JSON object per line, flushed as they happen):
#   {"event":"worker_started","worker":…,"brief":…,"model":…,"pid":…,
#    "timeoutSeconds":…,"startedUtc":…}
#   {"event":"worker_policy_violation","worker":…,"paths":[…],"finishedUtc":…}
#   {"event":"worker_finished","worker":…,"exit":…,"workerExit":…,"timedOut":…,
#    "seconds":…,"policyViolations":N,"log":…,"finishedUtc":…}
#
# Workers must not poll: use tools/dispatch/watch.sh <name> to block on the
# finished event. The script itself blocks until the worker exits, so
# `worker.sh … &` plus `watch.sh` is the intended pattern.
#
# Exit code: the worker's own, or 64 bad usage, 66 brief not found, 78 the worker
# wrote outside code paths, 124 timed out.
set -uo pipefail

# Fixed by policy: workers are always deepseek-v4.1-flash with thinking disabled.
readonly WORKER_MODEL="deepseek-v4.1-flash"
readonly WORKER_THINKING="off"
timeout_seconds="${WORKER_TIMEOUT:-3600}"

# Workers write code and tests. Everything below is owner/session territory.
readonly FORBIDDEN_PREFIXES=(docs/ data/ assets/ tools/ logs/)
readonly FORBIDDEN_ROOT_SUFFIX=.md

usage() {
  sed -n '2,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit 64
}

while [ $# -gt 0 ]; do
  case "$1" in
    --timeout) [ $# -ge 2 ] || usage; timeout_seconds="$2"; shift 2 ;;
    -h|--help) usage ;;
    --) shift; break ;;
    -*) echo "worker.sh: unknown option '$1' (model and thinking are fixed)" >&2; usage ;;
    *) break ;;
  esac
done

name="${1:-}"
brief="${2:-}"
[ -n "$name" ] || usage
[ -n "$brief" ] || usage
[ -f "$brief" ] || { echo "worker.sh: brief not found: $brief" >&2; exit 66; }

case "$timeout_seconds" in
  ''|*[!0-9]*) echo "worker.sh: --timeout must be whole seconds" >&2; exit 64 ;;
esac

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$root/logs/workers"
log="$root/logs/workers/${name}.log"
status="$root/logs/workers/${name}.status"
events="$root/logs/workers/${name}.events.jsonl"

# JSON string escaping: backslash and double quote are the only characters that
# can appear in a repo-relative path; control characters are stripped rather
# than escaped, so a malformed path can never break the event stream.
json_string() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  s="${s//$'\t'/ }"
  s="${s//$'\n'/ }"
  s="${s//$'\r'/ }"
  printf '"%s"' "$s"
}

emit() { printf '%s\n' "$1" >>"$events"; }

# Snapshot of paths already dirty before the run, so a pre-existing dirty tree
# is not blamed on the worker.
baseline_paths="$(
  cd "$root" && git status --porcelain -uall 2>/dev/null |
    sed -e 's/^...//' -e 's/.* -> //' | sort
)"
baseline_file="$(mktemp)"
printf '%s\n' "$baseline_paths" >"$baseline_file"

started_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
started=$(date +%s)

cd "$root" || exit 1

emit "{\"event\":\"worker_started\",\"worker\":$(json_string "$name"),\"brief\":$(json_string "$brief"),\"model\":$(json_string "$WORKER_MODEL"),\"pid\":$$,\"timeoutSeconds\":$timeout_seconds,\"startedUtc\":$(json_string "$started_utc")}"

worker_prompt="You are a focused implementation worker. You write CODE ONLY: source files and their tests.

Hard rules:
- Write code and tests only. Never create or edit documentation or data: nothing under docs/, data/, assets/, tools/ or logs/, and no Markdown file at the repository root (AGENTS.md, PLAN.md, README.md).
- Do not commit, amend, stash, checkout, reset or otherwise run git commands.
- Do not modify files outside the paths the brief allows. Stay inside src/ and tests/ unless the brief names CMakeLists.txt.
- Do not run network commands, package installs, data downloads, GDAL imports, or provider API calls.
- If the brief asks for documentation, data authoring or threshold design, stop and say so instead of writing those files.
- Read AGENTS.md and PLAN.md for the spec. Match the existing Qt/C++ style. Add tests for new behaviour.
- Build and test what you touched: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug (if build/ is absent), then cmake --build build -j and ctest --test-dir build --output-on-failure."

if command -v timeout >/dev/null 2>&1; then
  timeout --kill-after=30 "$timeout_seconds" pi -p \
    --model "$WORKER_MODEL" \
    --thinking "$WORKER_THINKING" \
    --session-dir "$root/logs/pi-sessions" \
    --name "worker-${name}" \
    --no-mcp \
    --append-system-prompt "$worker_prompt" \
    "$(cat "$brief")" >"$log" 2>&1
  code=$?
else
  pi -p \
    --model "$WORKER_MODEL" \
    --thinking "$WORKER_THINKING" \
    --session-dir "$root/logs/pi-sessions" \
    --name "worker-${name}" \
    --no-mcp \
    --append-system-prompt "$worker_prompt" \
    "$(cat "$brief")" >"$log" 2>&1
  code=$?
fi

ended=$(date +%s)
seconds=$((ended - started))
finished_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

timed_out=false
# 124 = timeout's own exit; 137 = killed after the --kill-after grace period.
if [ "$code" -eq 124 ] || [ "$code" -eq 137 ]; then
  timed_out=true
fi

# --- Worker contract audit ---------------------------------------------------
# Paths touched by this run that are outside code/test territory.
violations=()
if command -v git >/dev/null 2>&1; then
  while IFS= read -r path; do
    [ -n "$path" ] || continue
    grep -Fxq -- "$path" "$baseline_file" && continue

    forbidden=false
    for prefix in "${FORBIDDEN_PREFIXES[@]}"; do
      case "$path" in "$prefix"*) forbidden=true; break ;; esac
    done
    case "$path" in
      */*) : ;;                                     # nested: prefix rules decided it
      *"$FORBIDDEN_ROOT_SUFFIX") forbidden=true ;;  # root-level Markdown
    esac

    [ "$forbidden" = true ] && violations+=("$path")
  done < <(git status --porcelain -uall 2>/dev/null | sed -e 's/^...//' -e 's/.* -> //')
fi
rm -f "$baseline_file"

if [ "${#violations[@]}" -gt 0 ]; then
  joined=""
  for path in "${violations[@]}"; do
    joined="${joined:+$joined,}$(json_string "$path")"
  done
  emit "{\"event\":\"worker_policy_violation\",\"worker\":$(json_string "$name"),\"paths\":[$joined],\"finishedUtc\":$(json_string "$finished_utc")}"
  printf 'worker.sh: %s wrote outside code paths (workers write code only):\n' "$name" >&2
  printf '  %s\n' "${violations[@]}" >&2
  effective=$(( code == 0 ? 78 : code ))
else
  effective="$code"
fi

emit "{\"event\":\"worker_finished\",\"worker\":$(json_string "$name"),\"exit\":$effective,\"workerExit\":$code,\"timedOut\":$timed_out,\"seconds\":$seconds,\"policyViolations\":${#violations[@]},\"log\":$(json_string "$log"),\"finishedUtc\":$(json_string "$finished_utc")}"

# Human-readable one-liner, written atomically so a reader never sees a partial line.
tmp="$status.tmp.$$"
printf 'worker=%s exit=%s seconds=%s timed_out=%s violations=%s log=%s\n' \
  "$name" "$effective" "$seconds" "$timed_out" "${#violations[@]}" "$log" >"$tmp"
mv -f "$tmp" "$status"
cat "$status"

exit "$effective"
