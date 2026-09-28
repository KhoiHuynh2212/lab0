#!/usr/bin/env bash

# Build and test the workload visualizer with Valgrind.
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
    echo "Valgrind is required to run the workload-visualizer tests." >&2
    exit 1
fi

# gcov is only needed when a coverage report was requested.
if [[ "$TEST_MODE" != "leak" ]] && ! command -v gcov >/dev/null 2>&1; then
    echo "gcov is required to run the workload-visualizer coverage tests." >&2
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

# Recreate the test directory so old plots and coverage files cannot affect this run.
rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"
cd "$TEST_BUILD_DIRECTORY"

# Warning flags shared by the unit-test and command-line builds.
COMMON_FLAGS=(
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

echo "[1/3] Workload-visualizer unit tests under Valgrind"
"$CXX_COMPILER" "${COMMON_FLAGS[@]}" "${TEST_BUILD_FLAGS[@]}" \
    "$SCRIPT_DIRECTORY/tests/test_workload_visualizer.cpp" \
    "$SCRIPT_DIRECTORY/src/workload_visualizer.cpp" \
    -o tests-workload-visualizer

run_with_valgrind ./tests-workload-visualizer

echo "[2/3] Workload-visualizer command-line tests under Valgrind"
"$CXX_COMPILER" "${COMMON_FLAGS[@]}" "${TEST_BUILD_FLAGS[@]}" \
    "$SCRIPT_DIRECTORY/src/main.cpp" \
    "$SCRIPT_DIRECTORY/src/workload_visualizer.cpp" \
    -o workload-visualizer

# Help is a normal successful command-line path.
run_with_valgrind ./workload-visualizer --help >/dev/null

# Missing required options should be rejected cleanly.
expect_program_failure ./workload-visualizer

# Create a small workload that can be used by both plotting modes.
WORKLOAD_FILE="$TEST_BUILD_DIRECTORY/workload.csv"
cat > "$WORKLOAD_FILE" <<'CSV'
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
CSV

# Create a histogram through Valgrind.
run_with_valgrind ./workload-visualizer "$WORKLOAD_FILE" \
    --hist="arrival time" \
    --bins=5 \
    --out="$TEST_BUILD_DIRECTORY/histogram.png" >/dev/null

# Create a scatter plot through Valgrind.
run_with_valgrind ./workload-visualizer "$WORKLOAD_FILE" \
    --scat="arrival time,len(Tasks)" \
    --out="$TEST_BUILD_DIRECTORY/scatter.png" >/dev/null

# Every PNG begins with the same eight-byte signature.
for png_file in "$TEST_BUILD_DIRECTORY/histogram.png" "$TEST_BUILD_DIRECTORY/scatter.png"; do
    png_signature="$(od -An -t u1 -N8 "$png_file" | tr -s ' ' | sed 's/^ //; s/ $//')"
    if [[ "$png_signature" != "137 80 78 71 13 10 26 10" ]]; then
        echo "not a PNG file: $png_file" >&2
        exit 1
    fi
done

# The same Valgrind runs above also create gcov data when coverage is enabled.
if [[ "$TEST_MODE" != "leak" ]]; then
    echo "[3/3] Workload-visualizer gcov report"

    # The unit-test build covers the plotting and workload-reading logic.
    gcov -b -c tests-workload-visualizer-workload_visualizer.gcno >/dev/null

    # The command-line build covers argument parsing in main.cpp.
    gcov -b -c workload-visualizer-main.gcno >/dev/null

    echo "Coverage files were written to: $TEST_BUILD_DIRECTORY"
else
    echo "[3/3] Leak test complete"
fi

echo "ALL WORKLOAD-VISUALIZER TESTS PASSED UNDER VALGRIND"
