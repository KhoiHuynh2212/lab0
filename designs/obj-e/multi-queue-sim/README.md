# Multi-Queue Simulator Design

`multi-queue-sim` runs every queue-system row in one configuration as a single connected simulation. Jobs can enter from workload files, move through queue-system edges, and leave through sink workload files.

---

# Purpose

Objective B models one queue system. Objective E keeps the same workload and server-function ideas but adds routing between several queue systems.

```txt
source workload
      │
      ↓
┌──────────┐     ┌──────────┐     ┌──────────┐
│    q0    │ ──→ │    q1    │ ──→ │    q2    │
└──────────┘     └──────────┘     └────┬─────┘
                                       ↓
                                  sink workload
```

All queue systems advance on the same discrete simulation clock.

---

Configuration Input

One CSV row describes one queue system. Required columns are matched case-insensitively with punctuation ignored, and several equivalent header spellings are accepted by the parser.

A normal header is:

```csv
queue_sys_id,max_queue_size,num_servers,server_function,server_function_rate,source,sink,edges_in,edges_out
```

| Field                  | Meaning                                                              |
| ---------------------- | -------------------------------------------------------------------- |
| `queue_sys_id`         | Unique queue-system ID such as `q0`.                                 |
| `max_queue_size`       | Maximum number of jobs allowed to wait. `0` means no waiting slots.  |
| `num_servers`          | Number of parallel servers; must be positive.                        |
| `server_function`      | Same `Sub-N` function-chain syntax used by Objective B.              |
| `server_function_rate` | Timesteps spent on each function step; must be positive.             |
| `source`               | Optional workload file that injects jobs into this queue.            |
| `sink`                 | Optional workload file that receives completed jobs.                 |
| `edges_in`             | Incoming queue IDs separated using the Objective B edge-list parser. |
| `edges_out`            | Outgoing queue IDs used for routing.                                 |

Source and sink paths are resolved relative to the configuration file, not relative to the shell's current directory.

Every queue ID named by an incoming or outgoing edge must also exist as a row in the same configuration.

---

# Runtime State

Each queue system needs its own:

```txt
FIFO waiting queue
array of server states
source workload and next source index
completed sink jobs
cumulative error count
outgoing-edge round-robin index
```

Each busy server keeps:

```txt
current job
service time remaining for this function step
server-function step index
```

A lookup table maps queue IDs to runtime indexes so routed jobs can be delivered efficiently.

---

# Routing Rules

When a job leaves a server, routing follows this order:

1. If the job has no tasks left **and** this queue has a sink, write it to that sink at the end of the run.
2. Otherwise, if the queue has outgoing edges, choose the next edge in round-robin order and deliver the job to that queue during the same timestep.
3. Otherwise, record `ERROR NO-OUT-EDGE`.

If a job arrives at a queue with no tasks left, it is routed immediately using those same rules instead of occupying a server.

If an arriving job still has tasks:

1. use an idle server immediately when the waiting queue is empty,
2. otherwise append to the FIFO waiting queue if space remains,
3. otherwise record `ERROR QUEUE-FULL`.

---

# Timestep Pseudocode

The order is important because every queue system must see the same clock and no queue should gain an advantage just because its config row appears earlier.

```txt
WHILE the stop condition has not been met
    CLEAR the list of routed arrivals for this tick

    PHASE 1: advance every busy server in every queue
        decrement function-step timers
        when a function step finishes:
            log TASK-STOP
            subtract work from the current task
            remove a completed task
            either start the next function step/task
            or log EXITS-SERVER and route the job

    PHASE 2: dispatch already-waiting FIFO jobs to idle servers

    PHASE 3: admit arrivals
        admit source-file jobs whose arrival time equals this tick
        admit jobs routed from other queues in phase 1
        route empty jobs immediately
        start, queue, or reject non-empty jobs

    DISPATCH waiting jobs again so same-tick openings can be used

    PHASE 4: write one qmet.csv row per queue system

    advance the global clock
END WHILE
```

---

# Stop Conditions

Without a time limit, stop only when every queue system has:

- no future source jobs,
- no waiting jobs, and
- no busy servers.

With a time limit, stop when that limit is reached even if work remains. The terminal summary reports waiting, in-service, and not-yet-arrived counts so an intentionally truncated run is visible.

---

# Outputs

## Queue metrics

One row per queue system per processed timestep:

```csv
time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,num-of-errors
```

`num-of-errors` is cumulative for that queue system.

## Event log

Each event names the queue where it happened:

```txt
10 j0 IN q3 TASK-START 1 "W 10"
20 j0 IN q3 DEPART-VIA q7
20 j0 IN q7 ARRIVE-VIA q3
```

Routed arrivals are represented by the target queue's subsequent events; the simulator's main event actions remain compatible with Objective C and `pretty-print-events`.

## Sink workloads

Several exit queues may name the same sink file. The simulator first combines all completed jobs for that resolved path, sorts them stably by exit time, and writes the sink once so later queues do not overwrite earlier results.

---

# Worked Example

Configuration:

```csv
queue_sys_id,max_queue_size,num_servers,server_function,server_function_rate,source,sink,edges_in,edges_out
q0,2,1,Sub-10*,1,in0.wl,, ,q1
q1,2,1,Sub-10*,1,,out0.wl,q0,
```

Source workload:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
```

Conceptually:

```txt
t=0: j0 arrives at q0 and starts task 0
t=1: q0 finishes task 0 and routes j0 to q1
t=1: q1 admits j0 and starts remaining task 1
t=2: q1 finishes task 1 and sends j0 to out0.wl
```

The exact event ordering also includes server-entry, task-start/stop, server-exit, and departure events.

---

# Edge Cases

- Duplicate queue IDs are invalid.
- An edge that names an unknown queue is invalid.
- Zero servers or a non-positive server-function rate is invalid.
- Cyclic networks are allowed by the simulator; whether a job terminates depends on its remaining tasks and routing.
- A source or sink field may be empty.
- Multiple source or sink queues may share the same workload filename.
- Queue size `0` allows direct-to-idle-server service but no waiting when all servers are busy.

---

# Resources and Connections

- [Implementation and command reference](/implementation/obj-e/multi-queue-sim/README.md)
- [Objective B server-function design](/designs/obj-b/simple-queue-sim/README.md)
- [Configuration generator design](/designs/obj-e/create-config-from-network/README.md)
- [Pretty-printer design](/designs/obj-e/pretty-print-events/README.md)
- [AS-733 example](/examples/obj-e/as-733/README.md)
- [C++ `std::deque` reference](https://en.cppreference.com/w/cpp/container/deque.html)
