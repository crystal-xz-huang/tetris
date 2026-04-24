#!/bin/bash
#
# run_tests.sh — regression + benchmark suite for the tetris refactor.
#
# Phase 1 (correctness): for every tests/*.in, runs ./tetris and ./tetris_v2,
#                        diffs their combined stdout+stderr, and checks that
#                        their exit codes match.
# Phase 2 (benchmark):   runs each binary N times per test, reports wall
#                        time per test and overall, and prints the speedup.
#
# Usage:
#   ./run_tests.sh                    # correctness + benchmark
#   ./run_tests.sh --no-bench         # correctness only
#   ./run_tests.sh --bench-only       # skip diffing, only time
#   TETRIS_BENCH_ITERS=20 ./run_tests.sh
#
# A test PASSes iff both binaries produce byte-identical output AND
# return the same exit code.

set -euo pipefail
cd "$(dirname "$0")"

BENCH=1
DIFF=1
for arg in "$@"; do
    case "$arg" in
        --no-bench)   BENCH=0 ;;
        --bench-only) DIFF=0 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

ITERS=${TETRIS_BENCH_ITERS:-10}

# --- Build -----------------------------------------------------------------

echo "Building tetris and tetris_v2..."
if ! make tetris tetris_v2 > /tmp/tetris_build.log 2>&1; then
    echo "BUILD FAILED:"
    cat /tmp/tetris_build.log
    exit 2
fi

if [ ! -x ./tetris ] || [ ! -x ./tetris_v2 ]; then
    echo "ERROR: expected ./tetris and ./tetris_v2 to exist after build" >&2
    exit 2
fi

# Sanity-check that each binary can actually run in this environment.
# Catches stale binaries left over from a previous build on a different
# toolchain (where 'make' thinks they're up-to-date but they can't exec).
for bin in ./tetris ./tetris_v2; do
    rc=0
    echo q | "$bin" > /dev/null 2>&1 || rc=$?
    if [ "$rc" != 0 ]; then
        echo "ERROR: $bin exited $rc on a trivial 'q' input." >&2
        if [ "$rc" = 126 ] || [ "$rc" = 127 ]; then
            echo "       Binary is likely stale or built for another toolchain." >&2
            echo "       Try:  make clean && make" >&2
        fi
        exit 2
    fi
done

if [ -z "$(ls tests/*.in 2>/dev/null)" ]; then
    echo "No tests found in ./tests/" >&2
    exit 2
fi

# --- Phase 1: correctness --------------------------------------------------

PASS=0
FAIL=0
FAILED_TESTS=()

if [ $DIFF -eq 1 ]; then
    for test_input in tests/*.in; do
        name=$(basename "$test_input" .in)

        orig_out="tests/${name}.out.original"
        v2_out="tests/${name}.out.v2"

        # `|| rc=$?` pattern: under `set -e`, a bare non-zero exit aborts
        # the script. The ||-branch both suppresses that and captures the code.
        orig_rc=0
        ./tetris    < "$test_input" > "$orig_out" 2>&1 || orig_rc=$?
        v2_rc=0
        ./tetris_v2 < "$test_input" > "$v2_out"   2>&1 || v2_rc=$?

        if [ "$orig_rc" != "$v2_rc" ]; then
            echo "FAIL: $name (exit codes differ: tetris=$orig_rc tetris_v2=$v2_rc)"
            FAIL=$((FAIL + 1))
            FAILED_TESTS+=("$name")
            continue
        fi

        if diff -q "$orig_out" "$v2_out" > /dev/null; then
            echo "PASS: $name"
            PASS=$((PASS + 1))
            rm -f "$orig_out" "$v2_out" 2>/dev/null || true
        else
            echo "FAIL: $name (output differs)"
            diff "$orig_out" "$v2_out" | head -30 | sed 's/^/    /'
            FAIL=$((FAIL + 1))
            FAILED_TESTS+=("$name")
        fi
    done

    echo ""
    echo "Correctness: $PASS passed, $FAIL failed"

    if [ $FAIL -gt 0 ]; then
        echo "Failed tests:"
        for t in "${FAILED_TESTS[@]-}"; do
            [ -z "$t" ] && continue
            echo "  - $t"
        done
        echo "(Output files left in tests/*.out.{original,v2} for inspection.)"
        exit 1
    fi
fi

# --- Phase 2: benchmark ----------------------------------------------------

if [ $BENCH -eq 0 ]; then
    exit 0
fi

if ! command -v python3 > /dev/null 2>&1; then
    echo ""
    echo "Skipping benchmark (python3 not found)."
    exit 0
fi

echo ""
echo "Benchmark ($ITERS iterations per test, wall-clock):"
echo ""

python3 - "$ITERS" tests/*.in <<'PY'
import subprocess, sys, time

iters = int(sys.argv[1])
tests = sys.argv[2:]

def bench(binary, test):
    """Run `binary < test` `iters` times, return total wall-clock seconds."""
    t0 = time.perf_counter()
    for _ in range(iters):
        with open(test, 'rb') as stdin:
            subprocess.run(
                [binary], stdin=stdin,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
    return time.perf_counter() - t0

hdr_test   = 'test'
hdr_orig   = 'tetris (s)'
hdr_v2     = 'tetris_v2 (s)'
hdr_delta  = 'delta (s)'
hdr_speed  = 'speedup'
print(f"  {hdr_test:<28}  {hdr_orig:>11}  {hdr_v2:>14}  {hdr_delta:>10}  {hdr_speed:>8}")
print(f"  {'-'*28}  {'-'*11}  {'-'*14}  {'-'*10}  {'-'*8}")

total_orig = 0.0
total_v2   = 0.0
for test in tests:
    name = test.rsplit('/', 1)[-1]
    if name.endswith('.in'):
        name = name[:-3]
    t_orig = bench('./tetris',    test)
    t_v2   = bench('./tetris_v2', test)
    total_orig += t_orig
    total_v2   += t_v2
    delta   = t_orig - t_v2
    speedup = (t_orig / t_v2) if t_v2 > 0 else float('inf')
    print(f"  {name:<28}  {t_orig:>11.4f}  {t_v2:>14.4f}  "
          f"{delta:>+10.4f}  {speedup:>7.2f}x")

print(f"  {'-'*28}  {'-'*11}  {'-'*14}  {'-'*10}  {'-'*8}")
delta   = total_orig - total_v2
speedup = (total_orig / total_v2) if total_v2 > 0 else float('inf')
print(f"  {'TOTAL':<28}  {total_orig:>11.4f}  {total_v2:>14.4f}  "
      f"{delta:>+10.4f}  {speedup:>7.2f}x")

print()
print(f"Note: wall-clock includes process fork/exec — {iters}x per row.")
print(f"      The refactor is structural only, so expect the delta to be ~0")
print(f"      (within measurement noise) when both are built with the same compiler.")
PY

exit 0
