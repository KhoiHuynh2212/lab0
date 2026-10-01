# Plot Derived Event Metrics Design

`plot-derive-event-metrics` creates either a histogram of one numeric `emetric.csv` column or a scatter plot comparing two numeric columns.

---

# 🎯 Purpose

The derived event-metrics CSV contains one row per queue visit. This plotting tool is deliberately general: instead of hard-coding one metric, the caller names the column or columns to inspect.

```txt
┌───────────────┐
│ emetric.csv   │
└───────┬───────┘
        ↓
┌───────────────────────────┐
│ plot-derive-event-metrics │
└────────────┬──────────────┘
             ↓
      histogram or scatter
             ↓
        ┌─────────┐
        │   PNG   │
        └─────────┘
```

---

# Expected Input

The program can plot any numeric column that exists in the CSV. A typical `emetric.csv` header is:

```csv
job-id,queue-sys-id,arrival-time,time-in-queue,time-in-service,response-time,turn-around-time
```

Rows with an empty value in a selected plotting column are skipped. This matters because incomplete or dropped visits can legitimately have blank derived fields.

Header lookup ignores case and punctuation, so a request for `response-time` can match the normal derived-metrics header.

---

# Plot Modes

## Histogram

Use one column and a positive bin count.

```txt
selected numeric column → divide values into bins → count values per bin → PNG
```

## Scatter

Use exactly two comma-separated columns: `X,Y`.

```txt
X column + Y column → pair values from the same usable rows → points → PNG
```

A row is usable for a scatter plot only when both selected cells are present and numeric.

---

# Pseudocode

```txt
PARSE command line
REQUIRE exactly one mode: histogram OR scatter
READ CSV header
RESOLVE requested column names

FOR each non-empty row
    IF a selected cell is empty
        SKIP this row
    ELSE
        PARSE selected cells as numbers
        STORE the value or value pair
    END IF
END FOR

IF no usable rows remain
    FAIL with an explanatory error
END IF

IF histogram mode
    REQUIRE bins > 0
    DRAW histogram
ELSE
    REQUIRE exactly two selected column names
    DRAW scatter plot
END IF

APPLY optional title and axis limits
CREATE output parent directory if needed
SAVE PNG
```

---

# Worked Examples

Given:

```csv
job-id,queue-sys-id,arrival-time,time-in-queue,time-in-service,response-time,turn-around-time
j0,q0,0,3,5,3,8
j1,q0,2,1,5,1,6
j2,q0,4,,0,,0
```

A histogram of `response-time` uses only `3` and `1`; the blank value for `j2` is skipped.

A scatter plot of `arrival-time,response-time` uses these two points:

```txt
(0, 3)
(2, 1)
```

The third row is skipped because its response time is blank.

---

# Plot Controls

The implementation supports:

| Setting | Meaning |
| --- | --- |
| `--hist COLUMN` | Histogram one column. |
| `--scat X,Y` | Scatter two columns. |
| `--bins N` | Histogram bin count; default is 10. |
| `--title TEXT` | Replace the default title. |
| `--xlim LOW,HIGH` | Set x-axis bounds. |
| `--ylim LOW,HIGH` | Set y-axis bounds. |

---

# Edge Cases

- Do not allow histogram and scatter modes together.
- Reject a zero or negative histogram bin count.
- Reject a scatter selector that is not exactly two column names.
- Reject malformed axis limits that are not `low,high`.
- Reject non-numeric selected values instead of silently changing them.
- Skip empty selected cells because blanks mean “not measured,” not zero.

---

# Resources and Connections

- [Implementation and exact CLI](/implementation/obj-c/plot-derive-event-metrics/README.md)
- [derive-event-metrics design](/designs/obj-c/derive-event-metrics/README.md)
- [Matplotlib histogram documentation](https://matplotlib.org/stable/api/_as_gen/matplotlib.pyplot.hist.html)
- [Matplotlib scatter documentation](https://matplotlib.org/stable/api/_as_gen/matplotlib.pyplot.scatter.html)
