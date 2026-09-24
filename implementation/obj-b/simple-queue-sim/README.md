# Obj-B: Simple Queue Simulator (C++)

## Build

```bash
make
```

This produces `./simple-queue-sim`.

## Run

```bash
./simple-queue-sim CONFIG [TIME_LIMIT] [--metrics FILE] [--events FILE]
```

or:

```bash
./simple-queue-sim CONFIG --time-limit N --metrics qmet.csv --events job.log
```

When no time limit is supplied, the simulator runs until all source jobs have arrived and the waiting queue and servers are empty. With a limit `N`, it processes `t = 0..N-1`, matching the `while clock < limit` convention in the provided queue example.

Source and sink paths inside the configuration are resolved relative to the configuration file. Metrics and event paths are relative to the current working directory unless an explicit path is passed.

### Example

```bash
make
cd ../../../examples/obj-b
../../implementation/obj-b/simple-queue-sim/simple-queue-sim \
  simple-queue-config-basic.csv 61 \
  --metrics qmet-basic.csv \
  --events events-basic.log
```

## Supported formats

Queue configuration:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,in-basic.wl,out-basic.wl,[],[]
```

Workload / sink:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
```

Supported functions are `Sub-X`, comma-separated chains such as `Sub-5,Sub-10`, and a trailing `*` to repeat the chain until `Tasks == []`.

## Main data structures

- `Task` stores a task ID and remaining work.
- `Job` stores a job ID, arrival time, and task list.
- `QueueConfig` stores the queue settings loaded from the configuration CSV.
- `SimulationResult` stores a short summary of the finished run.

The CSV, workload, and log examples stay comment-free because they are read as data files.

## Tests

```bash
./run-all-tests.sh
```

The test script performs:

- parser and simulator unit/integration tests,
- AddressSanitizer + UndefinedBehaviorSanitizer with leak detection,
- CLI smoke/error tests,
- gcov checking with **zero ordinary uncovered source lines** (100% reachable core source-line coverage), and
- Valgrind leak checking when Valgrind is installed.

GCC may mark compiler-generated exception-cleanup code attached to closing braces as `=====` rather than a normal source line; the script ignores only those cleanup-only markers and fails on any `#####` source line.

## Files

```text
include/queue_sim.hpp   public data structures and simulator API
src/queue_sim.cpp       CSV/workload/config parsing and simulation engine
src/main.cpp            command-line interface
tests/test_queue_sim.cpp automated tests
run-all-tests.sh        coverage/leak/test runner
Makefile                build targets
```
