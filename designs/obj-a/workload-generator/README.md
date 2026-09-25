# Objective A Design

Objective A creates workload data and then gives us a way to look at that data before it is sent into the queue simulator.

It has two parts:

1. [`workload-generator/`](/designs/obj-a/workload-generator/) - creates synthetic workloads
2. [`workload-visualizer/`](/designs/obj-a/workload-visualizer/) - turns workload values into PNG plots

The overall design is:

```text
               ┌────────────────────┐
               │ Generator settings │
               └─────────┬──────────┘
                         │
                         ↓
               ┌────────────────────┐
               │ workload-generator │
               └─────────┬──────────┘
                         │
                         ↓
               ┌────────────────────┐
               │    workload.csv    │
               └─────────┬──────────┘
                         │
              ┌──────────┴──────────┐
              │                     │
              ↓                     ↓
    ┌─────────────────────┐   ┌─────────────────────┐
    │ workload-visualizer │   │        Obj-B        │
    └──────────┬──────────┘   └──────────┬──────────┘
               │                         │
               ↓                         ↓
    ┌─────────────────────┐   ┌─────────────────────┐
    │      plot.png       │   │    queue outputs    │
    └─────────────────────┘   └─────────────────────┘
```

---

## 1. Shared Workload Format

Both Obj-A tools use the same workload format as Obj-B:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

Each task has this form:

```txt
<task-id>: W <positive work amount>
```

For example:

```txt
1: W 20
```

means task `1` has `20` units of work.

If a job has no tasks:

```txt
[]
```

then the job is complete.

---

## 2. Workload Generator

The generator creates jobs over a chosen time interval.

At each timestep it:

1. draws the number of arriving jobs from a distribution
2. gives each job a unique ID
3. creates the requested number of tasks
4. gives each task either fixed work or distributed work
5. writes the jobs in arrival-time order

The supported arrival distributions are:

```txt
uniform
normal
geometric
poisson
```

Detailed pseudocode and a worked example are in:

[`/designs/obj-a/workload-generator/README.md`](/designs/obj-a/workload-generator/README.md)

The working program is in:

[`/implementation/obj-a/workload-generator/`](/implementation/obj-a/workload-generator/)

---

## 3. Workload Visualizer

The visualizer reads a workload and makes one of two plots:

```txt
Histogram:  one workload value -> frequency bars
Scatter:    two workload values -> one point per job
```

The main values used for plotting are:

- `arrival time`
- `len(Tasks)`

`len(Tasks)` is found by counting the tasks in each job.

Detailed pseudocode and worked examples are in:

[`/designs/obj-a/workload-visualizer/README.md`](/designs/obj-a/workload-visualizer/README.md)

The working program is in:

[`/implementation/obj-a/workload-visualizer/`](/implementation/obj-a/workload-visualizer/)

---

## 4. Connection to Objective B

The generator does not need a conversion step before Obj-B.

```text
┌────────────────────────┐
│ Obj-A generator output │
└────────────┬───────────┘
             │
             ↓
┌────────────────────────┐
│ Job Id, arrival time,  │
│ Tasks                  │
└────────────┬───────────┘
             │
             ↓
┌────────────────────────┐
│ Obj-B source workload  │
└────────────────────────┘
```

Obj-B also writes completed jobs using the same three columns. This means the visualizer can read either:

- a workload before simulation
- or a completed sink workload after simulation

Example files are in:

[`/examples/obj-a/`](/examples/obj-a/)

The complete Obj-A run guide is in:

[`/implementation/obj-a/README.md`](/implementation/obj-a/README.md)
