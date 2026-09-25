# 🔬 Lab 0

This lab builds a queueing-system simulator and the tools around it. The repository is organized around Objectives A through E so workload generation, simulation, analysis, modelling, and multi-queue work can all live in one place.

The main idea is:

```text
workloads → queue simulation → metrics/events → analysis → modelling
```

Each objective has its own section below. As more parts of the lab are added, their commands, examples, and documentation should be added under the matching objective instead of creating a separate top-level guide.

- [🔬 Lab 0](#-lab-0)
- [🏁 Quick Start](#-quick-start)
  - [Build Objective A](#build-objective-a)
  - [Test Objective A](#test-objective-a)
  - [Build Objective B](#build-objective-b)
  - [Test Objective B](#test-objective-b)
  - [Clean Objective A and Objective B](#clean-objective-a-and-objective-b)
  - [Future Objective Commands](#future-objective-commands)
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
  - [Objectives C, D, and E](#objectives-c-d-and-e)
- [📚 Documentation](#-documentation)
- [🔗 Links](#-links)
- [👥 Group 2](#-group-2)

---

# 🏁 Quick Start

All commands in this README are written as if they are run from the repository root:

```text
lab0/
```

## Build Objective A

```bash
make -C implementation/obj-a build
```

This builds:

```text
implementation/obj-a/workload-generator/workload-generator
implementation/obj-a/workload-visualizer/workload-visualizer
```

## Test Objective A

```bash
make -C implementation/obj-a test
```

## Build Objective B

```bash
make -C implementation/obj-b/simple-queue-sim build
```

This builds:

```text
implementation/obj-b/simple-queue-sim/simple-queue-sim
```

## Test Objective B

```bash
make -C implementation/obj-b/simple-queue-sim test
```

The test targets use Valgrind, so Valgrind must be installed before running the leak tests.

## Clean Objective A and Objective B

```bash
make -C implementation/obj-a clean
make -C implementation/obj-b/simple-queue-sim clean
```

## Future Objective Commands

Add the main build, test, and clean commands here as Objectives C, D, and E gain runnable programs or lab-level scripts.

---

# 🌳 Directory

The repository keeps designs, examples, implementations, and analysis separate so each objective can grow without putting everything in one directory.

```text
.
├── .gitlab-ci.yml
├── analysis
│   ├── multi-LARGE
│   ├── multi-MEDIUM
│   ├── multi-SMALL
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
| `/implementation/` | C++ programs, Makefiles, and automated tests.                |
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

```text
<task-id>: W <positive work amount>
```

For example:

```text
0: W 10
```

means task `0` has `10` units of work.

An empty task list means the job is done:

```text
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

```text
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

```text
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
- [Obj-B examples](/examples/obj-b/README.md)
- [Dr. Veras's `q_ex-1.py`](/examples/obj-b/q_ex-1.py)
- [`q_ex-1.py` explained](/examples/obj-b/q_ex-1_explained.md)

## Queue Configuration Format

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,in.wl,out.wl,[],[]
```

For this example:

```text
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

```text
qmet.csv
job.log
```

The complete input/output formats, server-function rules, event ordering, and command-line forms are in the [Simple Queue Simulator run guide](/implementation/obj-b/simple-queue-sim/README.md).

## Objective B Flow

```text
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

The repository has space for these tools:

```text
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

> TECH LEAD TODO: add links here as each Objective C README and example guide is written.

Suggested locations:

```text
/designs/obj-c/
/examples/obj-c/
/implementation/obj-c/
```

## Objective C Commands

> TECH LEAD TODO: keep the main run commands here and link each tool to its complete README once the command-line interfaces are finalized.

---

# 📈 Objective D: Single Queue Modelling and Analysis

Objective D uses the tools from the earlier objectives to model and study a real process as a single queue.

The work for this objective belongs mainly in:

```text
analysis/simple/
```

The general process is:

```text
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

> TECH LEAD TODO: add the final single-queue model, experiment commands, plots, and analysis links here.

---

# 🌐 Objective E: Multi Queueing System

Objective E extends the queueing work to multiple connected queue systems.

The repository has space for:

```text
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

- **Multi Queue Simulator**: runs connected queue systems.
- **Event Pretty Printer**: makes large event logs easier to read.
- **Network Config Generator**: builds queue configuration data from a network description.

## Objective E Analysis

Multi-queue results can be kept at different workload sizes in:

```text
analysis/multi-SMALL/
analysis/multi-MEDIUM/
analysis/multi-LARGE/
```

## Objective E Documentation

> TECH LEAD TODO: add the main Objective E run commands, configuration examples, and README links here as those interfaces are finalized.

---

# 🔄 How the Objectives Connect

The objectives are meant to pass files and results to each other instead of acting as unrelated programs.

A useful high-level view is:

```text
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

```text
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

## Objectives C, D, and E

> TECH LEAD TODO: add each objective's build, clean, test, and main run commands here as their command-line interfaces are finalized.

For detailed Objective A and Objective B commands, see:

- [Workload Generator run guide](/implementation/obj-a/workload-generator/README.md)
- [Workload Visualizer run guide](/implementation/obj-a/workload-visualizer/README.md)
- [Simple Queue Simulator run guide](/implementation/obj-b/simple-queue-sim/README.md)

---

# 📚 Documentation

This table is the main index for the documentation that already has a README or explanation file.

| Topic                      | File                                                                                                         |
| :------------------------- | :----------------------------------------------------------------------------------------------------------- |
| Objective A overview       | [`/implementation/obj-a/README.md`](/implementation/obj-a/README.md)                                         |
| Generator commands         | [`/implementation/obj-a/workload-generator/README.md`](/implementation/obj-a/workload-generator/README.md)   |
| Visualizer commands        | [`/implementation/obj-a/workload-visualizer/README.md`](/implementation/obj-a/workload-visualizer/README.md) |
| Objective A design         | [`/designs/obj-a/README.md`](/designs/obj-a/README.md)                                                       |
| Objective A examples       | [`/examples/obj-a/README.md`](/examples/obj-a/README.md)                                                     |
| Objective B overview       | [`/implementation/obj-b/README.md`](/implementation/obj-b/README.md)                                         |
| Simulator commands         | [`/implementation/obj-b/simple-queue-sim/README.md`](/implementation/obj-b/simple-queue-sim/README.md)       |
| Simulator design           | [`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)                     |
| Objective B examples       | [`/examples/obj-b/README.md`](/examples/obj-b/README.md)                                                     |
| Provided queue example     | [`/examples/obj-b/q_ex-1_explained.md`](/examples/obj-b/q_ex-1_explained.md)                                 |
| Objective C designs        | [`/designs/obj-c/`](/designs/obj-c/)                                                                         |
| Objective C examples       | [`/examples/obj-c/`](/examples/obj-c/)                                                                       |
| Objective C implementation | [`/implementation/obj-c/`](/implementation/obj-c/)                                                           |
| Single-queue analysis      | [`/analysis/simple/`](/analysis/simple/)                                                                     |
| Objective E designs        | [`/designs/obj-e/`](/designs/obj-e/)                                                                         |
| Objective E examples       | [`/examples/obj-e/`](/examples/obj-e/)                                                                       |
| Objective E implementation | [`/implementation/obj-e/`](/implementation/obj-e/)                                                           |
| Multi-queue analysis       | [`/analysis/`](/analysis/)                                                                                   |

> TECH LEAD TODO: As Objectives C, D, and E gain their own README files, add the direct links to this table and to their objective sections above.

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
