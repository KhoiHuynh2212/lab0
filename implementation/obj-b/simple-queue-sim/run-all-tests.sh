#!/usr/bin/env bash

# Build and test the Obj-B queue simulator with Valgrind.
set -euo pipefail

# Find this directory so the script works from any current directory.
SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_BUILD_DIRECTORY="$SCRIPT_DIRECTORY/.test-build"
CXX_COMPILER="${CXX:-g++}"
TEST_MODE="${1:-all}"
VALGRIND_COMMAND="${VALGRIND:-valgrind}"

# These options report invalid memory access, leaks, reachable memory, and open files.
VALGRIND_OPTIONS=(
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=all
    --show-reachable=yes
    --num-callers=20
    --track-fds=yes
    --errors-for-leak-kinds=all
    --error-exitcode=99
)

# Accept the three modes used by the Makefile.
case "$TEST_MODE" in
    all|coverage|leak)
        ;;
    *)
        echo "usage: $0 [all|coverage|leak]" >&2
        exit 2
        ;;
esac

# Valgrind is required because every executable test is run through it.
if ! command -v "$VALGRIND_COMMAND" >/dev/null 2>&1; then
    echo "Valgrind is required to run the Obj-B tests." >&2
    exit 1
fi

# gcov is only needed when a coverage report was requested.
if [[ "$TEST_MODE" != "leak" ]] && ! command -v gcov >/dev/null 2>&1; then
    echo "gcov is required to run the Obj-B coverage tests." >&2
    exit 1
fi

# Use one helper so every normal test program gets the same Valgrind settings.
run_with_valgrind() {
    "$VALGRIND_COMMAND" "${VALGRIND_OPTIONS[@]}" "$@"
}

# Some command-line tests are supposed to fail. A Valgrind failure is still an error.
expect_program_failure() {
    set +e
    run_with_valgrind "$@" >/dev/null 2>&1
    local program_status=$?
    set -e

    if [[ $program_status -eq 0 ]]; then
        echo "expected command to fail: $*" >&2
        exit 1
    fi

    if [[ $program_status -eq 99 ]]; then
        echo "Valgrind found a memory error while running: $*" >&2
        exit 1
    fi
}

# Recreate the test directory so old output and coverage files cannot affect this run.
rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"
cd "$TEST_BUILD_DIRECTORY"

# Warning flags shared by the unit-test and command-line builds.
COMMON_COMPILER_FLAGS=(
    -std=c++20
    -Wall
    -Wextra
    -Wpedantic
    -I"$SCRIPT_DIRECTORY/include"
)

# Coverage mode adds gcov instrumentation. Leak-only mode keeps debug symbols.
if [[ "$TEST_MODE" == "leak" ]]; then
    TEST_BUILD_FLAGS=(-O0 -g)
else
    TEST_BUILD_FLAGS=(-O0 -g -fprofile-arcs -ftest-coverage)
fi

echo "[1/3] Obj-B unit and integration tests under Valgrind"
"$CXX_COMPILER" "${COMMON_COMPILER_FLAGS[@]}" "${TEST_BUILD_FLAGS[@]}" \
    "$SCRIPT_DIRECTORY/tests/test_queue_sim.cpp" \
    "$SCRIPT_DIRECTORY/src/queue_sim.cpp" \
    -o tests-queue-sim

run_with_valgrind ./tests-queue-sim

echo "[2/3] Obj-B command-line tests under Valgrind"
"$CXX_COMPILER" "${COMMON_COMPILER_FLAGS[@]}" "${TEST_BUILD_FLAGS[@]}" \
    "$SCRIPT_DIRECTORY/src/main.cpp" \
    "$SCRIPT_DIRECTORY/src/queue_sim.cpp" \
    -o simple-queue-sim

# Help should succeed.
run_with_valgrind ./simple-queue-sim --help >/dev/null

# Running without a configuration should fail cleanly.
expect_program_failure ./simple-queue-sim

# A missing configuration file should also fail cleanly.
expect_program_failure ./simple-queue-sim /definitely/not/a/config.csv

# The same Valgrind run above creates gcov data when coverage is enabled.
if [[ "$TEST_MODE" != "leak" ]]; then
    echo "[3/3] Obj-B core line coverage"

    GCOV_REPORT="$(gcov -b -c tests-queue-sim-queue_sim.gcno 2>&1)"
    printf '%s\n' "$GCOV_REPORT" | grep -A4 -F "$SCRIPT_DIRECTORY/src/queue_sim.cpp" || true

    LINE_COVERAGE_PERCENTAGE="$({
        printf '%s\n' "$GCOV_REPORT" | awk -v target="$SCRIPT_DIRECTORY/src/queue_sim.cpp" '
            $0 == "File \047" target "\047" { found_target = 1; next }
            found_target && /^Lines executed:/ {
                gsub(/%/, "", $2)
                split($2, percentage_parts, ":")
                print percentage_parts[2]
                exit
            }
        '
    })"

    if [[ -z "$LINE_COVERAGE_PERCENTAGE" ]]; then
        echo "could not determine gcov line coverage" >&2
        exit 1
    fi

    # gcov uses ##### for ordinary executable lines that were never reached.
    if grep -q '#####' queue_sim.cpp.gcov; then
        echo "uncovered reachable lines remain:" >&2
        grep -n '#####' queue_sim.cpp.gcov >&2
        exit 1
    fi

    echo "Reachable source-line coverage: 100% (gcov raw: ${LINE_COVERAGE_PERCENTAGE}%)"
else
    echo "[3/3] Leak test complete"
fi

echo "ALL OBJ-B TESTS PASSED UNDER VALGRIND"
