#!/usr/bin/env bash

# Test plot-sys-metrics: create PNGs from the fixture and check error cases.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_BUILD_DIRECTORY="$SCRIPT_DIRECTORY/.test-build"
FIXTURE_DIRECTORY="$SCRIPT_DIRECTORY/tests/fixtures"
PYTHON_COMMAND="${PYTHON:-python3}"
TOOL="$SCRIPT_DIRECTORY/plot_sys_metrics.py"

rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"

if ! "$PYTHON_COMMAND" -c "import matplotlib" >/dev/null 2>&1; then
    echo "matplotlib is required to run the plot-sys-metrics tests." >&2
    exit 1
fi

expect_png() {
    if [[ ! -f "$1" ]]; then
        echo "missing expected plot: $1" >&2
        exit 1
    fi
    local signature
    signature="$(od -An -t u1 -N8 "$1" | tr -s ' ' | sed 's/^ //; s/ $//')"
    if [[ "$signature" != "137 80 78 71 13 10 26 10" ]]; then
        echo "not a PNG file: $1" >&2
        exit 1
    fi
}

expect_failure() {
    set +e
    "$PYTHON_COMMAND" "$TOOL" "$@" >/dev/null 2>&1
    local status=$?
    set -e
    if [[ $status -eq 0 ]]; then
        echo "expected command to fail: $*" >&2
        exit 1
    fi
}

echo "[1/3] Success path"
"$PYTHON_COMMAND" "$TOOL" "$FIXTURE_DIRECTORY/sysmetric.csv" "$TEST_BUILD_DIRECTORY/plot.png"
expect_png "$TEST_BUILD_DIRECTORY/plot.png"

"$PYTHON_COMMAND" "$TOOL" "$FIXTURE_DIRECTORY/sysmetric.csv" "$TEST_BUILD_DIRECTORY/titled.png" --title="fixture"
expect_png "$TEST_BUILD_DIRECTORY/titled.png"

echo "[2/3] Launcher smoke test"
"$SCRIPT_DIRECTORY/plot-sys-metrics" --help >/dev/null

echo "[3/3] Failure cases"
expect_failure
expect_failure "$FIXTURE_DIRECTORY/sysmetric.csv"
expect_failure "$TEST_BUILD_DIRECTORY/missing.csv" "$TEST_BUILD_DIRECTORY/bad.png"

BAD_CSV="$TEST_BUILD_DIRECTORY/bad.csv"
printf 'time-step,throughput\n0,1\n' > "$BAD_CSV"
expect_failure "$BAD_CSV" "$TEST_BUILD_DIRECTORY/bad.png"

printf 'time-step,avg-response-time,median-response-time,10-response-time,90-response-time,throughput\n0,x,1,1,1,1\n' > "$BAD_CSV"
expect_failure "$BAD_CSV" "$TEST_BUILD_DIRECTORY/bad.png"

echo "ALL PLOT-SYS-METRICS TESTS PASSED"
