# Objective B: Simple Queue Simulator

Objective B contains the C++ simple queue simulator in:

[`/implementation/obj-b/simple-queue-sim/`](/implementation/obj-b/simple-queue-sim/)

The simulator reads a queue configuration and a source workload, then produces queue and job output files.

```text
┌─────────────────────────────────┐
│ queue config + source workload  │
└────────────────┬────────────────┘
                 │
                 ↓
        ┌──────────────────┐
        │ simple-queue-sim │
        └────────┬─────────┘
                 │
        ┌────────┼────────┐
        │        │        │
        ↓        ↓        ↓
 ┌───────────┐ ┌──────────┐ ┌─────────┐
 │ sink file │ │ qmet.csv │ │ job.log │
 └───────────┘ └──────────┘ └─────────┘
```

---

## 1. Build

From the repository root:

```bash
make -C implementation/obj-b/simple-queue-sim build
```

From the simulator directory:

```bash
cd implementation/obj-b/simple-queue-sim
make build
```

Typing only `make` also builds the simulator.

The executable is:

```txt
implementation/obj-b/simple-queue-sim/simple-queue-sim
```

---

## 2. Run

Run until all source jobs have arrived and the queue and servers are empty:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv
```

Run for a fixed number of timesteps:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv 100
```

or:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --time-limit 100
```

Choose output files:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --metrics qmet.csv \
    --events job.log
```

The complete command reference is here:

[`/implementation/obj-b/simple-queue-sim/README.md`](/implementation/obj-b/simple-queue-sim/README.md)

---

## 3. Input Files

The simulator needs exactly one queue-system row in its configuration:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,in.wl,out.wl,[],[]
```

The source workload uses:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 20', '1: W 10']"
```

Source and sink paths in the configuration are relative to the configuration file's directory.

---

## 4. Outputs

The simulator can create:

```txt
sink workload   completed jobs
qmet.csv        one queue-metrics row per timestep
job.log         job state changes and errors
```

If custom `--metrics` and `--events` paths are not provided, the defaults are:

```txt
qmet.csv
job.log
```

---

## 5. Tests

Run all Obj-B tests:

```bash
make -C implementation/obj-b/simple-queue-sim test
```

Coverage tests:

```bash
make -C implementation/obj-b/simple-queue-sim run-tests-coverage
```

Leak tests:

```bash
make -C implementation/obj-b/simple-queue-sim run-tests-leak
```

Clean test and build files:

```bash
make -C implementation/obj-b/simple-queue-sim clean
```

The test targets require Valgrind.

The test script may also be run directly:

```bash
implementation/obj-b/simple-queue-sim/run-all-tests.sh
implementation/obj-b/simple-queue-sim/run-all-tests.sh all
implementation/obj-b/simple-queue-sim/run-all-tests.sh coverage
implementation/obj-b/simple-queue-sim/run-all-tests.sh leak
```

---

## 6. Connection to Obj-A

Obj-A's generator creates the source workload format used by Obj-B:

```text
┌──────────────────────────┐
│ Obj-A workload-generator │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│       workload.csv       │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│     Obj-B simulator      │
└──────────────────────────┘
```

Obj-A's visualizer can also read the completed sink workload produced by Obj-B.

For a full walkthrough, see:

[`/README.md#5-running-obj-a-and-obj-b-together`](/README.md#5-running-obj-a-and-obj-b-together)

---

## 7. Related Documentation

- [Simple Queue Simulator run guide](/implementation/obj-b/simple-queue-sim/README.md)
- [Simple Queue Simulator design](/designs/obj-b/simple-queue-sim/README.md)
- [Obj-B examples](/examples/obj-b/README.md)
- [`q_ex-1.py` explained](/examples/obj-b/q_ex-1_explained.md)
