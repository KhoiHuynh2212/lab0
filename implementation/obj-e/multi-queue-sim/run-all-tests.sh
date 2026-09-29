#!/usr/bin/env bash
set -euo pipefail

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -Wall -Wextra -Wpedantic -O0 -g}"
OBJB_DIR="${OBJB_DIR:-../../obj-b/simple-queue-sim}"
CPPFLAGS="-Iinclude -I${OBJB_DIR}/include"
VALGRIND="${VALGRIND:-valgrind}"

TEST_SOURCES=(
    src/test_multi_queue_sim.cpp
    src/multi_queue_sim.cpp
    "${OBJB_DIR}/src/queue_sim.cpp"
)

BUILD_DIR=".test-build"
TEST_BINARY="${BUILD_DIR}/test_multi_queue_sim"

mode="${1:-all}"

mkdir -p "${BUILD_DIR}"

# Plain (non-coverage) build: one compiler invocation is fine here.
build_test_binary_plain() {
    # shellcheck disable=SC2086
    "${CXX}" ${CPPFLAGS} ${CXXFLAGS} \
        "${TEST_SOURCES[@]}" -o "${TEST_BINARY}"
}

# Coverage build: compile each source to its own .o inside BUILD_DIR so the
# .gcno file gcov needs lands next to a predictable .o name, then link.
build_test_binary_with_coverage() {
    local object_files=()

    for source_file in "${TEST_SOURCES[@]}"; do
        local object_name
        object_name="$(basename "${source_file}" .cpp).o"
        local object_path="${BUILD_DIR}/${object_name}"

        # shellcheck disable=SC2086
        "${CXX}" ${CPPFLAGS} ${CXXFLAGS} --coverage -fprofile-arcs -ftest-coverage \
            -c "${source_file}" -o "${object_path}"

        object_files+=("${object_path}")
    done

    # shellcheck disable=SC2086
    "${CXX}" ${CXXFLAGS} --coverage "${object_files[@]}" -o "${TEST_BINARY}"
}

run_under_valgrind() {
    local valgrind_extra_flags="$1"
    # shellcheck disable=SC2086
    "${VALGRIND}" --error-exitcode=1 ${valgrind_extra_flags} "${TEST_BINARY}"
}

case "${mode}" in
    all)
        echo "==> building test binary"
        build_test_binary_plain
        echo "==> running under Valgrind (memcheck)"
        run_under_valgrind ""
        ;;

    leak)
        echo "==> building test binary"
        build_test_binary_plain
        echo "==> running under Valgrind (full leak check)"
        run_under_valgrind "--leak-check=full --show-leak-kinds=all"
        ;;

    coverage)
        echo "==> building test binary with coverage instrumentation"
        build_test_binary_with_coverage
        echo "==> running test binary (no Valgrind under coverage - gcov/Valgrind don't mix)"
        # Run from the repository root (not BUILD_DIR) so relative paths in
        # the tests (temp-directory based) behave exactly as they do
        # normally; gcov just needs the .gcda files, which land in BUILD_DIR
        # regardless of the run's current directory.
        "${TEST_BINARY}"
        echo "==> generating gcov report for the simulator core"
        gcov -o "${BUILD_DIR}/multi_queue_sim.o" src/multi_queue_sim.cpp
        ;;

    *)
        echo "usage: $0 [all|coverage|leak]" >&2
        exit 2
        ;;
esac

echo "==> ${mode} completed successfully"