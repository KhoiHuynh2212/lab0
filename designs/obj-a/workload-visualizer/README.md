# Workload Visualizer Design

The workload visualizer reads a workload file and creates either a histogram or a scatter plot.

The two workload values needed by the Obj-A plots are:

- `arrival time`
- `len(Tasks)`

`len(Tasks)` is calculated by reading the task list in each workload row.

## Inputs

The first command-line argument is the workload filename.

Histogram mode accepts:

- column name
- bin count
- output PNG filename

Scatter mode accepts:

- x-axis column name
- y-axis column name
- output PNG filename

Both modes can also accept:

- x-axis limits
- y-axis limits
- title
- image width and height

## Pseudocode

```text
read command-line arguments
make sure exactly one plot mode was selected
read workload CSV
find Job Id, arrival time, and Tasks columns

for each workload row:
    read arrival time
    count tasks in Tasks list
    store values needed for plotting

if histogram mode:
    get selected numeric column
    divide selected x range into requested number of bins
    count values in each bin
    draw axes, grid, labels, and bars

if scatter mode:
    get selected x and y columns
    choose axis ranges
    draw axes, grid, labels, and one point for each job

apply optional x limits, y limits, and title
write image as PNG
```

## Worked histogram example

Input workload:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

Command:

```bash
./workload-visualizer workload.csv \
    --hist="arrival time" \
    --bins=3 \
    --out=arrival-time.png
```

The histogram contains the arrival-time values 0, 10, and 20.

## Worked scatter example

Command:

```bash
./workload-visualizer workload.csv \
    --scat="arrival time,len(Tasks)" \
    --out=arrival-vs-tasks.png
```

The plotted points are:

```text
(0, 2)
(10, 1)
(20, 3)
```

## PNG output design

The visualizer writes PNG files directly from C++.

The renderer:

1. creates an RGB image in memory,
2. draws the plot frame and data,
3. converts the image rows to PNG scanlines,
4. writes a valid PNG file with IHDR, IDAT, and IEND chunks.

This keeps the visualizer self-contained and avoids requiring a separate plotting program at runtime.
