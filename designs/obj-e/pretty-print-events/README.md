# 🧾 Pretty-Print Events Design

`pretty-print-events` turns a simulator `job.log` into a table with time down the left side and one column per queue system. It is a debugging view: the event log stays the source of truth, while the table makes cross-queue behavior easier to follow.

---

# 🎯 Purpose

A multi-queue event log can contain many events at one timestep. Reading it line-by-line makes it hard to see which queue each event belongs to. The pretty printer places events into queue columns while preserving the important order constraints.

```txt
┌──────────────┐
│ job.log      │
└──────┬───────┘
       ↓
┌─────────────────────┐
│ pretty-print-events │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ terminal table      │
│ optional TSV copy   │
└─────────────────────┘
```

---

# 📥 Event Format

A parsable line looks like:

```txt
<time> <job> IN <queue> <event> [detail]
```

Example:

```txt
10 j0 IN q0 TASK-START 1 "W 10"
10 j1 IN q2 ERROR QUEUE-FULL
20 j0 IN q3 DEPART-VIA out0.wl
```

Known event kinds receive distinct terminal styles. Unknown kinds are still displayed using a default style.

Blank lines are ignored. Nonblank lines that do not match the event pattern are counted and reported as skipped.

---

# 🧹 Filtering

Before layout, the caller may filter by:

- queue IDs,
- job IDs,
- event kinds,
- first timestep,
- last timestep.

All active filters must match for an event to remain.

Queue columns normally use every queue found in the selected events and sort naturally, so `q2` appears before `q10`. If `--queues` is supplied, that requested order becomes the column order.

---

# 🧠 Row-Packing Pseudocode

The default mode tries to place independent events from the same timestep on the same visual row.

Two constraints prevent unsafe packing:

1. events in the **same queue** must stay top-to-bottom in log order,
2. events for the **same job** must stay top-to-bottom in log order even when the job crosses queues.

```txt
GROUP selected events by timestep

FOR each timestep group
    next_free_row[queue] = 0
    last_row_of_job[job] = none

    FOR each event in original log order
        column = event.queue
        row = maximum(
            next_free_row[column],
            last_row_of_job[event.job] + 1
        )

        PLACE event in that row and queue column
        next_free_row[column] = row + 1
        last_row_of_job[event.job] = row
    END FOR

    PRINT the timestep only on the first row of the group
END FOR
```

With `--sequential`, packing is disabled and each event gets its own row.

---

# 🧪 Worked Example

Input:

```txt
5 a IN q0 ARRIVE-VIA in0.wl
5 b IN q1 ARRIVE-VIA in1.wl
5 a IN q1 DEPART-VIA out0.wl
```

The first two events can share a row because they use different queues and different jobs. The third event must be below the first event for job `a`.

Conceptually:

```txt
time  q0                    q1
-----------------------------------------------
   5  a ARRIVE-VIA in0.wl   b ARRIVE-VIA in1.wl
                             a DEPART-VIA out0.wl
```

---

# 📤 Output Formats

## Terminal table

The table can use ANSI color depending on `--color auto|always|never`. Long cells are cut to the selected maximum width and end with `~` when truncated.

## TSV

`--tsv FILE` writes the same row/column layout as tab-separated values. TSV output has no color codes and does not truncate cell text.

## Legend

`--legend` prints a short explanation of known event types after the terminal table.

---

# ⚠️ Edge Cases

- If `--from` is greater than `--to`, fail instead of producing a misleading empty range.
- If no events match, print no table and report `no events to show` on stderr.
- Duplicate queue names in `--queues` collapse to the first occurrence.
- A positive column width is required.
- Color `auto` is enabled only when stdout is a terminal.
- A TSV write error should make the command fail even if terminal output was already printed.

---

# 🔗 Resources and Connections

- [Implementation and complete CLI](/implementation/obj-e/pretty-print-events/README.md)
- [Multi-queue simulator design](/designs/obj-e/multi-queue-sim/README.md)
- [AS-733 pretty-print examples](/examples/obj-e/as-733/README.md)
- [Debugging Pretty article referenced by the implementation](https://blog.josejg.com/debugging-pretty/)
