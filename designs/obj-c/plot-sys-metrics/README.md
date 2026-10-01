# Plot System Metrics Design

`plot-sys-metrics` turns the CSV created by `derive-sys-metrics` into one PNG with two aligned views: response-time statistics on top and throughput below.

---

# Purpose

A `sysmetric.csv` file is useful for calculations, but a plot makes trends easier to see. The tool keeps latency and throughput in separate panels so values with different units are not forced onto one y-axis.

```txt
┌─────────────────┐
│ sysmetric.csv   │
└────────┬────────┘
         ↓
┌─────────────────┐
│ plot-sys-metrics│
└────────┬────────┘
         ↓
┌─────────────────┐
│ system plot PNG │
│ latency + rate  │
└─────────────────┘
```

---

# Expected Input

The input CSV must contain these columns. Header matching ignores case and punctuation.

| Column | Meaning |
| --- | --- |
| `time-step` | Time when jobs exited the complete system. |
| `Avg-response-time` | Mean end-to-end response time for jobs exiting at that step. |
| `median-response-time` | Median response time. |
| `10-response-time` | 10th-percentile response time. |
| `90-response-time` | 90th-percentile response time. |
| `throughput` | Number of jobs that exited at that step. |

Empty response-time cells are allowed and are skipped for that response series. `time-step` and `throughput` may not be empty.

Example:

```csv
time-step,Avg-response-time,median-response-time,10-response-time,90-response-time,throughput
20,20,20,20,20,1
30,18,18,18,18,2
```

---

# Expected Output

One PNG is written to the path supplied by the caller. The parent directory is created when necessary.

The top panel contains four response-time lines:

- average
- median
- P10
- P90

The lower panel contains throughput bars using the same time-step x-axis.

---

# Pseudocode

```txt
READ input CSV header
FIND all six required columns

FOR each non-empty data row
    PARSE time-step and throughput as numbers
    PARSE each response-time field when it is not empty
    STORE values in matching series
END FOR

CREATE figure with two vertically stacked axes

FOR average, median, P10, and P90
    DROP points whose response value is empty
    DRAW line against time-step
END FOR

DRAW throughput bars against time-step
SET labels, titles, grid, and legend
SAVE the figure as PNG
```

The response series intentionally skip missing cells rather than treating them as zero. Zero would mean a measured response time of zero, which is different from “no measurement for this row.”

---

# Worked Example

Given:

```csv
time-step,Avg-response-time,median-response-time,10-response-time,90-response-time,throughput
10,5,5,3,7,2
20,8,7,4,12,3
```

The top panel gets two points for each latency series. The lower panel gets a bar of height `2` at time `10` and a bar of height `3` at time `20`.

A title passed with `--title="Car wash baseline"` becomes the prefix of the latency-panel title. The throughput panel stays labeled `throughput vs time-step`.

---

# Edge Cases

- Missing input file: fail instead of creating an empty plot.
- Missing required column: fail and name the missing column.
- Non-numeric required value: fail and report the row.
- Empty file or header-only file: fail because there is no data to plot.
- Empty response cell: keep the row for throughput but omit that latency point.

---

# Resources and Connections

- [Implementation source and run guide](/implementation/obj-c/plot-sys-metrics/README.md)
- [derive-sys-metrics design](/designs/obj-c/derive-sys-metrics/README.md)
- [Objective D single-queue analysis](/analysis/simple/README.md)
- [Matplotlib documentation](https://matplotlib.org/stable/)
