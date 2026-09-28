#!/usr/bin/env bash

# Test create-event-trace: create PNGs from the fixture and check error cases.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_BUILD_DIRECTORY="$SCRIPT_DIRECTORY/.test-build"
FIXTURE_DIRECTORY="$SCRIPT_DIRECTORY/tests/fixtures"
PYTHON_COMMAND="${PYTHON:-python3}"
TOOL="$SCRIPT_DIRECTORY/create_event_trace.py"

rm -rf "$TEST_BUILD_DIRECTORY"
mkdir -p "$TEST_BUILD_DIRECTORY"

if ! "$PYTHON_COMMAND" -c "import matplotlib" >/dev/null 2>&1; then
    echo "matplotlib is required to run the create-event-trace tests." >&2
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
"$PYTHON_COMMAND" "$TOOL" "$FIXTURE_DIRECTORY/jobin.log" "$TEST_BUILD_DIRECTORY/trace.png"
expect_png "$TEST_BUILD_DIRECTORY/trace.png"

"$PYTHON_COMMAND" "$TOOL" "$FIXTURE_DIRECTORY/jobin.log" "$TEST_BUILD_DIRECTORY/titled.png" --title="Fixture trace"
expect_png "$TEST_BUILD_DIRECTORY/titled.png"

echo "[2/3] Launcher smoke test"
"$SCRIPT_DIRECTORY/create-event-trace" --help >/dev/null

echo "[3/3] Failure cases"
expect_failure
expect_failure "$FIXTURE_DIRECTORY/jobin.log"
expect_failure "$TEST_BUILD_DIRECTORY/missing.log" "$TEST_BUILD_DIRECTORY/bad.png"

EMPTY_LOG="$TEST_BUILD_DIRECTORY/empty.log"
printf 'Event Log\n\n' > "$EMPTY_LOG"
expect_failure "$EMPTY_LOG" "$TEST_BUILD_DIRECTORY/bad.png"

echo "ALL CREATE-EVENT-TRACE TESTS PASSED"
