#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

make tetris >/dev/null

quick_output=$(printf '?\nq\n' | ./tetris)
printf '%s\n' "$quick_output" | grep -F "Welcome to my tetris!" >/dev/null
printf '%s\n' "$quick_output" | grep -F "piece_symbol     = 73" >/dev/null
printf '%s\n' "$quick_output" | grep -F "Quitting..." >/dev/null
printf '%s\n' "$quick_output" | grep -F "Goodbye!" >/dev/null

fixture_output=$(./tetris < input.txt)
printf '%s\n' "$fixture_output" | grep -F "SCORE: 240" >/dev/null
printf '%s\n' "$fixture_output" | grep -F "Game over :[" >/dev/null
printf '%s\n' "$fixture_output" | grep -F "Goodbye!" >/dev/null

printf 'e2e tests passed\n'
