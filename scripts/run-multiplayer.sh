#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLIENT_COUNT="${CLIENT_COUNT:-2}"
SERVER_URL="${SERVER_URL:-ws://localhost:9001}"
SERVER_PORT="${PORT:-9001}"
CLIENT_BIN="$ROOT/client/build/wisp-arena"
CLIENT_PIDS=()

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" >/dev/null 2>&1; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
  fi
  for pid in "${CLIENT_PIDS[@]}"; do
    kill "$pid" >/dev/null 2>&1 || true
  done
}

wait_for_port() {
  local tries=100
  while (( tries > 0 )); do
    if ! kill -0 "$SERVER_PID" >/dev/null 2>&1; then
      echo "Server exited before it opened port $SERVER_PORT." >&2
      return 1
    fi
    if (exec 3<>"/dev/tcp/127.0.0.1/$SERVER_PORT") >/dev/null 2>&1; then
      exec 3>&-
      exec 3<&-
      return 0
    fi
    sleep 0.1
    ((tries--))
  done

  echo "Server did not come up on port $SERVER_PORT." >&2
  return 1
}

trap cleanup EXIT INT TERM

"$ROOT/scripts/build-client.sh" >/dev/null

(cd "$ROOT/server" && bun run start) &
SERVER_PID=$!

wait_for_port

for _ in $(seq 1 "$CLIENT_COUNT"); do
  "$CLIENT_BIN" "$SERVER_URL" &
  CLIENT_PIDS+=("$!")
done

wait
