# Derive Event Metrics Design

`derive-event-metrics` reads simulator events and produces one timing record for each visit a job makes to a queue system. A job that returns to the same queue later gets another row because that is a separate visit.

# Purpose

The event log records what happened. This tool turns that sequence into durations that are easier to compare across jobs and queues.

```txt
┌──────────────┐
│ job.log      │
└──────┬───────┘
       ↓
┌──────────────────────┐
│ derive-event-metrics │
└──────────┬───────────┘
           ↓
┌──────────────────────┐
│ emetric.csv          │
│ one row per visit    │
└──────────────────────┘
```

---

# Expected Input

Relevant event lines use this shape:

```txt
<time> <job-id> IN <queue-id> <EVENT> [details]
```

Example:

```txt
0 j0 IN q0 ARRIVE-VIA in0.wl
2 j0 IN q0 ENTERS-SERVER 0
2 j0 IN q0 TASK-START 0 "W 10"
12 j0 IN q0 TASK-STOP 0 "W 10"
12 j0 IN q0 EXITS-SERVER 0
12 j0 IN q0 DEPART-VIA out0.wl
```

The events that matter most are `ARRIVE-VIA`, `ENTERS-SERVER`, `EXITS-SERVER`, `DEPART-VIA`, and `ERROR`.

---

# Expected Output

```csv
job-id,queue-sys-id,arrival-time,time-in-queue,time-in-service,response-time,turn-around-time
```

| Field              | Meaning                                                          |
| :----------------: | :--------------------------------------------------------------- |
| `job-id`           | Job being measured.                                              |
| `queue-sys-id`     | Queue system for this visit.                                     |
| `arrival-time`     | Time of the visit's `ARRIVE-VIA`.                                |
| `time-in-queue`    | Time during the visit when the job was not in service.           |
| `time-in-service`  | Total time between service entry and exit periods for the visit. |
| `response-time`    | Arrival until the first `ENTERS-SERVER`.                         |
| `turn-around-time` | Arrival until `DEPART-VIA` or `ERROR`.                           |

A value is left empty when the event log never reaches the event needed to calculate it. For example, a queue-full job never enters a server, so response and service fields may be empty.

---

# 🧱 Important State

For each open visit, keep:

```txt
job id
queue id
arrival time
first server-entry time, if any
current server-entry time, if currently in service
accumulated service time
final departure/error time, if any
```

Visits must be tracked independently. The key is not just a job ID because one job can move through multiple queues and can revisit a queue.

---

# 🧠 Pseudocode

```txt
READ event log in file order

FOR each event line
    PARSE time, job id, queue id, event kind

    IF event is ARRIVE-VIA
        START a new open visit for this job and queue
        RECORD arrival time

    ELSE IF event is ENTERS-SERVER
        FIND the matching open visit
        IF this is its first server entry
            RECORD first server-entry time
        END IF
        RECORD current service start

    ELSE IF event is EXITS-SERVER
        FIND the matching open visit
        ADD current time - service start to accumulated service time
        CLEAR current service start

    ELSE IF event is DEPART-VIA or ERROR
        FIND the matching open visit
        RECORD end time
        CALCULATE every metric that has enough information
        EMIT one visit row
        REMOVE the visit from open state
    END IF
END FOR

FOR visits still open at end of file
    EMIT a partial row with unavailable fields left empty
END FOR
```

---

# Worked Example

For this visit:

```txt
0 j0 IN q0 ARRIVE-VIA in0.wl
3 j0 IN q0 ENTERS-SERVER 0
8 j0 IN q0 EXITS-SERVER 0
8 j0 IN q0 DEPART-VIA out0.wl
```

The derived values are:

```txt
arrival-time      = 0
response-time     = 3 - 0 = 3
time-in-service   = 8 - 3 = 5
turn-around-time  = 8 - 0 = 8
time-in-queue     = 8 - 5 = 3
```

The row is therefore conceptually:

```csv
j0,q0,0,3,5,3,8
```

---

# Edge Cases

- A queue-full error may close a visit before service starts.
- A time-limited simulation may leave a visit open at end-of-file.
- A job may visit the same queue more than once; do not merge those visits.
- The implementation accumulates every completed `ENTERS-SERVER` → `EXITS-SERVER` interval it sees for a visit. Normal simulator output usually has one such interval per queue visit.
- Unrelated or malformed lines should not be mistaken for valid visit transitions.
