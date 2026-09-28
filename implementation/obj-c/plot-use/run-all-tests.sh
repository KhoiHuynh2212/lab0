#!/usr/bin/env bash

# Test plot-use: create three PNGs from the fixture and check error cases.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_BUILD_DIRECTORY="$SCRIPT_DIRECTORY/.test-build"
FIXTURE_DIRECTORY="$SCRIPT_DIRECTORY/tests/fixtures"
PYTHON_COMMAND="${PYTHON:-python3}"

rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"

if ! "$PYTHON_COMMAND" -c "import matplotlib" >/dev/null 2>&1; then
    echo "matplotlib is required to run the plot-use tests." >&2
    exit 1
fi

echo "[1/3] Unit / CLI success path"
RESULTS_DIRECTORY="$TEST_BUILD_DIRECTORY/results"
"$PYTHON_COMMAND" "$SCRIPT_DIRECTORY/plot_use.py" \
    "$FIXTURE_DIRECTORY/quse.csv" \
    "$RESULTS_DIRECTORY"

for png_name in utilization.png saturation.png error-rate.png; do
    png_file="$RESULTS_DIRECTORY/$png_name"
    if [[ ! -f "$png_file" ]]; then
        echo "missing expected plot: $png_file" >&2
        exit 1
    fi

    png_signature="$(od -An -t u1 -N8 "$png_file" | tr -s ' ' | sed 's/^ //; s/ $//')"
    if [[ "$png_signature" != "137 80 78 71 13 10 26 10" ]]; then
        echo "not a PNG file: $png_file" >&2
        exit 1
    fi
done

echo "[2/3] Launcher smoke test"
"$SCRIPT_DIRECTORY/plot-use" --help >/dev/null

echo "[3/3] Failure cases"
expect_failure() {
    set +e
    "$PYTHON_COMMAND" "$SCRIPT_DIRECTORY/plot_use.py" "$@" >/dev/null 2>&1
    local status=$?
    set -e
    if [[ $status -eq 0 ]]; then
        echo "expected command to fail: $*" >&2
        exit 1
    fi
}

# Missing required arguments / files / columns.
expect_failure
expect_failure "$TEST_BUILD_DIRECTORY/missing.csv" "$TEST_BUILD_DIRECTORY/bad-results"

# Missing required column.
BAD_CSV="$TEST_BUILD_DIRECTORY/bad.csv"
printf 'time-step,Utilization(10)\n0,1\n' > "$BAD_CSV"
expect_failure "$BAD_CSV" "$TEST_BUILD_DIRECTORY/bad-results"

echo "ALL PLOT-USE TESTS PASSED"
