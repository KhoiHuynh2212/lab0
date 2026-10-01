#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

make -C "$ROOT/implementation/obj-a" test
make -C "$ROOT/implementation/obj-b/simple-queue-sim" test

for tool in \
    filter-by-queue \
    filter-by-job \
    compute-use \
    derive-sys-metrics \
    plot-use \
    plot-sys-metrics \
    derive-event-metrics \
    plot-derive-event-metrics \
    create-event-trace
do
    make -C "$ROOT/implementation/obj-c/$tool" test
done

make -C "$ROOT/implementation/obj-e" test

echo "All tests passed."