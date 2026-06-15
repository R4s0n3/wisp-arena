#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/client/main.cpp"
OUT_DIR="$ROOT/client/build"
OUT="$OUT_DIR/wisp-arena"

if ! command -v g++ >/dev/null 2>&1; then
  echo "g++ is required to build the client." >&2
  exit 1
fi

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config is required to locate raylib and ixwebsocket." >&2
  exit 1
fi

if ! pkg-config --exists raylib ixwebsocket; then
  echo "Missing development packages for raylib and/or ixwebsocket." >&2
  echo "Install them, then rerun: make run" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

read -r -a CFLAGS <<<"$(pkg-config --cflags raylib ixwebsocket)"
read -r -a LIBS <<<"$(pkg-config --libs raylib ixwebsocket)"

g++ -std=c++17 -O2 -pthread "${CFLAGS[@]}" "$SRC" -o "$OUT" "${LIBS[@]}"

printf '%s\n' "$OUT"
