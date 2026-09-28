#!/usr/bin/env bash

# Build and test derive-event-metrics with Valgrind.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_BUILD_DIRECTORY="$SCRIPT_DIRECTORY/.test-build"
FIXTURE_DIRECTORY="$SCRIPT_DIRECTORY/tests/fixtures"
CXX_COMPILER="${CXX:-g++}"
TEST_MODE="${1:-all}"
VALGRIND_COMMAND="${VALGRIND:-valgrind}"

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

case "$TEST_MODE" in
    all|coverage|leak)
        ;;
    *)
        echo "usage: $0 [all|coverage|leak]" >&2
        exit 2
        ;;
esac

if ! command -v "$VALGRIND_COMMAND" >/dev/null 2>&1; then
    echo "Valgrind is required to run the derive-event-metrics tests." >&2
    exit 1
fi

if [[ "$TEST_MODE" != "leak" ]] && ! command -v gcov >/dev/null 2>&1; then
    echo "gcov is required to run the derive-event-metrics coverage tests." >&2
    exit 1
fi

run_with_valgrind() {
    "$VALGRIND_COMMAND" "${VALGRIND_OPTIONS[@]}" "$@"
}

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

rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"
cd "$TEST_BUILD_DIRECTORY"

COMMON_COMPILER_FLAGS=(
    -std=c++20
    -Wall
    -Wextra
    -Wpedantic
    -I"$SCRIPT_DIRECTORY/include"
)

if [[ "$TEST_MODE" == "leak" ]]; then
    TEST_BUILD_FLAGS=(-O0 -g)
else
    TEST_BUILD_FLAGS=(-O0 -g -fprofile-arcs -ftest-coverage)
fi

echo "[1/3] Unit tests under Valgrind"
"$CXX_COMPILER" "${COMMON_COMPILER_FLAGS[@]}" "${TEST_BUILD_FLAGS[@]}" \
    "$SCRIPT_DIRECTORY/tests/test_derive_event_metrics.cpp" \
    "$SCRIPT_DIRECTORY/src/derive_event_metrics.cpp" \
    -o tests-derive-event-metrics

run_with_valgrind ./tests-derive-event-metrics

echo "[2/3] Command-line tests under Valgrind"
"$CXX_COMPILER" "${COMMON_COMPILER_FLAGS[@]}" "${TEST_BUILD_FLAGS[@]}" \
    "$SCRIPT_DIRECTORY/src/main.cpp" \
    "$SCRIPT_DIRECTORY/src/derive_event_metrics.cpp" \
    -o derive-event-metrics

run_with_valgrind ./derive-event-metrics --help >/dev/null
expect_program_failure ./derive-event-metrics
expect_program_failure ./derive-event-metrics "$FIXTURE_DIRECTORY/jobin.log"
expect_program_failure ./derive-event-metrics "$TEST_BUILD_DIRECTORY/missing.log" out.csv

METRICS_OUTPUT="$TEST_BUILD_DIRECTORY/emetric.csv"
run_with_valgrind ./derive-event-metrics \
    "$FIXTURE_DIRECTORY/jobin.log" \
    "$METRICS_OUTPUT"

if ! diff -u "$FIXTURE_DIRECTORY/emetric.csv" "$METRICS_OUTPUT"; then
    echo "derived metrics did not match expected fixture" >&2
    exit 1
fi

if [[ "$TEST_MODE" != "leak" ]]; then
    echo "[3/3] Core line coverage"

    GCOV_REPORT="$(gcov -b -c tests-derive-event-metrics-derive_event_metrics.gcno 2>&1)"
    printf '%s\n' "$GCOV_REPORT" | grep -A4 -F "$SCRIPT_DIRECTORY/src/derive_event_metrics.cpp" || true

    LINE_COVERAGE_PERCENTAGE="$({
        printf '%s\n' "$GCOV_REPORT" | awk -v target="$SCRIPT_DIRECTORY/src/derive_event_metrics.cpp" '
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

    if grep -q '#####' derive_event_metrics.cpp.gcov; then
        echo "uncovered reachable lines remain:" >&2
        grep -n '#####' derive_event_metrics.cpp.gcov >&2
        exit 1
    fi

    echo "Reachable source-line coverage: 100% (gcov raw: ${LINE_COVERAGE_PERCENTAGE}%)"
else
    echo "[3/3] Leak test complete"
fi

echo "ALL DERIVE-EVENT-METRICS TESTS PASSED UNDER VALGRIND"
