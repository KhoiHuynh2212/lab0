# [`q_ex-1.py`](./q_ex-1.py) Explained

This document explains Dr. Veras's [`q_ex-1.py`](./q_ex-1.py) program as a starting point for Lab 0. The script simulates one FIFO queue feeding one server, advances time one step at a time, and records when jobs finish. This file demonstrates the basic event loop and state variables that our larger queue simulator implementation will need.

## 1. What the Program Models

The program keeps track of the following:

- a list of jobs that have not arrived yet
- a waiting queue for jobs that have arrived but cannot be serviced yet
- one server that can process only one job at a time
- a list of completed jobs
- a simulation clock that advances one timestep at a time

The workload is defined as a list of tuples:

```python
jobs = [("a",1,2),
        ("b",2,1),
        ("c",3,2),
        ("d",8,2),
        ("e",8,3),
        ("f",9,2)]
```

Each tuple has the form:

```txt
(job ID, arrival time, service time)
```

For example:

```py
("a", 1, 2)
```

means that:

- the job's ID is `"a"`
- the job arrives at timestep `1`
- once it enters the server, it requires `2` timesteps of service

The workload is already sorted by arrival time. This is important because the program checks only the first remaining job in the `jobs` list when determining whether a new job should enter the queue.

### Mapping the Workload

There are two different timelines that are useful to visualize:

1. When jobs arrive and how much service they request
2. When jobs actually receive service

#### Arrival/Requested Services

If we temporarily ignore queueing, each job can be drawn beginning at its arrival time and extending for the amount of service it requests:

```txt
Timestep: | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 | 11 | 12 |
               |    a    |
                    | b  |
                         |    c    |
                                                  |    d    |
                                                  |      e       |
                                                       |    f    |
```

This diagram is useful for showing the workload being submitted to the system, but it is not the actual server schedule.

#### Actual FIFO Service Schedule

The simulator uses a First In, First Out (FIFO) queue. Arriving jobs are appended to the `work_queue`, and when the server becomes available, the program removes the first waiting job with:

```py
j = work_queue.pop(0)
```

This means jobs are serviced in the order they enter the queue. Therefore, the actual schedule would be:

| Job | Arrives | Starts Service | Finishes | Reason for Waiting  |
| :-: | :-----: | :------------: | :------: | :-----------------: |
|  a  |    1    |       1        |    3     |     Server free     |
|  b  |    2    |       3        |    4     | `a` is using server |
|  c  |    3    |       4        |    6     | `b` is ahead of it  |
|  d  |    8    |       8        |    10    |     Server free     |
|  e  |    8    |       10       |    13    | `d` is ahead of it  |
|  f  |    9    |       13       |    15    | `e` is ahead of it  |

And this is the actual timeline:

```txt
Timestep: | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 | 11 | 12 | 13 | 14 | 15 |
               |    a    |
                         | b  |
                              |    c    |
                                                  |    d    |
                                                            |      e       |
                                                                           |    f    |
```

The arrival time is when the job initially enters the system, while the service time tells us when the server is available to process it.

For example:

```txt
Job b:
    arrives at 2
    waits from 2 to 3
    receives service from 3 to 4

Job e:
    arrives at 8
    waits from 8 to 10
    receives service from 10 to 13

Job f:
    arrives at 9
    waits from 9 to 13
    receives service from 13 to 15
```

---

## 2. The Program's State

The simulation starts with the following variables:

```python
clock=0
limit=20

work_queue=list()
jobs_complete=list()
job_in_service=None
service_time_left=0
```

|      Variable       | Purpose                             |
| :-----------------: | :---------------------------------- |
|       `clock`       | current simulation timestep         |
|       `limit`       | timestep where the simulation stops |
|    `work_queue`     | jobs that arrived and are waiting   |
|   `jobs_complete`   | jobs that finished                  |
|  `job_in_service`   | job currently using the server      |
| `service_time_left` | remaining work for that job         |

`None` means the server is idle.

---

## 3. The Main Simulation Loop

The simulator runs one timestep at a time:

```python
while clock < limit:
```

Each loop does the following:

- [`q_ex-1.py` Explained](#q_ex-1py-explained)
  - [1. What the Program Models](#1-what-the-program-models)
    - [Mapping the Workload](#mapping-the-workload)
      - [Arrival/Requested Services](#arrivalrequested-services)
      - [Actual FIFO Service Schedule](#actual-fifo-service-schedule)
  - [2. The Program's State](#2-the-programs-state)
  - [3. The Main Simulation Loop](#3-the-main-simulation-loop)
    - [1. Update the Current Job](#1-update-the-current-job)
    - [2. Complete a Finished Job](#2-complete-a-finished-job)
    - [3. Add New Arrivals](#3-add-new-arrivals)
    - [4. Send a Waiting Job to the Server](#4-send-a-waiting-job-to-the-server)
    - [5. Print the State and Advance Time](#5-print-the-state-and-advance-time)
  - [4. Expected Result](#4-expected-result)
  - [5. Lab Objectives](#5-lab-objectives)

The order matters. A job may arrive before the server is available, so arrival time and the time the service starts are not always the same.

### 1. Update the Current Job

```python
if job_in_service != None:
    service_time_left -= 1
```

If the server is busy, one timestep of work is completed.

For job `a`:

```text
starts with 2
clock 2: 2 -> 1
clock 3: 1 -> 0
```

### 2. Complete a Finished Job

```python
if job_in_service != None and service_time_left == 0:
    jobs_complete.append((job_in_service,clock,0))
    job_in_service = None
```

When the remaining service time reaches `0`, the job is added to `jobs_complete`.

Setting:

```python
job_in_service = None
```

makes the server available for another job during the same timestep.

### 3. Add New Arrivals

```python
while len(jobs) and jobs[0][1] == clock:
    j = jobs.pop(0)
    work_queue.append(j)
```

`jobs[0][1]` is the arrival time of the first job still waiting to enter the system.

A `while` loop is used because multiple jobs can arrive at the same time. At timestep `8`, both:

```python
("d",8,2)
("e",8,3)
```

must be added.

`append()` puts each arriving job at the back of `work_queue`.

### 4. Send a Waiting Job to the Server

```python
if job_in_service == None and len(work_queue)>0:
    j = work_queue.pop(0)
    job_in_service = j[0]
    service_time_left = j[2]
```

`pop(0)` removes the first waiting job.

Because jobs are added with `append()` and removed with `pop(0)`, the queue is FIFO:

```text
first job in -> first job out
```

For example, at timestep `9`:

```text
[e][f](d)->
```

means:

- `d` is in service
- `e` is first in line
- `f` is behind `e`

### 5. Print the State and Advance Time

```python
print(''.join(
    ["[{id}]".format(id=j[0]) for j in work_queue]
    + ["({id})->".format(id=job_in_service)]
))

clock+=1
```

The output uses:

```text
[job]   = waiting in the queue
(job)-> = currently in service
```

Then `clock += 1` moves the simulation to the next timestep.

---

## 4. Expected Result

The jobs complete in this order:

```python
[
    ('a', 3, 0),
    ('b', 4, 0),
    ('c', 6, 0),
    ('d', 10, 0),
    ('e', 13, 0),
    ('f', 15, 0)
]
```

The important behavior is that a job may have to wait even after it has arrived.

For example:

```text
b arrives at 2, starts at 3
e arrives at 8, starts at 10
f arrives at 9, starts at 13
```

That waiting behavior is what makes this a queue simulation rather than just a list of independent jobs.

---

## 5. Lab Objectives

The task sheet places the main single-queue work under Objective B.

The relevant tasks include:

- create examples under [`./examples/obj-b/`](/examples/obj-b/)
- create example queue configurations
- create examples/pseudocode for server functions
- create example queue-metric files
- create example event logs
- write pseudocode and work through example input/output for [`./designs/obj-b/simple-queue-sim/`](/designs/obj-b/simple-queue-sim/)
- create the working implementation in [`./implementation/obj-b/simple-queue-sim/`](/implementation/obj-b/simple-queue-sim/)
- create automated tests for the simple queue simulator

Veras's [`q_ex-1.py`](./q_ex-1.py) is useful mainly as a reference/example for Objective B. It demonstrates:

| Objective B idea | Where it appears in [`q_ex-1.py`](./q_ex-1.py) |
| :--------------- | :--------------------------------------------- |
| simulation clock | `clock` and the `while` loop                   |
| source workload  | the `jobs` list                                |
| queue            | `work_queue`                                   |
| FIFO behavior    | `append()` + `pop(0)`                          |
| one server       | `job_in_service`                               |
| service duration | `service_time_left`                            |
| completed jobs   | `jobs_complete`                                |
| queue state      | the printed `[waiting](service)->` output      |

So to be clear, [`q_ex-1.py`](./q_ex-1.py) is not the final Objective B implementation, it's what Dr. Veras gave us as an example.

The task sheet says the real simple queue simulator must be implemented in C++ under [./implementation/obj-b/simple-queue-sim/](/implementation/obj-b/simple-queue-sim/).

The full lab also requires features that this small example does not include, such as:

- configurable queue size
- multiple servers
- server functions
- source/sink files
- queue metrics
- and event logs.
