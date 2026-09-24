#!/usr/bin/env bash

# Build and run the workload-generator tests with memory checks.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_BUILD_DIRECTORY="$SCRIPT_DIRECTORY/.test-build"
CXX_COMPILER="${CXX:-c++}"

rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"

COMMON_FLAGS=(
    -std=c++20
    -Wall
    -Wextra
    -Wpedantic
    -I"$SCRIPT_DIRECTORY/include"
)

echo "[1/3] Unit tests under ASan/UBSan"
"$CXX_COMPILER" "${COMMON_FLAGS[@]}" -O1 -g -fno-omit-frame-pointer \
    -fsanitize=address,undefined \
    "$SCRIPT_DIRECTORY/tests/test_workload_generator.cpp" \
    "$SCRIPT_DIRECTORY/src/workload_generator.cpp" \
    -o "$TEST_BUILD_DIRECTORY/tests"

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
    "$TEST_BUILD_DIRECTORY/tests"

echo "[2/3] Build and exercise the command line program"
"$CXX_COMPILER" "${COMMON_FLAGS[@]}" -O2 \
    "$SCRIPT_DIRECTORY/src/main.cpp" \
    "$SCRIPT_DIRECTORY/src/workload_generator.cpp" \
    -o "$TEST_BUILD_DIRECTORY/workload-generator"

"$TEST_BUILD_DIRECTORY/workload-generator" --help >/dev/null

if "$TEST_BUILD_DIRECTORY/workload-generator" >/dev/null 2>&1; then
    echo "expected missing command-line options to fail" >&2
    exit 1
fi

GENERATED_WORKLOAD="$TEST_BUILD_DIRECTORY/generated.csv"
"$TEST_BUILD_DIRECTORY/workload-generator" \
    --out "$GENERATED_WORKLOAD" \
    --interval 0,2 \
    --distribution uniform \
    --min 1 \
    --max 1 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 1 >/dev/null

grep -q '^Job Id,arrival time,Tasks$' "$GENERATED_WORKLOAD"
grep -q '^j0,0,' "$GENERATED_WORKLOAD"
grep -q '^j2,2,' "$GENERATED_WORKLOAD"

echo "[3/3] Optional Valgrind leak check"
if command -v valgrind >/dev/null 2>&1; then
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --errors-for-leak-kinds=all \
        --error-exitcode=99 \
        "$TEST_BUILD_DIRECTORY/workload-generator" --help >/dev/null
else
    echo "valgrind not installed; ASan LeakSanitizer check above was used"
fi

echo "ALL WORKLOAD-GENERATOR TESTS PASSED"
