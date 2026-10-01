# Workload Visualizer Design

The workload visualizer reads a workload CSV and turns selected workload values into a PNG image.

The working C++ program is here:

[`/implementation/obj-a/workload-visualizer/`](/implementation/obj-a/workload-visualizer/)

Example workload files are here:

[`/examples/obj-a/`](/examples/obj-a/)

---

## 1. Input

The first input is a workload file:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

The visualizer reads two useful numeric values from each row:

```txt
arrival time
len(Tasks)
```

For the three jobs above:

| Job  | arrival time | len(Tasks) |
| :--: | :----------: | :--------: |
| `j0` |      0       |     2      |
| `j1` |      10      |     1      |
| `j2` |      20      |     3      |

The user also chooses exactly one plot mode:

- histogram
- scatter plot

---

## 2. Output

The output is a PNG image.

```txt
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

The output filename is chosen with `--out`.

---

## 3. Histogram Design

A histogram uses one numeric workload column.

For example:

```txt
arrival time values:
0, 10, 20
```

The program:

1. chooses the x-axis range
2. divides that range into bins
3. counts how many values belong in each bin
4. draws one bar for each bin
5. writes the image as a PNG

Pseudocode:

```txt
read workload
get selected numeric column
choose plot limits
split x range into bins
count values in each bin
draw axes and bars
write PNG
```

---

## 4. Scatter Plot Design

A scatter plot uses two numeric workload columns.

With:

```txt
x = arrival time
y = len(Tasks)
```

the example workload becomes:

```txt
(0, 2)
(10, 1)
(20, 3)
```

Pseudocode:

```txt
read workload
get x column
get y column
choose x and y ranges
for each job:
    draw one point at (x, y)
write PNG
```

A rough view is:

```txt
len(Tasks)
    ↑
  3 │                 •
  2 │ •
  1 │         •
  0 └────────────────────→ arrival time
      0       10       20
```

---

## 5. Plot Options

Both plot modes can use:

- x-axis limits
- y-axis limits
- title
- image width
- image height

Histogram mode also uses a bin count.

The default image size is:

```txt
1000 x 700
```

The minimum allowed image size is:

```txt
400 x 300
```

---

## 6. PNG Output

The program writes the PNG directly from C++.

The basic steps are:

```txt
┌──────────────────────────────┐
│    create image in memory    │
└──────────────┬───────────────┘
               │
               ↓
┌──────────────────────────────┐
│ draw background, axes,       │
│ labels, and data             │
└──────────────┬───────────────┘
               │
               ↓
┌──────────────────────────────┐
│ convert image rows to        │
│ PNG data                     │
└──────────────┬───────────────┘
               │
               ↓
┌──────────────────────────────┐
│        write PNG file        │
└──────────────────────────────┘
```

This keeps the visualizer self-contained.

---

## 7. Worked Histogram Example

Command:

```bash
./workload-visualizer workload.csv \
    --hist="arrival time" \
    --bins=3 \
    --out=arrival-time.png
```

The values being plotted are:

```txt
0, 10, 20
```

---

## 8. Worked Scatter Example

Command:

```bash
./workload-visualizer workload.csv \
    --scat="arrival time,len(Tasks)" \
    --out=arrival-vs-tasks.png
```

The points are:

```txt
(0, 2)
(10, 1)
(20, 3)
```

---

## 9. Connection to the Rest of the Lab

The visualizer can read:

```txt
Obj-A generated workloads
Obj-B completed sink workloads
```

This means it can show the workload before or after simulation.

For every run option, see:

[`/implementation/obj-a/workload-visualizer/README.md`](/implementation/obj-a/workload-visualizer/README.md)
