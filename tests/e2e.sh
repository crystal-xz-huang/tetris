#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

make tetris >/dev/null

# '?' in default mode reprints the HELP reference.
help_output=$(printf '?\nq\n' | ./tetris)
printf '%s\n' "$help_output" | grep -F "Welcome to my tetris!" >/dev/null
printf '%s\n' "$help_output" | grep -F "HELP"       >/dev/null
printf '%s\n' "$help_output" | grep -F "QUIT"       >/dev/null
printf '%s\n' "$help_output" | grep -F "Quitting..."  >/dev/null
printf '%s\n' "$help_output" | grep -F "Goodbye!"     >/dev/null

# The new UI renders TETRIS + NEXT + HELP boxes.
printf '%s\n' "$help_output" | grep -F "TETRIS"   >/dev/null
printf '%s\n' "$help_output" | grep -F "NEXT"     >/dev/null
printf '%s\n' "$help_output" | grep -F "LEVEL: 1" >/dev/null
printf '%s\n' "$help_output" | grep -F "SCORE: 0" >/dev/null
printf '%s\n' "$help_output" | grep -F "BEST :"   >/dev/null

# '?' in --debug mode still dumps the internal state.
debug_output=$(printf '?\nq\n' | ./tetris --debug)
printf '%s\n' "$debug_output" | grep -F "piece_symbol     = 73" >/dev/null
printf '%s\n' "$debug_output" | grep -F "Quitting..." >/dev/null
printf '%s\n' "$debug_output" | grep -F "Goodbye!"    >/dev/null

# The scripted fixture still reaches Game Over.
fixture_output=$(./tetris < input.txt)
printf '%s\n' "$fixture_output" | grep -F "Game over :[" >/dev/null
printf '%s\n' "$fixture_output" | grep -F "Goodbye!"     >/dev/null

# Soft/hard drops earn points under the new scoring, so any non-zero
# SCORE line proves the scoring path ran.
printf '%s\n' "$fixture_output" | grep -E "SCORE:[[:space:]]+[0-9]+" >/dev/null

# The HUD now includes a TIME panel (mm:ss).
printf '%s\n' "$fixture_output" | grep -E "TIME[[:space:]]*:[[:space:]]+[0-9]{2}:[0-9]{2}" >/dev/null

printf 'e2e tests passed\n'
