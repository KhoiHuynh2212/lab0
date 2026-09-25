# Objective B: Simple Queue Simulator

`simple-queue-sim` is the C++ implementation of the Objective B single-queue simulator.

It models one FIFO waiting queue with one or more servers.

The overall flow is:

```text
┌─────────────────┐     ┌────────────────────┐
│ source workload │ ──→ │ FIFO waiting queue │
└─────────────────┘     └─────────┬──────────┘
                                  │
                                  ↓
                         ┌──────────────────┐
                         │     servers      │
                         │ 0, 1, ...        │
                         └────────┬─────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │                           │
                    ↓                           ↓
          ┌─────────────────────┐      ┌───────────────────────┐
          │ sink / outgoing     │      │ queue metrics         │
          │ edge                │      │ qmet.csv              │
          └─────────────────────┘      └───────────────────────┘

┌────────────────────────┐      state changes      ┌─────────────┐
│ queue + server events  │ ──────────────────────→ │ event log   │
└────────────────────────┘                         │ job.log     │
                                                   └─────────────┘
```

The design document is here:

[`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)

The Obj-B examples are here:

[`/examples/obj-b/README.md`](/examples/obj-b/README.md)

---

## 1. Inputs

The simulator receives:

1. a queue configuration file
2. the source workload named by that configuration
3. optional command-line settings such as a time limit and output filenames

### Queue Configuration

The configuration must contain a header and exactly one non-empty queue-system row:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,in.wl,out.wl,[],[]
```

The columns mean:

|         Column         | Meaning                                       |
| :--------------------: | :-------------------------------------------- |
|     `Queue sys ID`     | queue-system name used in metrics and events  |
|    `Max Queue Size`    | maximum number of jobs waiting for a server   |
|     `# of servers`     | number of jobs that may be in service at once |
|   `Server Function`    | `Sub-X`, a chain, or a repeating chain        |
| `Server Function Rate` | timesteps used by each server-function step   |
|        `source`        | source workload filename                      |
|         `sink`         | completed-job workload filename               |
|       `Edges in`       | incoming edge names                           |
|      `Edges out`       | outgoing edge names                           |

`Max Queue Size` counts waiting jobs only. Jobs inside servers are not included.

Source and sink paths are resolved relative to the configuration file.

### Source Workload

The source workload uses:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 20', '1: W 10']"
j1,2,"['0: W 10']"
```

Jobs are sorted by arrival time when the file is loaded. If two jobs have the same arrival time, their file order is kept.

---

## 2. Outputs

The simulator produces up to four kinds of output.

### Terminal Summary

A successful run prints a short summary:

```txt
simulation complete: <time steps> time steps, <sink jobs> sink jobs, <errors> errors
```

### Sink Workload

If the configuration has a sink, completed jobs are written there:

```csv
Job Id,arrival time,Tasks
j0,20,[]
```

For sink jobs:

```txt
arrival time = time the job left the queue system
Tasks = []
```

### Queue Metrics

The default metrics file is:

```txt
qmet.csv
```

Its header is:

```csv
time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,num-of-errors
```

One row is written at every simulated timestep.

The columns mean:

|        Column         | Meaning                                      |
| :-------------------: | :------------------------------------------- |
|      `time-step`      | current simulation time                      |
|    `queue-sys-id`     | queue ID from the configuration              |
|  `num-jobs-in-queue`  | number of waiting jobs                       |
| `num-jobs-in-service` | number of busy servers                       |
|    `num-of-errors`    | cumulative queue-full and no-out-edge errors |

### Event Log

The default event file is:

```txt
job.log
```

Possible event forms are:

```txt
<time> <job-id> IN <qsid> ARRIVE-VIA <source-or-edge>
<time> <job-id> IN <qsid> DEPART-VIA <sink-or-edge>
<time> <job-id> IN <qsid> ENTERS-SERVER <server-id>
<time> <job-id> IN <qsid> EXITS-SERVER <server-id>
<time> <job-id> IN <qsid> TASK-START <task-id> "W <work>"
<time> <job-id> IN <qsid> TASK-STOP <task-id> "W <work>"
<time> <job-id> IN <qsid> ERROR QUEUE-FULL
<time> <job-id> IN <qsid> ERROR NO-OUT-EDGE
```

---

## 3. Build

From this directory:

```bash
make
```

or:

```bash
make build
```

From the repository root:

```bash
make -C implementation/obj-b/simple-queue-sim
```

or:

```bash
make -C implementation/obj-b/simple-queue-sim build
```

The executable is:

```txt
implementation/obj-b/simple-queue-sim/simple-queue-sim
```

---

## 4. Clean

From this directory:

```bash
make clean
```

From the repository root:

```bash
make -C implementation/obj-b/simple-queue-sim clean
```

---

## 5. Run Until the Queue Drains

The shortest run form is:

```bash
./simple-queue-sim CONFIG
```

From the repository root:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv
```

With no time limit, the simulator continues while any of these remain:

```txt
source jobs that have not arrived yet
waiting jobs
busy servers
```

When all three are empty, the run ends.

The default command-line output files are:

```txt
qmet.csv
job.log
```

---

## 6. Run With a Positional Time Limit

A time limit may be placed directly after the configuration path:

```bash
./simple-queue-sim CONFIG TIME_LIMIT
```

Example:

```bash
./simple-queue-sim queue-config.csv 100
```

A limit of `100` processes:

```txt
t = 0 through 99
```

because the simulator continues while:

```txt
current_time_step < time_limit
```

---

## 7. Run With `--time-limit`

The same limit can be written as a named option:

```bash
./simple-queue-sim queue-config.csv --time-limit 100
```

From the repository root:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --time-limit 100
```

Use one time-limit style in a command so the run is easy to read.

A time limit may end the simulation while jobs are still waiting, still in service, or still waiting to arrive from the source file.

---

## 8. Choose the Metrics File

Use:

```bash
--metrics FILE
```

Example:

```bash
./simple-queue-sim queue-config.csv \
    --metrics output/qmet.csv
```

If omitted, the default is:

```txt
qmet.csv
```

The metrics path is interpreted from the current working directory.

---

## 9. Choose the Event Log

Use:

```bash
--events FILE
```

Example:

```bash
./simple-queue-sim queue-config.csv \
    --events output/job.log
```

If omitted, the default is:

```txt
job.log
```

The event-log path is interpreted from the current working directory.

---

## 10. Use All Run Options Together

Without a time limit:

```bash
./simple-queue-sim queue-config.csv \
    --metrics output/qmet.csv \
    --events output/job.log
```

With a positional time limit:

```bash
./simple-queue-sim queue-config.csv 100 \
    --metrics output/qmet.csv \
    --events output/job.log
```

With a named time limit:

```bash
./simple-queue-sim queue-config.csv \
    --time-limit 100 \
    --metrics output/qmet.csv \
    --events output/job.log
```

The options may appear in a different order after the configuration path, but the configuration file must be the first normal argument.

---

## 11. Help

From this directory:

```bash
./simple-queue-sim --help
```

or:

```bash
./simple-queue-sim -h
```

From the repository root:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim --help
```

---

## 12. Server Functions

The simulator supports `Sub-X`.

### One Function

```txt
Sub-10
```

If the first task is:

```txt
0: W 20
```

then one completed server-function step changes it to:

```txt
0: W 10
```

If the first task is:

```txt
0: W 10
```

then `Sub-10` removes that task.

### Chained Functions

```txt
Sub-5,Sub-10
```

The server performs `Sub-5`, then `Sub-10`.

Each function step takes the configured `Server Function Rate` number of timesteps.

Because the field contains a comma, quote it in CSV:

```csv
"Sub-5,Sub-10"
```

### Repeating Functions

A trailing `*` repeats the full chain while tasks remain:

```txt
Sub-10*
```

or:

```txt
Sub-5,Sub-10*
```

For a server that should keep working until every task is gone, a repeating function is the normal choice.

---

## 13. Source, Sink, and Edge Behavior

A job with tasks normally goes to a server first.

A job with no tasks can be routed immediately.

When a job is ready to leave:

```text
┌────────────────────────┐
│ Tasks == []            │
│ and sink exists        │
└───────────┬────────────┘
            │
            ↓
      ┌────────────┐
      │    sink    │
      └────────────┘
```

Otherwise, if an outgoing edge exists:

```text
┌─────┐     ┌───────────────┐
│ job │ ──→ │ outgoing edge │
└─────┘     └───────────────┘
```

If neither route exists:

```txt
ERROR NO-OUT-EDGE
```

With several outgoing edges, the simulator uses them in round-robin order.

`Edges in` is read from the configuration, but this Objective B simulator does not run other queue systems that send jobs over those edges. That connection is part of the later multi-queue work.

---

## 14. Queue-Full Behavior

Suppose the configuration uses:

```txt
Max Queue Size = 1
# of servers = 1
```

and several jobs arrive while the server is busy.

The first waiting job uses the one queue slot. A later job that also needs to wait receives:

```txt
ERROR QUEUE-FULL
```

The error count increases and the job is dropped.

---

## 15. Timestep Order

Each timestep uses this order:

```txt
1. advance work already in service
2. finish any server-function steps that reached zero
3. route jobs that leave servers
4. move waiting FIFO jobs into idle servers
5. add source jobs arriving at this timestep
6. send a new arrival directly to an idle server when possible
7. otherwise queue it or record QUEUE-FULL
8. write the metrics row
9. advance time
```

A job that enters a server at time `t` starts counting down on the next timestep.

---

## 16. Example Manual Run

Create `in-basic.wl`:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 20', '1: W 10']"
j1,2,"['0: W 10']"
```

Create `queue-config.csv` in the same directory:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,2,in-basic.wl,out-basic.wl,[],[]
```

Run:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --metrics qmet.csv \
    --events job.log
```

The files are connected like this:

```text
                    ┌──────────────────┐
                    │ queue-config.csv │
                    └────────┬─────────┘
                             │
                             ↓
┌─────────────┐     ┌──────────────────┐
│ in-basic.wl │ ──→ │ simple-queue-sim │
└─────────────┘     └────────┬─────────┘
                             │
                  ┌──────────┼──────────┐
                  │          │          │
                  ↓          ↓          ↓
          ┌──────────────┐ ┌──────────┐ ┌─────────┐
          │ out-basic.wl │ │ qmet.csv │ │ job.log │
          └──────────────┘ └──────────┘ └─────────┘
```

Remember that `in-basic.wl` and `out-basic.wl` are relative to `queue-config.csv`, while `qmet.csv` and `job.log` are relative to the directory where the command is run.

---

## 17. Tests

### Full Test Suite

From this directory:

```bash
make test
```

From the repository root:

```bash
make -C implementation/obj-b/simple-queue-sim test
```

### Coverage

```bash
make run-tests-coverage
```

or from the repository root:

```bash
make -C implementation/obj-b/simple-queue-sim run-tests-coverage
```

### Leak Test

```bash
make run-tests-leak
```

or:

```bash
make -C implementation/obj-b/simple-queue-sim run-tests-leak
```

The Makefile test targets require Valgrind.

### Run the Shell Script Directly

```bash
./run-all-tests.sh
./run-all-tests.sh all
./run-all-tests.sh coverage
./run-all-tests.sh leak
```

From the repository root:

```bash
implementation/obj-b/simple-queue-sim/run-all-tests.sh
implementation/obj-b/simple-queue-sim/run-all-tests.sh all
implementation/obj-b/simple-queue-sim/run-all-tests.sh coverage
implementation/obj-b/simple-queue-sim/run-all-tests.sh leak
```

---

## 18. Files in This Directory

```txt
include/queue_sim.hpp
    public data structures and simulator functions

src/queue_sim.cpp
    workload/config parsing and simulation logic

src/main.cpp
    command-line interface

tests/test_queue_sim.cpp
    automated tests

run-all-tests.sh
    test, coverage, and Valgrind runner

Makefile
    build, clean, and test targets
```

---

## 19. Connection to Obj-A

The Obj-A generator creates the exact workload format used by the Obj-B source:

```text
┌──────────────────────────┐
│ Obj-A workload-generator │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│      generated.csv       │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│     simple-queue-sim     │
└──────────────────────────┘
```

After Obj-B finishes, Obj-A's visualizer can read the sink file:

```text
┌──────────────────────────┐
│   Obj-B sink workload    │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│ Obj-A workload-visualizer│
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│         plot.png         │
└──────────────────────────┘
```

For the complete combined walkthrough, see:

[`/README.md#5-running-obj-a-and-obj-b-together`](/README.md#5-running-obj-a-and-obj-b-together)

For the simulator design, see:

[`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)
