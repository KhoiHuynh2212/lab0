# 🧵 Create Event Trace Design

`create-event-trace` makes a job-by-time picture of the simulator event log. Each job gets one row. Marker shape shows whether the job is waiting, in service, or in an error state, while marker color identifies the queue system.

[Objective C designs](/designs/obj-c/README.md) · [Implementation README](/implementation/obj-c/create-event-trace/README.md) · [Event-log documentation](/implementation/obj-b/simple-queue-sim/README.md)

---

# 🎯 Purpose

Raw event logs are precise but can be hard to scan when several jobs overlap. The trace turns event transitions into a timeline so waiting and service periods can be seen directly.

```txt
┌──────────────┐
│ job.log      │
└──────┬───────┘
       ↓
┌────────────────────┐
│ create-event-trace │
└─────────┬──────────┘
          ↓
┌────────────────────┐
│ event trace PNG    │
│ one row per job    │
└────────────────────┘
```

---

# 📥 Expected Input

The parser keeps lines with at least five whitespace-separated fields where:

```txt
<time> <job-id> IN <queue-id> <event> [details]
```

and `<time>` contains digits.

Example:

```txt
0 j0 IN q0 ARRIVE-VIA in0.wl
3 j0 IN q0 ENTERS-SERVER 0
8 j0 IN q0 EXITS-SERVER 0
8 j0 IN q0 DEPART-VIA out0.wl
2 j1 IN q0 ARRIVE-VIA in0.wl
2 j1 IN q0 ERROR QUEUE-FULL
```

Lines that do not match the event shape are ignored. At least one valid event must remain.

---

# 🎨 Trace Meaning

| Marker | Meaning |
| --- | --- |
| square | job is in a waiting-queue state |
| circle | job is in service |
| `x` | error happened at that time |
| marker color | queue system where the state/event occurred |

A light horizontal line connects a job's state points so its timeline is easy to follow.

---

# 🧱 Important State

For one job, the trace builder only needs:

```txt
current state: queue, service, or none
state start time
current queue id
last time observed in the complete event log
```

When a state ends, the design expands that state into one marker per timestep from the state start up to, but not including, the ending event time.

---

# 🧠 Pseudocode

```txt
READ valid event lines
GROUP them by job id in file order
FIND the final timestamp across all jobs

FOR each job
    state = none

    FOR each event for this job
        IF ARRIVE-VIA
            state = queue
            remember start time and queue

        ELSE IF ENTERS-SERVER
            FLUSH previous state points up to this time
            state = service

        ELSE IF EXITS-SERVER
            FLUSH service points up to this time
            state = queue
            state starts at this time

        ELSE IF DEPART-VIA
            FLUSH current state points up to this time
            state = none

        ELSE IF ERROR
            FLUSH current state points up to this time
            ADD one error marker at this time
            state = none
        END IF
    END FOR

    IF state is still open at end of log
        FLUSH it through the final observed timestep
    END IF
END FOR

SORT job rows naturally when possible: j2 before j10
ASSIGN one color per queue id
DRAW markers, labels, grid, legend, and title
SAVE PNG
```

---

# 🧪 Worked Example

For:

```txt
0 j0 IN q0 ARRIVE-VIA in0.wl
2 j0 IN q0 ENTERS-SERVER 0
5 j0 IN q0 EXITS-SERVER 0
5 j0 IN q0 DEPART-VIA out0.wl
```

`j0` is shown as:

```txt
time 0  1  2  3  4  5
     □  □  ○  ○  ○
     q0 q0 q0 q0 q0
```

The square points show waiting during timesteps `0` and `1`. The circle points show service during `2`, `3`, and `4`. `DEPART-VIA` at `5` ends the state, so there is no waiting/service marker at `5`.

---

# ⚠️ Edge Cases

- A job can move through more than one queue; marker color changes with the queue.
- A queue-full error gets an `x` marker and closes the current state.
- A time-limited log may end while a job is still waiting or in service; the trace extends that state to the last event time seen anywhere in the log.
- Non-event lines are ignored rather than plotted.
- Job IDs that are numeric after an optional leading `j` are sorted numerically.

---

# 🔗 Resources and Connections

- [Implementation and exact CLI](/implementation/obj-c/create-event-trace/README.md)
- [Simple queue event log](/implementation/obj-b/simple-queue-sim/README.md)
- [Pretty-print-events design](/designs/obj-e/pretty-print-events/README.md)
- [Matplotlib documentation](https://matplotlib.org/stable/)
