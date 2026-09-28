#!/usr/bin/env bash

# Run both Obj-A test suites and verify that their files work with Obj-B.
set -euo pipefail

# Find the repository directories used by the integration test.
SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "$SCRIPT_DIRECTORY/../.." && pwd)"
GENERATOR_DIRECTORY="$SCRIPT_DIRECTORY/workload-generator"
VISUALIZER_DIRECTORY="$SCRIPT_DIRECTORY/workload-visualizer"
OBJ_B_DIRECTORY="$REPOSITORY_ROOT/implementation/obj-b/simple-queue-sim"
TEST_MODE="${1:-all}"
VALGRIND_COMMAND="${VALGRIND:-valgrind}"

# These options match the memory checks used by the component test scripts.
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

# Valgrind is required for both component tests and the Obj-A to Obj-B test.
if ! command -v "$VALGRIND_COMMAND" >/dev/null 2>&1; then
    echo "Valgrind is required to run the Obj-A tests." >&2
    exit 1
fi

# Use one helper for each program exercised by the compatibility test.
run_with_valgrind() {
    "$VALGRIND_COMMAND" "${VALGRIND_OPTIONS[@]}" "$@"
}

# Build the normal Obj-A executables before the component and compatibility tests.
make -C "$GENERATOR_DIRECTORY" build >/dev/null
make -C "$VISUALIZER_DIRECTORY" build >/dev/null

printf '\n=== Workload generator ===\n'
"$GENERATOR_DIRECTORY/run-all-tests.sh" "$TEST_MODE"

printf '\n=== Workload visualizer ===\n'
"$VISUALIZER_DIRECTORY/run-all-tests.sh" "$TEST_MODE"

printf '\n=== Obj-A to Obj-B compatibility under Valgrind ===\n'

# Keep all integration files in a temporary directory and remove them on exit.
TEMPORARY_DIRECTORY="$(mktemp -d)"
trap 'rm -rf "$TEMPORARY_DIRECTORY"' EXIT

CXX_COMPILER="${CXX:-g++}"

# Build a debug Obj-B executable for the compatibility test.
"$CXX_COMPILER" \
    -std=c++20 -Wall -Wextra -Wpedantic -O0 -g \
    -I"$OBJ_B_DIRECTORY/include" \
    "$OBJ_B_DIRECTORY/src/main.cpp" \
    "$OBJ_B_DIRECTORY/src/queue_sim.cpp" \
    -o "$TEMPORARY_DIRECTORY/simple-queue-sim"

# Obj-A creates a source workload in the exact format consumed by Obj-B.
run_with_valgrind "$GENERATOR_DIRECTORY/workload-generator" \
    --out "$TEMPORARY_DIRECTORY/source.csv" \
    --interval 0,2 \
    --distribution uniform \
    --min 1 \
    --max 1 \
    --tasks-per-job 1 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 1 >/dev/null

# Configure one queue that reads the generated workload and writes completed jobs.
cat > "$TEMPORARY_DIRECTORY/config.csv" <<'CSV'
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,1,source.csv,sink.csv,[],[]
CSV

# Obj-B reads the Obj-A workload while Valgrind checks the simulator process.
run_with_valgrind "$TEMPORARY_DIRECTORY/simple-queue-sim" \
    "$TEMPORARY_DIRECTORY/config.csv" \
    --metrics "$TEMPORARY_DIRECTORY/qmet.csv" \
    --events "$TEMPORARY_DIRECTORY/job.log" >/dev/null

# The sink keeps the workload columns and completed jobs have empty task lists.
test "$(wc -l < "$TEMPORARY_DIRECTORY/sink.csv" | tr -d ' ')" = "4"
grep -q '^j0,.*\[\]$' "$TEMPORARY_DIRECTORY/sink.csv"
grep -q '^j2,.*\[\]$' "$TEMPORARY_DIRECTORY/sink.csv"

# The visualizer reads the same workload format and creates a PNG through Valgrind.
run_with_valgrind "$VISUALIZER_DIRECTORY/workload-visualizer" \
    "$TEMPORARY_DIRECTORY/source.csv" \
    --hist="arrival time" \
    --bins=3 \
    --out="$TEMPORARY_DIRECTORY/source-histogram.png" >/dev/null

# Verify the eight-byte PNG signature.
png_signature="$(od -An -t u1 -N8 "$TEMPORARY_DIRECTORY/source-histogram.png" | tr -s ' ' | sed 's/^ //; s/ $//')"
if [[ "$png_signature" != "137 80 78 71 13 10 26 10" ]]; then
    echo "Obj-A integration plot was not a valid PNG" >&2
    exit 1
fi

echo "OBJ-A TESTS AND OBJ-B COMPATIBILITY CHECK PASSED UNDER VALGRIND"
