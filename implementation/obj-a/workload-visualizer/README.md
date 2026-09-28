# Objective A Workload Visualizer

`workload-visualizer` reads a workload CSV file and creates either a histogram or a scatter plot as a PNG image.

The program is written in C++ and lives in:

[`/implementation/obj-a/workload-visualizer/`](/implementation/obj-a/workload-visualizer/)

Example workload files are in:

[`/examples/obj-a/`](/examples/obj-a/)

---

## 1. What the Program Does

The program reads a workload such as:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

It can plot:

```txt
arrival time
len(Tasks)
```

The main flow is:

```text
┌─────────────────────┐
│    workload.csv     │
└──────────┬──────────┘
           │
           ↓
┌─────────────────────┐
│ workload-visualizer │
└──────────┬──────────┘
           │
           ↓
┌─────────────────────┐
│      plot.png       │
└─────────────────────┘
```

---

## 2. Input

The first positional argument is the workload filename.

For example:

```bash
./workload-visualizer workload.csv ...
```

The workload must contain:

```txt
Job Id
arrival time
Tasks
```

The visualizer calculates `len(Tasks)` by counting the tasks in each row.

You must choose exactly one plot mode:

```txt
--hist
or
--scat
```

---

## 3. Output

The output is a PNG image chosen with:

```bash
--out plot.png
```

For example:

```bash
./workload-visualizer workload.csv \
    --hist="arrival time" \
    --out=arrival-times.png
```

---

## 4. Build

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
make -C implementation/obj-a/workload-visualizer
```

or:

```bash
make -C implementation/obj-a/workload-visualizer build
```

The executable is:

```txt
implementation/obj-a/workload-visualizer/workload-visualizer
```

---

## 5. Clean

From this directory:

```bash
make clean
```

From the repository root:

```bash
make -C implementation/obj-a/workload-visualizer clean
```

---

## 6. Histogram Mode

General form:

```bash
./workload-visualizer <workload.csv> \
    --hist="<column>" \
    --out=<plot.png> \
    [options]
```

Supported histogram columns include:

```txt
arrival time
len(Tasks)
```

### Arrival-Time Histogram

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --hist="arrival time" \
    --bins=10 \
    --out=arrival-time.png
```

### Task-Count Histogram

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --hist="len(Tasks)" \
    --bins=3 \
    --out=task-count.png
```

The default bin count is `10`, so this is also valid:

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --hist="arrival time" \
    --out=arrival-time.png
```

---

## 7. Scatter Plot Mode

General form:

```bash
./workload-visualizer <workload.csv> \
    --scat="<x-column>,<y-column>" \
    --out=<plot.png> \
    [options]
```

Example:

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --scat="arrival time,len(Tasks)" \
    --out=arrival-vs-tasks.png
```

For the example workload, the points are:

```txt
(0, 2)
(10, 1)
(20, 3)
```

---

## 8. Plot Title

Add a title with:

```bash
--title="Arrival Time Distribution"
```

Example:

```bash
./workload-visualizer workload.csv \
    --hist="arrival time" \
    --title="Arrival Time Distribution" \
    --out=arrival-time.png
```

---

## 9. Axis Limits

Set x-axis limits with:

```bash
--xlim=0,100
```

Set y-axis limits with:

```bash
--ylim=0,20
```

Use both when needed:

```bash
./workload-visualizer workload.csv \
    --scat="arrival time,len(Tasks)" \
    --xlim=0,100 \
    --ylim=0,10 \
    --out=scatter.png
```

For each limit pair, the maximum must be greater than the minimum.

---

## 10. Image Size

The default image size is:

```txt
1000 x 700
```

Change it with:

```bash
--width=1200 --height=800
```

Example:

```bash
./workload-visualizer workload.csv \
    --hist="arrival time" \
    --width=1200 \
    --height=800 \
    --out=large-histogram.png
```

The minimum size is:

```txt
400 x 300
```

---

## 11. Complete Histogram Example

```bash
./workload-visualizer ../../../examples/obj-a/wl-poisson.csv \
    --hist="arrival time" \
    --bins=12 \
    --xlim=0,20 \
    --ylim=0,10 \
    --title="Poisson Arrival Times" \
    --width=1200 \
    --height=800 \
    --out=poisson-arrivals.png
```

---

## 12. Complete Scatter Example

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --scat="arrival time,len(Tasks)" \
    --xlim=0,25 \
    --ylim=0,4 \
    --title="Arrival Time vs Task Count" \
    --width=1200 \
    --height=800 \
    --out=arrival-vs-tasks.png
```

---

## 13. Option Syntax

Options can be written with `=`:

```bash
--bins=10
--out=plot.png
```

or with a space:

```bash
--bins 10
--out plot.png
```

This also works for `--hist`, `--scat`, `--xlim`, `--ylim`, `--title`, `--width`, and `--height`.

---

## 14. Help

From this directory:

```bash
./workload-visualizer --help
```

or:

```bash
./workload-visualizer -h
```

From the repository root:

```bash
implementation/obj-a/workload-visualizer/workload-visualizer --help
```

---

## 15. Tests

The Makefile has these targets:

```bash
make test
make run-tests-coverage
make run-tests-leak
```

From the repository root:

```bash
make -C implementation/obj-a/workload-visualizer test
make -C implementation/obj-a/workload-visualizer run-tests-coverage
make -C implementation/obj-a/workload-visualizer run-tests-leak
```

The Makefile test targets require Valgrind.

The test script can also be run directly:

```bash
implementation/obj-a/workload-visualizer/run-all-tests.sh
implementation/obj-a/workload-visualizer/run-all-tests.sh all
implementation/obj-a/workload-visualizer/run-all-tests.sh coverage
implementation/obj-a/workload-visualizer/run-all-tests.sh leak
```

---

## 16. Connection to Obj-A Generator and Obj-B

The visualizer accepts the same workload format used by both programs:

```text
             ┌────────────────────┐
             │ workload-generator │
             └─────────┬──────────┘
                       │
                       ↓
             ┌────────────────────┐
             │    workload.csv    │
             └─────────┬──────────┘
                       │
              ┌────────┴─────────┐
              │                  │
              ↓                  ↓
┌─────────────────────┐   ┌──────────────────┐
│ workload-visualizer │   │ simple-queue-sim │
└──────────┬──────────┘   └─────────┬────────┘
           │                        │
           ↓                        ↓
┌─────────────────────┐   ┌──────────────────┐
│      plot.png       │   │  sink workload   │
└─────────────────────┘   └─────────┬────────┘
                                    │
                                    └──────→ can be visualized again
```

This means the visualizer can plot:

- generated arrival workloads
- completed Obj-B sink workloads

For design details, see:

[`/designs/obj-a/workload-visualizer/README.md`](/designs/obj-a/workload-visualizer/README.md)
