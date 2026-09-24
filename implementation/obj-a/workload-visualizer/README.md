# Obj-A Workload Visualizer

`workload-visualizer` reads a workload CSV file and creates either a histogram or a scatter plot.

The program is self-contained C++ and writes PNG files directly.

## Build

```bash
cd implementation/obj-a/workload-visualizer
make
```

The executable is:

```text
./workload-visualizer
```

## Histogram

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --hist="arrival time" \
    --bins=10 \
    --out=arrival-time.png
```

The default bin count is 10, so `--bins` can be omitted.

## Scatter plot

```bash
./workload-visualizer ../../../examples/obj-a/wl-example.csv \
    --scat="arrival time,len(Tasks)" \
    --out=arrival-vs-tasks.png
```

The numeric workload values used by the visualizer are:

- `arrival time`
- `len(Tasks)`

## Optional plotting settings

Set axis limits with `min,max` pairs:

```bash
--xlim=0,100
--ylim=0,20
```

Add a title:

```bash
--title="Arrival Time Distribution"
```

Change the image size:

```bash
--width=1200 --height=800
```

The minimum image size is 400 by 300 pixels.

Run this for the complete option list:

```bash
./workload-visualizer --help
```

## Workload format

The input file uses the same format as the workload generator and Obj-B:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

`len(Tasks)` is calculated from the task list in each row.

## Tests

```bash
./run-all-tests.sh
```

The test script checks CSV parsing, workload loading, task counts, histogram output, scatter output, PNG structure, command-line behavior, and memory errors.
