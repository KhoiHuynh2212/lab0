# 🔬 Lab 0

This lab builds a queueing-system simulator and the tools around it. The repository is organized around Objectives A through E so workload generation, simulation, analysis, modelling, and multi-queue work can all live in one place.

The main idea is:

```txt
workloads → queue simulation → metrics/events → analysis → modelling
```

Each objective has its own section below. As more parts of the lab are added, their commands, examples, and documentation should be added under the matching objective instead of creating a separate top-level guide.

- [🔬 Lab 0](#-lab-0)
- [🏁 Quick Start](#-quick-start)
  - [Build and Test Objective A](#build-and-test-objective-a)
  - [Build and Test Objective B](#build-and-test-objective-b)
  - [Build and Test Objective C](#build-and-test-objective-c)
  - [Review Objective D](#review-objective-d)
  - [Build and Test Objective E](#build-and-test-objective-e)
  - [Clean Built Components](#clean-built-components)
- [🌳 Directory](#-directory)
- [🧰 Objective A: Workloads](#-objective-a-workloads)
  - [Workload Format](#workload-format)
  - [Objective A Documentation](#objective-a-documentation)
  - [Generate a Workload](#generate-a-workload)
  - [Make a Histogram](#make-a-histogram)
  - [Make a Scatter Plot](#make-a-scatter-plot)
  - [Objective A Inputs and Outputs](#objective-a-inputs-and-outputs)
- [🧵 Objective B: Simple Queueing System](#-objective-b-simple-queueing-system)
  - [Objective B Inputs](#objective-b-inputs)
  - [Objective B Outputs](#objective-b-outputs)
  - [Objective B Documentation](#objective-b-documentation)
  - [Queue Configuration Format](#queue-configuration-format)
  - [Run Until the Queue Drains](#run-until-the-queue-drains)
  - [Run With a Time Limit](#run-with-a-time-limit)
  - [Choose Metrics and Event Files](#choose-metrics-and-event-files)
  - [Objective B Flow](#objective-b-flow)
- [🧪 Objective C: Analysis Tooling](#-objective-c-analysis-tooling)
  - [Objective C Tool Groups](#objective-c-tool-groups)
  - [Objective C Documentation](#objective-c-documentation)
  - [Objective C Commands](#objective-c-commands)
- [📈 Objective D: Single Queue Modelling and Analysis](#-objective-d-single-queue-modelling-and-analysis)
  - [Objective D Questions](#objective-d-questions)
  - [Objective D Documentation](#objective-d-documentation)
- [🌐 Objective E: Multi Queueing System](#-objective-e-multi-queueing-system)
  - [Objective E Main Pieces](#objective-e-main-pieces)
  - [Objective E Commands](#objective-e-commands)
  - [Objective E Analysis](#objective-e-analysis)
  - [Objective E Documentation](#objective-e-documentation)
- [🔄 How the Objectives Connect](#-how-the-objectives-connect)
  - [Example: Objective A into Objective B](#example-objective-a-into-objective-b)
    - [Step 1: Build Both Programs](#step-1-build-both-programs)
    - [Step 2: Generate a Source Workload](#step-2-generate-a-source-workload)
    - [Step 3: Create a Queue Configuration](#step-3-create-a-queue-configuration)
    - [Step 4: Run Objective B](#step-4-run-objective-b)
    - [Step 5: Visualize a Workload](#step-5-visualize-a-workload)
- [🧾 Build and Test Command Reference](#-build-and-test-command-reference)
  - [Objective A as a Whole](#objective-a-as-a-whole)
  - [Workload Generator Only](#workload-generator-only)
  - [Workload Visualizer Only](#workload-visualizer-only)
  - [Objective B Simple Queue Simulator](#objective-b-simple-queue-simulator)
  - [Objective C Tools](#objective-c-tools)
  - [Objective D Analysis](#objective-d-analysis)
  - [Objective E Multi Queue Tools](#objective-e-multi-queue-tools)
- [📚 Documentation](#-documentation)
- [🔗 Links](#-links)
- [👥 Group 2](#-group-2)

---

# 🏁 Quick Start

All commands in this README are written as if they are run from the repository root:

```txt
lab0/
```

## Build and Test Objective A

```bash
make -C implementation/obj-a build
make -C implementation/obj-a test
```

This builds the workload generator and workload visualizer. The test suite also has coverage and Valgrind modes; see the [Objective A command reference](#objective-a-as-a-whole).

## Build and Test Objective B

```bash
make -C implementation/obj-b/simple-queue-sim build
make -C implementation/obj-b/simple-queue-sim test
```

This builds `implementation/obj-b/simple-queue-sim/simple-queue-sim`. The test target uses Valgrind.

## Build and Test Objective C

Objective C has one Makefile per tool. Build or test the tool you are working on from the repository root:

```bash
make -C implementation/obj-c/compute-use build
make -C implementation/obj-c/derive-sys-metrics build
make -C implementation/obj-c/derive-event-metrics build
make -C implementation/obj-c/plot-sys-metrics build
```

The same pattern works for every Objective C directory. A complete list is in [Objective C Commands](#objective-c-commands) and [Objective C Tools](#objective-c-tools).

## Review Objective D

The completed single-queue car-wash study is documented in:

- [Objective D analysis](/analysis/simple/README.md)
- [Objective D results guide](/analysis/simple/results/analysis/README.md)

The Objective D unit tests can be run with:

```bash
python -m unittest discover -s analysis/simple/tests -v
```

## Build and Test Objective E

Objective E has one top-level Makefile for the multi-queue simulator and its Python tools:

```bash
make -C implementation/obj-e build
make -C implementation/obj-e test
```

The build produces:

```txt
implementation/obj-e/multi-queue-sim/multi-queue-sim
```

The full Objective E test target runs the C++ simulator tests through Valgrind and then runs the Python tool tests.

## Clean Built Components

```bash
make -C implementation/obj-a clean
make -C implementation/obj-b/simple-queue-sim clean

for tool in \
    compute-use create-event-trace derive-event-metrics derive-sys-metrics \
    filter-by-job filter-by-queue plot-derive-event-metrics plot-sys-metrics plot-use
do
    make -C "implementation/obj-c/$tool" clean
done

make -C implementation/obj-e clean
```

---

# 🌳 Directory

The repository keeps designs, examples, implementations, and analysis separate so each objective can grow without putting everything in one directory.

```txt
.
├── .gitlab-ci.yml
├── analysis
│   ├── multi-LARGE
│   ├── multi-MEDIUM
│   ├── multi-SMALL
│   ├── multi-common
│   └── simple
├── designs
│   ├── obj-a
│   │   ├── workload-generator
│   │   └── workload-visualizer
│   ├── obj-b
│   │   └── simple-queue-sim
│   ├── obj-c
│   │   ├── compute-use
│   │   ├── create-event-trace
│   │   ├── derive-event-metrics
│   │   ├── derive-sys-metrics
│   │   ├── filter-by-job
│   │   ├── filter-by-queue
│   │   ├── plot-derive-event-metrics
│   │   ├── plot-sys-metrics
│   │   └── plot-use
│   └── obj-e
│       ├── create-config-from-network
│       ├── multi-queue-sim
│       └── pretty-print-events
├── examples
│   ├── obj-a
│   ├── obj-b
│   ├── obj-c
│   └── obj-e
├── implementation
│   ├── obj-a
│   │   ├── workload-generator
│   │   └── workload-visualizer
│   ├── obj-b
│   │   └── simple-queue-sim
│   │       ├── run-all-tests.sh
│   │       └── simple-queue-sim
│   ├── obj-c
│   │   ├── compute-use
│   │   ├── create-event-trace
│   │   ├── derive-event-metrics
│   │   ├── derive-sys-metrics
│   │   ├── filter-by-job
│   │   ├── filter-by-queue
│   │   ├── plot-derive-event-metrics
│   │   ├── plot-sys-metrics
│   │   └── plot-use
│   └── obj-e
│       ├── create-config-from-network
│       ├── multi-queue-sim
│       │   └── run-all-tests.sh
│       └── pretty-print-events
├── lab
│   ├── docs
│   │   └── outline.pdf
│   └── run-all-tests.sh
└── README.md
```

The top-level directories have these jobs:

|     Directory      | Purpose                                                      |
| :----------------: | :----------------------------------------------------------- |
|    `/analysis/`    | Results and analysis from queue experiments.                 |
|    `/designs/`     | Pseudocode, design notes, formats, and worked examples.      |
|    `/examples/`    | Example input files, output files, and small demonstrations. |
| `/implementation/` | Programs, Python tools, Makefiles, and automated tests.      |
|      `/lab/`       | Lab-wide scripts and documents.                              |

> Remove `.gitkeep` files after a blank directory begins containing real files.
>
> Remove `TODO.md` files after every task inside them is complete.

---

# 🧰 Objective A: Workloads

Objective A covers the workload that enters the queueing system and the tools used to look at workload files.

It contains two C++ programs:

1. **Workload Generator**: creates synthetic workload files.
2. **Workload Visualizer**: reads workload files and creates PNG plots.

## Workload Format

A workload contains a job ID, an arrival time, and a list of tasks:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

A task has the form:

```txt
<task-id>: W <positive work amount>
```

For example:

```txt
0: W 10
```

means task `0` has `10` units of work.

An empty task list means the job is done:

```txt
[]
```

## Objective A Documentation

- [Obj-A overview](/implementation/obj-a/README.md)
- [Workload Generator run guide](/implementation/obj-a/workload-generator/README.md)
- [Workload Visualizer run guide](/implementation/obj-a/workload-visualizer/README.md)
- [Obj-A design overview](/designs/obj-a/README.md)
- [Workload Generator design](/designs/obj-a/workload-generator/README.md)
- [Workload Visualizer design](/designs/obj-a/workload-visualizer/README.md)
- [Obj-A example files](/examples/obj-a/README.md)

## Generate a Workload

A small uniform example:

```bash
implementation/obj-a/workload-generator/workload-generator \
    --out generated.csv \
    --interval 0,20 \
    --distribution uniform \
    --min 0 \
    --max 2 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 7
```

The workload generator supports:

```txt
uniform
normal
geometric
poisson
```

For every generator option and examples of every distribution, see the [Workload Generator run guide](/implementation/obj-a/workload-generator/README.md).

## Make a Histogram

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    generated.csv \
    --hist="arrival time" \
    --bins=10 \
    --out=arrival-times.png
```

## Make a Scatter Plot

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    generated.csv \
    --scat="arrival time,len(Tasks)" \
    --out=arrival-vs-tasks.png
```

The visualizer also supports titles, axis limits, and custom image sizes. See the [Workload Visualizer run guide](/implementation/obj-a/workload-visualizer/README.md) for the complete command list.

## Objective A Inputs and Outputs

```txt
Generator command-line options
            │
            ↓
     workload file
       ╱         ╲
      ↓           ↓
Obj-B source   visualizer
                  │
                  ↓
               PNG plot
```

The generated workload is designed to be used directly as a source workload for the queue simulators later in the lab.

---

# 🧵 Objective B: Simple Queueing System

Objective B is the simple queue simulator. It models one queueing system with a waiting queue, one or more servers, a source, a sink, and optional outgoing edges.

## Objective B Inputs

The simulator mainly reads:

1. a queue configuration CSV
2. a source workload file named by that configuration

A configuration can describe:

- the queue system ID
- maximum waiting-queue size
- number of servers
- server function
- server function rate
- source workload
- sink workload
- incoming edges
- outgoing edges

## Objective B Outputs

A run can create:

1. a sink workload containing completed jobs
2. a queue metrics CSV
3. an event log

The simulator also tracks errors such as a full queue or a job that has no usable outgoing edge.

## Objective B Documentation

- [Obj-B overview](/implementation/obj-b/README.md)
- [Simple Queue Simulator run guide](/implementation/obj-b/simple-queue-sim/README.md)
- [Simple Queue Simulator design](/designs/obj-b/simple-queue-sim/README.md)
- [Obj-B example files](/examples/obj-b/)
- [Example source workload](/examples/obj-b/in0.wl)
- [Example queue configuration](/examples/obj-b/simple-queue-config-demo.csv)
- [Example queue metrics](/examples/obj-b/qmet-demo.csv)
- [Example event log](/examples/obj-b/events-demo.log)

## Queue Configuration Format

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,in.wl,out.wl,[],[]
```

For this example:

```txt
Queue ID:             q0
Waiting queue size:   8 jobs
Servers:              1
Server function:      Sub-10*
Function rate:        10 timesteps
Source workload:      in.wl
Sink workload:        out.wl
Incoming edges:       none
Outgoing edges:       none
```

## Run Until the Queue Drains

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv
```

With no time limit, the simulator continues until all source jobs have arrived and no jobs remain in the waiting queue or servers.

## Run With a Time Limit

Positional form:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv 100
```

Named-option form:

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --time-limit 100
```

## Choose Metrics and Event Files

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --metrics qmet.csv \
    --events job.log
```

If those options are omitted, the default files are:

```txt
qmet.csv
job.log
```

The complete input/output formats, server-function rules, event ordering, and command-line forms are in the [Simple Queue Simulator run guide](/implementation/obj-b/simple-queue-sim/README.md).

## Objective B Flow

```txt
source workload ─↘︎
                   ↘︎
queue configuration → simple-queue-sim → sink workload
                                   │
                                   ├───→ qmet.csv
                                   │
                                   └───→ job.log
```

---

# 🧪 Objective C: Analysis Tooling

Objective C contains the tools used to filter simulator output and turn queue metrics and event logs into information that is easier to analyze.

Objective C contains these tools:

```txt
implementation/obj-c/
├── compute-use
├── create-event-trace
├── derive-event-metrics
├── derive-sys-metrics
├── filter-by-job
├── filter-by-queue
├── plot-derive-event-metrics
├── plot-sys-metrics
└── plot-use
```

## Objective C Tool Groups

The tools fall into a few main groups:

- **Filtering**: keep only selected queues or jobs.
- **USE metrics**: work with utilization, saturation, and errors.
- **System metrics**: derive response-time and throughput information from events.
- **Job metrics**: derive queue/service timing for individual jobs.
- **Plotting and tracing**: turn the derived information into plots and event traces.

## Objective C Documentation

Start with the [Objective C design overview](/designs/obj-c/README.md), which shows how `qmet.csv` and `job.log` feed the nine analysis tools.

| Tool                        |                        Implementation guide                         |                            Design                            |
| :-------------------------- | :-----------------------------------------------------------------: | :----------------------------------------------------------: |
| `filter-by-queue`           |      [README](/implementation/obj-c/filter-by-queue/README.md)      |      [Design](/designs/obj-c/filter-by-queue/README.md)      |
| `filter-by-job`             |       [README](/implementation/obj-c/filter-by-job/README.md)       |       [Design](/designs/obj-c/filter-by-job/README.md)       |
| `compute-use`               |        [README](/implementation/obj-c/compute-use/README.md)        |        [Design](/designs/obj-c/compute-use/README.md)        |
| `plot-use`                  |         [README](/implementation/obj-c/plot-use/README.md)          |         [Design](/designs/obj-c/plot-use/README.md)          |
| `derive-sys-metrics`        |    [README](/implementation/obj-c/derive-sys-metrics/README.md)     |    [Design](/designs/obj-c/derive-sys-metrics/README.md)     |
| `plot-sys-metrics`          |     [README](/implementation/obj-c/plot-sys-metrics/README.md)      |     [Design](/designs/obj-c/plot-sys-metrics/README.md)      |
| `derive-event-metrics`      |   [README](/implementation/obj-c/derive-event-metrics/README.md)    |   [Design](/designs/obj-c/derive-event-metrics/README.md)    |
| `plot-derive-event-metrics` | [README](/implementation/obj-c/plot-derive-event-metrics/README.md) | [Design](/designs/obj-c/plot-derive-event-metrics/README.md) |
| `create-event-trace`        |    [README](/implementation/obj-c/create-event-trace/README.md)     |    [Design](/designs/obj-c/create-event-trace/README.md)     |

Small CSV inputs for the plotting tools are in [`/examples/obj-c/`](/examples/obj-c/).

## Objective C Commands

Each tool can be built from the repository root with `make -C implementation/obj-c/<tool> build`. The most common run forms are:

|            Tool             | Common command                                                                                                                   |
| :-------------------------: | :------------------------------------------------------------------------------------------------------------------------------- |
|      `filter-by-queue`      | `implementation/obj-c/filter-by-queue/filter-by-queue qmet.csv q0.csv --queues=q0`                                               |
|       `filter-by-job`       | `implementation/obj-c/filter-by-job/filter-by-job job.log j0.log --jobs=j0`                                                      |
|        `compute-use`        | `implementation/obj-c/compute-use/compute-use qmet.csv quse.csv`                                                                 |
|         `plot-use`          | `implementation/obj-c/plot-use/plot-use quse.csv use-plots`                                                                      |
|    `derive-sys-metrics`     | `implementation/obj-c/derive-sys-metrics/derive-sys-metrics job.log sysmetric.csv`                                               |
|     `plot-sys-metrics`      | `implementation/obj-c/plot-sys-metrics/plot-sys-metrics sysmetric.csv system.png`                                                |
|   `derive-event-metrics`    | `implementation/obj-c/derive-event-metrics/derive-event-metrics job.log emetric.csv`                                             |
| `plot-derive-event-metrics` | `implementation/obj-c/plot-derive-event-metrics/plot-derive-event-metrics emetric.csv wait.png --hist="response-time" --bins=20` |
|    `create-event-trace`     | `implementation/obj-c/create-event-trace/create-event-trace job.log trace.png`                                                   |

The C++ tools provide coverage and leak-test targets where their Makefiles define them. The Python plotting/tracing tools use `build`, `test`, and `clean` targets and require `matplotlib`.

---

# 📈 Objective D: Single Queue Modelling and Analysis

Objective D uses the tools from the earlier objectives to model and study a real process as a single queue.

The work for this objective belongs mainly in:

```txt
analysis/simple/
```

The general process is:

```txt
pick a real process
        │
        ↓
map it to a queue
        │
        ↓
create workload + queue configuration
        │
        ↓
run the simulator
        │
        ↓
analyze USE + throughput + response time
        │
        ↓
change resources and compare results
```

## Objective D Questions

This section should grow to document:

- what real-world process is being modelled
- how that process maps to the simulator
- which metrics matter and why
- how the workload is generated
- how the queue configuration is chosen
- how the system behaves at different scales
- how changes in resources affect the results
- what conclusions can be drawn from the experiments

## Objective D Documentation

The current single-queue study models a car wash with one FIFO waiting line and one or more wash bays. The analysis documents the model assumptions, workload generation, experiment matrix, validation, Objective C outputs, plots, and sensitivity study.

- [Objective D: Car Wash Modeling and Analysis](/analysis/simple/README.md)
- [Saved analysis results and interpretation](/analysis/simple/results/analysis/README.md)
- [Experiment matrix](/analysis/simple/experiments.csv)
- [Configuration template](/analysis/simple/configs/carwash-template.csv)

The analysis README contains the reproduction command and explains which results are measured directly, which values are derived, and where the supplied Objective C tools have measurement limitations.

---

# 🌐 Objective E: Multi Queueing System

Objective E extends the queueing work to multiple connected queue systems.

Objective E is organized as:

```txt
designs/obj-e/
├── create-config-from-network
├── multi-queue-sim
└── pretty-print-events

implementation/obj-e/
├── create-config-from-network
├── multi-queue-sim
└── pretty-print-events

examples/obj-e/
```

## Objective E Main Pieces

- **Multi Queue Simulator**: runs all queue-system rows together and routes jobs over configured edges.
- **Event Pretty Printer**: lays out `job.log` by time and queue so multi-queue behavior is easier to follow.
- **Network Config Generator**: converts a SNAP-style edge list or Matrix Market `.mtx` file into a queue configuration CSV.

## Objective E Commands

Build and test the Objective E tools:

```bash
make -C implementation/obj-e build
make -C implementation/obj-e test
```

Generate a queue configuration from the included network example:

```bash
python3 implementation/obj-e/create-config-from-network/create-config-from-network.py     examples/obj-e/network-demo.mtx     qconf.csv
```

Run the multi-queue simulator with a configuration whose source workload files exist:

```bash
implementation/obj-e/multi-queue-sim/multi-queue-sim     qconf.csv     --metrics qmet.csv     --events job.log
```

The simulator also accepts an optional positional time limit or `--time-limit N`. Source and sink paths in the configuration are resolved relative to the configuration file.

Pretty-print an event log:

```bash
python3 implementation/obj-e/pretty-print-events/pretty-print-events.py     job.log     --legend
```

Use `--help` with either Python tool to see its full option list. The simulator's command form is:

```txt
multi-queue-sim CONFIG [TIME_LIMIT] [--time-limit N] [--metrics FILE] [--events FILE]
```

## Objective E Analysis

Multi-queue analysis is split by scale, with shared workflow material in `multi-common`:

- [Shared multi-queue workflow](/analysis/multi-common/README.md)
- [SMALL-scale study](/analysis/multi-SMALL/README.md)
- [MEDIUM-scale study](/analysis/multi-MEDIUM/README.md)
- [LARGE-scale study](/analysis/multi-LARGE/README.md)

## Objective E Documentation

- [Objective E design overview](/designs/obj-e/README.md)
- [Multi Queue Simulator design](/designs/obj-e/multi-queue-sim/README.md)
- [Event Pretty Printer design](/designs/obj-e/pretty-print-events/README.md)
- [Network Config Generator design](/designs/obj-e/create-config-from-network/README.md)
- [Objective E implementation directory](/implementation/obj-e/)
- [Objective E example network](/examples/obj-e/network-demo.mtx)

The network generator feeds the simulator configuration, the simulator produces `qmet.csv` and `job.log`, and the pretty printer is a debugging view of the event log:

```txt
┌────────────────────┐
│ network edge list  │
└─────────┬──────────┘
          ↓
┌────────────────────┐
│ create-config-     │
│ from-network       │
└─────────┬──────────┘
          ↓
┌────────────────────┐
│ queue config CSV   │
└─────────┬──────────┘
          │ + source workloads
          ↓
┌────────────────────┐
│ multi-queue-sim    │
└──────┬─────────┬───┘
       ↓         ↓
   qmet.csv    job.log
                 │
                 ↓
        ┌────────────────────┐
        │ pretty-print-events│
        └────────────────────┘
```

---

# 🔄 How the Objectives Connect

The objectives are meant to pass files and results to each other instead of acting as unrelated programs.

A useful high-level view is:

```txt
                    Objective A
          workload-generator / visualizer
                       │
                       │ workload
                       ↓
              ┌──────────────────┐
              │                  │
              ↓                  ↓
         Objective B         Objective E
        simple queue        multi queue
              │                  │
              └──────────────────┘
                       |
          ┌────────────┴────────────┐
          │                         │
          ↓                         ↓
       qmet.csv                   job.log
          │                         │
          └────────────┬────────────┘
                       │
                       ↓
                  Objective C
              analysis + plotting
                       │
                       ↓
                  Objective D
             modelling + conclusions
```

The workload visualizer from Objective A can also be used before or after a simulation to inspect source and sink workload files.

## Example: Objective A into Objective B

### Step 1: Build Both Programs

```bash
make -C implementation/obj-a build
make -C implementation/obj-b/simple-queue-sim build
```

### Step 2: Generate a Source Workload

```bash
implementation/obj-a/workload-generator/workload-generator \
    --out generated.csv \
    --interval 0,5 \
    --distribution uniform \
    --min 1 \
    --max 1 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 1
```

This creates one job at every timestep from `0` through `5`.

### Step 3: Create a Queue Configuration

Create `queue-config.csv` in the repository root:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,1,generated.csv,completed.csv,[],[]
```

Source and sink paths are resolved relative to the configuration file, so this configuration reads `generated.csv` from the repository root and writes `completed.csv` there.

### Step 4: Run Objective B

```bash
implementation/obj-b/simple-queue-sim/simple-queue-sim \
    queue-config.csv \
    --metrics qmet.csv \
    --events job.log
```

After the run:

```txt
generated.csv   Objective A source workload
completed.csv   Objective B completed-job workload
qmet.csv        queue metrics
job.log         job events
```

### Step 5: Visualize a Workload

Source arrival times:

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    generated.csv \
    --hist="arrival time" \
    --out=source-arrivals.png
```

Completion times from the sink:

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    completed.csv \
    --hist="arrival time" \
    --out=completion-times.png
```

In a completed sink workload, the `arrival time` column stores the time the job left the queue system.

---

# 🧾 Build and Test Command Reference

These commands are run from the repository root.

## Objective A as a Whole

| Command                                           | Description                                        |
| :------------------------------------------------ | :------------------------------------------------- |
| `make -C implementation/obj-a`                    | Build both Objective A tools.                      |
| `make -C implementation/obj-a build`              | Clean and build both Objective A tools.            |
| `make -C implementation/obj-a clean`              | Remove Objective A build/test files.               |
| `make -C implementation/obj-a test`               | Run the complete Objective A tests.                |
| `make -C implementation/obj-a run-tests-coverage` | Run Objective A tests and create coverage reports. |
| `make -C implementation/obj-a run-tests-leak`     | Run Objective A leak tests with Valgrind.          |
| `implementation/obj-a/run-all-tests.sh`           | Run the Objective A test script directly.          |
| `implementation/obj-a/run-all-tests.sh coverage`  | Run coverage mode directly.                        |
| `implementation/obj-a/run-all-tests.sh leak`      | Run leak mode directly.                            |

## Workload Generator Only

| Command                                                              | Description                              |
| :------------------------------------------------------------------- | :--------------------------------------- |
| `make -C implementation/obj-a/workload-generator`                    | Build the generator.                     |
| `make -C implementation/obj-a/workload-generator build`              | Clean and build the generator.           |
| `make -C implementation/obj-a/workload-generator clean`              | Remove generator build/test files.       |
| `make -C implementation/obj-a/workload-generator test`               | Run generator tests.                     |
| `make -C implementation/obj-a/workload-generator run-tests-coverage` | Run generator coverage tests.            |
| `make -C implementation/obj-a/workload-generator run-tests-leak`     | Run generator leak tests with Valgrind.  |
| `implementation/obj-a/workload-generator/workload-generator --help`  | Show all generator command-line options. |

## Workload Visualizer Only

| Command                                                               | Description                               |
| :-------------------------------------------------------------------- | :---------------------------------------- |
| `make -C implementation/obj-a/workload-visualizer`                    | Build the visualizer.                     |
| `make -C implementation/obj-a/workload-visualizer build`              | Clean and build the visualizer.           |
| `make -C implementation/obj-a/workload-visualizer clean`              | Remove visualizer build/test files.       |
| `make -C implementation/obj-a/workload-visualizer test`               | Run visualizer tests.                     |
| `make -C implementation/obj-a/workload-visualizer run-tests-coverage` | Run visualizer coverage tests.            |
| `make -C implementation/obj-a/workload-visualizer run-tests-leak`     | Run visualizer leak tests with Valgrind.  |
| `implementation/obj-a/workload-visualizer/workload-visualizer --help` | Show all visualizer command-line options. |

## Objective B Simple Queue Simulator

| Command                                                            | Description                               |
| :----------------------------------------------------------------- | :---------------------------------------- |
| `make -C implementation/obj-b/simple-queue-sim`                    | Build the simulator.                      |
| `make -C implementation/obj-b/simple-queue-sim build`              | Clean and build the simulator.            |
| `make -C implementation/obj-b/simple-queue-sim clean`              | Remove simulator build/test files.        |
| `make -C implementation/obj-b/simple-queue-sim test`               | Run the complete Objective B tests.       |
| `make -C implementation/obj-b/simple-queue-sim run-tests-coverage` | Run Objective B coverage tests.           |
| `make -C implementation/obj-b/simple-queue-sim run-tests-leak`     | Run Objective B leak tests with Valgrind. |
| `implementation/obj-b/simple-queue-sim/run-all-tests.sh`           | Run the Objective B test script directly. |
| `implementation/obj-b/simple-queue-sim/run-all-tests.sh coverage`  | Run coverage mode directly.               |
| `implementation/obj-b/simple-queue-sim/run-all-tests.sh leak`      | Run leak mode directly.                   |
| `implementation/obj-b/simple-queue-sim/simple-queue-sim --help`    | Show simulator command-line help.         |

## Objective C Tools

Objective C does not have one umbrella Makefile, so commands are run in the individual tool directories.

|            Tool             | Build                                                          | Test                                                          |
| :-------------------------: | :------------------------------------------------------------- | :------------------------------------------------------------ |
|      `filter-by-queue`      | `make -C implementation/obj-c/filter-by-queue build`           | `make -C implementation/obj-c/filter-by-queue test`           |
|       `filter-by-job`       | `make -C implementation/obj-c/filter-by-job build`             | `make -C implementation/obj-c/filter-by-job test`             |
|        `compute-use`        | `make -C implementation/obj-c/compute-use build`               | `make -C implementation/obj-c/compute-use test`               |
|         `plot-use`          | `make -C implementation/obj-c/plot-use build`                  | `make -C implementation/obj-c/plot-use test`                  |
|    `derive-sys-metrics`     | `make -C implementation/obj-c/derive-sys-metrics build`        | `make -C implementation/obj-c/derive-sys-metrics test`        |
|     `plot-sys-metrics`      | `make -C implementation/obj-c/plot-sys-metrics build`          | `make -C implementation/obj-c/plot-sys-metrics test`          |
|   `derive-event-metrics`    | `make -C implementation/obj-c/derive-event-metrics build`      | `make -C implementation/obj-c/derive-event-metrics test`      |
| `plot-derive-event-metrics` | `make -C implementation/obj-c/plot-derive-event-metrics build` | `make -C implementation/obj-c/plot-derive-event-metrics test` |
|    `create-event-trace`     | `make -C implementation/obj-c/create-event-trace build`        | `make -C implementation/obj-c/create-event-trace test`        |

For C++ Objective C tools that define them, use `run-tests-coverage` and `run-tests-leak` in the same way as `test`.

## Objective D Analysis

| Command                                                                                                     | Description                                               |
| :---------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------- |
| `python -m unittest discover -s analysis/simple/tests -v`                                                   | Run the Objective D analysis tests.                       |
| `python analysis/simple/scripts/analyze_results.py --output-root analysis/simple/.build/analysis-replay-01` | Reproduce the saved analysis into a new output directory. |

See [Objective D analysis](/analysis/simple/README.md) before running the full reproduction workflow; it documents dependencies, saved evidence, and output-directory rules.

## Objective E Multi Queue Tools

| Command                                           | Description                                             |
| :------------------------------------------------ | :------------------------------------------------------ |
| `make -C implementation/obj-e`                    | Build the multi-queue simulator.                        |
| `make -C implementation/obj-e build`              | Rebuild the multi-queue simulator.                      |
| `make -C implementation/obj-e clean`              | Remove Objective E build/test output.                   |
| `make -C implementation/obj-e test`               | Run C++ Valgrind tests and Python tool tests.           |
| `make -C implementation/obj-e run-tests-coverage` | Run C++ gcov and Python coverage when available.        |
| `make -C implementation/obj-e run-tests-leak`     | Run the C++ simulator leak tests.                       |
| `make -C implementation/obj-e check-python`       | Check that both Python tools exist and parse correctly. |
| `make -C implementation/obj-e test-python`        | Run the pretty-printer Python unit tests.               |

For detailed Objective A and Objective B commands, see:

- [Workload Generator run guide](/implementation/obj-a/workload-generator/README.md)
- [Workload Visualizer run guide](/implementation/obj-a/workload-visualizer/README.md)
- [Simple Queue Simulator run guide](/implementation/obj-b/simple-queue-sim/README.md)

---

# 📚 Documentation

This table is the main index for the repository's detailed guides.

| Topic                             | File                                                                                                         |
| :-------------------------------- | :----------------------------------------------------------------------------------------------------------- |
| Objective A overview              | [`/implementation/obj-a/README.md`](/implementation/obj-a/README.md)                                         |
| Workload Generator commands       | [`/implementation/obj-a/workload-generator/README.md`](/implementation/obj-a/workload-generator/README.md)   |
| Workload Visualizer commands      | [`/implementation/obj-a/workload-visualizer/README.md`](/implementation/obj-a/workload-visualizer/README.md) |
| Objective A designs               | [`/designs/obj-a/README.md`](/designs/obj-a/README.md)                                                       |
| Objective A examples              | [`/examples/obj-a/README.md`](/examples/obj-a/README.md)                                                     |
| Objective B overview              | [`/implementation/obj-b/README.md`](/implementation/obj-b/README.md)                                         |
| Simple Queue Simulator commands   | [`/implementation/obj-b/simple-queue-sim/README.md`](/implementation/obj-b/simple-queue-sim/README.md)       |
| Simple Queue Simulator design     | [`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)                     |
| Objective B example files         | [`/examples/obj-b/`](/examples/obj-b/)                                                                       |
| Objective C design overview       | [`/designs/obj-c/README.md`](/designs/obj-c/README.md)                                                       |
| Objective C implementations       | [`/implementation/obj-c/`](/implementation/obj-c/)                                                           |
| Objective C example data          | [`/examples/obj-c/`](/examples/obj-c/)                                                                       |
| Objective D single-queue analysis | [`/analysis/simple/README.md`](/analysis/simple/README.md)                                                   |
| Objective D results guide         | [`/analysis/simple/results/analysis/README.md`](/analysis/simple/results/analysis/README.md)                 |
| Objective E design overview       | [`/designs/obj-e/README.md`](/designs/obj-e/README.md)                                                       |
| Objective E implementations       | [`/implementation/obj-e/`](/implementation/obj-e/)                                                           |
| Objective E network example       | [`/examples/obj-e/network-demo.mtx`](/examples/obj-e/network-demo.mtx)                                       |
| Multi-queue shared workflow       | [`/analysis/multi-common/README.md`](/analysis/multi-common/README.md)                                       |
| SMALL multi-queue study           | [`/analysis/multi-SMALL/README.md`](/analysis/multi-SMALL/README.md)                                         |
| MEDIUM multi-queue study          | [`/analysis/multi-MEDIUM/README.md`](/analysis/multi-MEDIUM/README.md)                                       |
| LARGE multi-queue study           | [`/analysis/multi-LARGE/README.md`](/analysis/multi-LARGE/README.md)                                         |

---

# 🔗 Links

- [Task Sheet](https://1drv.ms/x/c/470c9d6d25c55391/IQCHiMcoylyeQYELoJuQF3yGAQygDJI3jfT833fP3bGEcKg?e=Dsh32I)
- [Final Report](https://overleaf.cs.ou.edu/project/6a98c77302875a2503a2537a)
- [Presentation](https://sooners-my.sharepoint.com/:p:/g/personal/jace_ou_edu/IQAGarZVcGmxSbaPc47I2IXMAax3SK66qyfHr37jMrBJdx8?e=xniUSi)
- [Download GlobalConnect VPN](https://itsupport.ou.edu/TDClient/30/Unified/KB/Article/128/About-OU-VPN-GlobalProtect-Options-for-your-Connection-Type)

---

# 👥 Group 2

| Member |     Name     |   Role    |          Email           |   Discord    |
| :----: | :----------: | :-------: | :----------------------: | :----------: |
|   A    |  Joy Mosisa  |  Manager  |  <joy.j.mosia-1@ou.edu>  | stormify.exe |
|   B    | Jace Rausch  | Tech Lead |      <jace@ou.edu>       |    jace.r    |
|   C    |  Jay Singh   |  Member   | <jasakaran.singh@ou.edu> |  jsinggh512  |
|   D    |  Khoi Huyn   |  Member   | <khoi.t.huynh-1@ou.edu>  |  leokhoi22   |
|   E    | Luc Mwizerwa |  Member   |       <luc@ou.edu>       |    .luc23    |
