# 📈 Objective C: compute-use

`compute-use` turns a chronological queue-metrics CSV (typically pre-filtered by `filter-by-queue`) into rolling USE-method history: Utilization, Saturation, and Errors at windows of size 10, 100, and 1000.

---

# 1. What the Program Does

For every input row at time step $t$, the tool evaluates three metrics over the last $W$ samples ($W \in \{10, 100, 1000\}$), using the prefix of length $k = t + 1$ until the window is full.

```txt
┌──────────────┐     ┌─────────────┐     ┌──────────────┐
│   qmet.csv   │ ──→ │ compute-use │ ──→ │   quse.csv   │
└──────────────┘     └─────────────┘     └──────────────┘
```

|   Metric    | Meaning                                         | Formula over window of size $k$                                                                             |
| :---------: | :---------------------------------------------- | :---------------------------------------------------------------------------------------------------------- |
| Utilization | % of time a job is in service (rounded to ones) | $\mathrm{round}\!\left(\frac{1}{k}\sum u_i\right)$ where $u_i = 1$ if `num-jobs-in-service` $> 0$, else $0$ |
| Saturation  | Average jobs queueing (rounded to ones)         | $\mathrm{round}\!\left(\frac{1}{k}\sum\right.$ `num-jobs-in-queue`$_i\left.\right)$                         |
|   Errors    | Total errors seen in the window                 | $\sum$ `Num-of-errors`$_i$                                                                                  |

Output `time-step` is taken from the input row. Prefer feeding a single queue (e.g. after `filter-by-queue`) so the series is one queueing system.

---

# 2. Input CSV

Sorted by `time-step`. Column names match Objective B metrics (case and punctuation are ignored when locating columns):

```csv
time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors
0,0,1,1,0
1,0,1,1,0
2,0,3,1,0
```

|         Field         | Meaning                                                      |
| :-------------------: | :----------------------------------------------------------- |
|      `time-step`      | Discrete clock tick                                          |
|    `queue-sys-id`     | Queue identifier (often a single id after `filter-by-queue`) |
|  `num-jobs-in-queue`  | Waiting jobs ($N_Q$)                                         |
| `num-jobs-in-service` | Jobs in service ($N_S$)                                      |
|    `Num-of-errors`    | Errors recorded at that step                                 |

---

# 3. Output CSV

Exact header:

```csv
time-step,Utilization(10),Saturation(10),Errors(10),Utilization(100),Saturation(100),Errors(100),Utilization(1000),Saturation(1000),Errors(1000)
```

One output row per input data row, carrying the three metrics for each of the three windows.

---

# 4. Command-Line Interface

```bash
./compute-use <input.csv> <output.csv>
./compute-use --help
```

|    Argument    | Meaning                   |
| :------------: | :------------------------ |
|  `input.csv`   | Source queue-metrics file |
|  `output.csv`  | USE-metrics output file   |
| `--help`, `-h` | Show usage text           |

Example:

```bash
./compute-use qmet.csv quse.csv
```

---

# 5. Build and Test (Linux)

Requires `g++` (C++20), `make`, and Valgrind for the test suite.

```bash
make -C implementation/obj-c/compute-use build
make -C implementation/obj-c/compute-use test
```

---

# 6. Implementation Notes

- Language and standard: C++20 with `g++`, matching Obj-A / Obj-B / filter-by-queue / filter-by-job.
- Sliding windows: three fixed-capacity deques with running sums for $O(1)$ updates per row.
- Header matching: alphanumeric-normalized column lookup, same style as Obj-B / filter-by-queue.
- Tests: Valgrind memcheck plus optional `gcov` coverage, same pattern as sibling Obj-C tools.

---

# 7. Related Documentation

- Implementation README: [`/implementation/obj-c/compute-use/README.md`](/implementation/obj-c/compute-use/README.md)
- Upstream filter: [`/designs/obj-c/filter-by-queue/README.md`](/designs/obj-c/filter-by-queue/README.md)
- Repository overview: [`/README.md`](/README.md)
