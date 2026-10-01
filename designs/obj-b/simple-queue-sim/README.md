# Simple Queue Simulator Design

The simple queue simulator models one queue system with a FIFO waiting queue and one or more servers.

The working C++ implementation is here:

[`/implementation/obj-b/simple-queue-sim/`](/implementation/obj-b/simple-queue-sim/)

The provided small queue example is explained here:

[`/examples/obj-b/q_ex-1_explained.md`](/examples/obj-b/q_ex-1_explained.md)

---

## 1. Inputs

The simulator has two main input files.

### Queue Configuration

The configuration contains exactly one queue-system row:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,in.wl,out.wl,[],[]
```

The columns mean:

|                 Column | Meaning                                          |
| ---------------------: | :----------------------------------------------- |
|         `Queue sys ID` | name of the queue system                         |
|       `Max Queue Size` | maximum number of jobs waiting for a server      |
|         `# of servers` | number of servers that can work at the same time |
|      `Server Function` | work performed on the first task                 |
| `Server Function Rate` | timesteps needed for one server-function step    |
|               `source` | workload file read by this queue                 |
|                 `sink` | completed-job workload written by this queue     |
|             `Edges in` | incoming edge names                              |
|            `Edges out` | outgoing edge names                              |

The maximum queue size counts only waiting jobs. Jobs already in a server do not count toward it.

### Source Workload

The source workload contains jobs:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 20', '1: W 10']"
j1,2,"['0: W 10']"
```

Each task has this form:

```txt
<task-id>: W <positive work amount>
```

A job is complete when:

```txt
Tasks == []
```

---

## 2. Outputs

The simulator can create:

```txt
sink workload
queue metrics
job event log
terminal summary
```

The flow is:

```txt
┌─────────────────┐     ┌────────────────────┐     ┌─────────────┐
│ source workload │ ──→ │ FIFO waiting queue │ ──→ │   servers   │
└─────────────────┘     └─────────┬──────────┘     └──────┬──────┘
                                  │                       │
                                  │                       ├──────────────→ ┌───────────────────────┐
                                  │                       │                │ sink / outgoing edge  │
                                  │                       │                └───────────────────────┘
                                  └───────────┬───────────┘
                                              │ metrics
                                              ↓
                                        ┌──────────┐
                                        │ qmet.csv │
                                        └──────────┘

┌─────────────────────┐      state changes      ┌─────────┐
│ queue + server state│ ──────────────────────→ │ job.log │
└─────────────────────┘                         └─────────┘
```

### Sink Workload

Completed jobs are written using the workload format:

```csv
Job Id,arrival time,Tasks
j0,30,[]
```

For a sink job, `arrival time` stores the timestep when the job left the system.

### Queue Metrics

One metrics row is written after each simulated timestep:

```csv
time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,num-of-errors
0,q0,0,1,0
1,q0,0,1,0
```

`num-of-errors` is the cumulative error count at that point in the simulation.

### Event Log

A job is logged whenever its state changes.

Examples:

```txt
0 j0 IN q0 ARRIVE-VIA in.wl
0 j0 IN q0 ENTERS-SERVER 0
0 j0 IN q0 TASK-START 0 "W 20"
10 j0 IN q0 TASK-STOP 0 "W 20"
10 j0 IN q0 TASK-START 0 "W 10"
20 j0 IN q0 TASK-STOP 0 "W 10"
20 j0 IN q0 TASK-START 1 "W 10"
30 j0 IN q0 TASK-STOP 1 "W 10"
30 j0 IN q0 EXITS-SERVER 0
30 j0 IN q0 DEPART-VIA out.wl
```

Errors use:

```txt
ERROR QUEUE-FULL
ERROR NO-OUT-EDGE
```

---

## 3. Server Functions

The supported function is:

```txt
Sub-X
```

`Sub-X` changes the first task only.

If the first task is larger than `X`, subtract `X`:

```txt
┌──────────────────────┐
│    ['0: W 25']       │
└──────────┬───────────┘
           │ Sub-10
           ↓
┌──────────────────────┐
│    ['0: W 15']       │
└──────────────────────┘
```

If the first task is less than or equal to `X`, remove it:

```txt
┌──────────────────────────────┐
│ ['0: W 10', '1: W 20']       │
└──────────────┬───────────────┘
               │ Sub-10
               ↓
┌──────────────────────────────┐
│ ['1: W 20']                  │
└──────────────────────────────┘
```

### Function Chains

Functions can be chained:

```txt
Sub-5,Sub-10
```

The server applies `Sub-5`, waits the configured function rate, then applies `Sub-10` after another function-rate period.

### Repeating Functions

A trailing `*` repeats the chain while the job still has tasks:

```txt
Sub-10*
```

For:

```txt
['0: W 25']
```

this becomes:

```txt
┌──────┐     ┌──────┐     ┌─────┐     ┌────┐
│ W 25 │ ──→ │ W 15 │ ──→ │ W 5 │ ──→ │ [] │
└──────┘     └──────┘     └─────┘     └────┘
```

Each arrow takes one `Server Function Rate` period.

---

## 4. FIFO Queue Behavior

Waiting jobs are First In, First Out.

If the waiting queue is:

```txt
[j1][j2][j3]
```

then the next free server takes:

```txt
j1
```

before `j2` or `j3`.

If a source job arrives while the waiting queue is empty and a server is idle, the job can enter that server immediately.

If every server is busy, the job waits as long as there is room in the queue.

If the waiting queue is already full, the job receives:

```txt
ERROR QUEUE-FULL
```

---

## 5. Timestep Order

The simulator processes each timestep in this order:

```txt
1. advance jobs already in service
2. finish server-function steps that reached zero
3. route jobs that leave servers
4. move already-waiting jobs into idle servers
5. add source jobs whose arrival time equals the current timestep
6. send a new arrival directly to a server when possible
7. otherwise place the arrival in the FIFO waiting queue
8. record QUEUE-FULL if there is no waiting space
9. write one metrics row
10. advance the clock
```

A newly started job does not have its service time decremented until the next timestep.

This is the same important timing idea shown in [`q_ex-1.py`](/examples/obj-b/q_ex-1.py): a job can enter a server at time `t`, but its first unit of service time passes after that point.

---

## 6. Routing a Job Out

When a job leaves a server:

```txt
if Tasks is empty and a sink exists:
    write job to sink
else if an outgoing edge exists:
    depart through an outgoing edge
else:
    ERROR NO-OUT-EDGE
```

With multiple outgoing edges, the simple simulator takes turns between them in round-robin order.

Example:

```txt
Edges out = [edgeA, edgeB]

1st departure -> edgeA
2nd departure -> edgeB
3rd departure -> edgeA
4th departure -> edgeB
```

Obj-B logs the outgoing-edge departure. It does not simulate another queue receiving that edge; that belongs to the later multi-queue system.

---

## 7. Main State

The simulator keeps track of:

|      State       | Purpose                                                |
| :--------------: | :----------------------------------------------------- |
|   source jobs    | jobs that have not arrived yet                         |
|  waiting queue   | FIFO jobs waiting for a server                         |
|  server states   | current job and remaining service time for each server |
|    sink jobs     | completed jobs waiting to be written                   |
|   error count    | queue-full and no-out-edge errors                      |
| current timestep | simulation clock                                       |

Each server keeps:

```txt
busy or idle
current job
service time remaining
current server-function step
```

---

## 8. Pseudocode

```txt
load one queue configuration
load and sort the source workload
create empty FIFO waiting queue
create all servers as idle
set time = 0

while the time limit has not been reached
or, with no limit, while unfinished work remains:

    for each busy server:
        subtract one from service time remaining

        if the current function step finished:
            log TASK-STOP
            apply the current Sub-X step

            if another function step should run:
                reset service time remaining
                log TASK-START
            else:
                log EXITS-SERVER
                route the job to sink, outgoing edge, or error

    move waiting jobs into idle servers in FIFO order

    for each source job arriving now:
        log ARRIVE-VIA

        if the job already has no tasks:
            route it immediately
        else if queue is empty and a server is idle:
            start it on that server
        else if waiting queue has room:
            append it to the waiting queue
        else:
            log ERROR QUEUE-FULL

    write queue metrics
    time = time + 1

write completed sink workload
return final summary
```

---

## 9. Worked Example

Configuration:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,2,1,Sub-10*,2,in.wl,out.wl,[],[]
```

Source:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 20']"
j1,1,"['0: W 10']"
```

The server-function rate is `2`, so each `Sub-10` step takes two timesteps.

A simplified timeline is:

```txt
time:  0   1   2   3   4   5   6
j0:   [------ server ------]
j1:       [ wait ]          [server]
```

`j0` enters first. `j1` arrives while the server is busy, so it waits. After `j0` finishes, FIFO dispatch sends `j1` into the server.

---

## 10. Connection to Objective A

Obj-A creates source workloads using exactly the format Obj-B reads:

```txt
┌──────────────────────────┐
│ Obj-A workload-generator │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│     source workload      │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│     simple-queue-sim     │
└──────────────────────────┘
```

Obj-A's visualizer can also read Obj-B's completed sink workload.

For the complete command reference, see:

[`/implementation/obj-b/simple-queue-sim/README.md`](/implementation/obj-b/simple-queue-sim/README.md)
