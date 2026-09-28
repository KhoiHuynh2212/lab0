# Objective C: derive-sys-metrics

`derive-sys-metrics` converts a discrete-event simulation log into a time series of system-level response-time and throughput metrics for jobs that enter via sources and leave via sinks.

The program is written in **C++** (same toolchain as Objectives A and B) and lives in:

[`/implementation/obj-c/derive-sys-metrics/`](/implementation/obj-c/derive-sys-metrics/)

---

## 1. What the Program Does

For each job that enters through a source (`ARRIVE-VIA *.wl`) and later leaves through a sink (`DEPART-VIA *.wl`), the tool records response time = exit time − arrival time. Metrics are then aggregated **per exit time-step**:

```text
┌──────────────┐     ┌────────────────────┐     ┌─────────────────┐
│  jobin.log   │ ──→ │ derive-sys-metrics │ ──→ │  sysmetric.csv  │
└──────────────┘     └────────────────────┘     └─────────────────┘
```

| Output column | Meaning |
| --- | --- |
| `time-step` | Simulation time when one or more jobs exited via a sink |
| `Avg-response-time` | Mean response time of jobs exiting at that step (rounded to ones) |
| `median-response-time` | Median response time of those jobs |
| `10-response-time` | 10th-percentile response time (nearest-rank) |
| `90-response-time` | 90th-percentile response time (nearest-rank) |
| `throughput` | Number of jobs exiting via a sink at that step |

Edge hops (`ARRIVE-VIA` / `DEPART-VIA` targets that are not `*.wl` workload paths) are ignored so the series reflects end-to-end system sojourn, not intermediate queue transfers.

---

## 2. Input Event Log

The file must begin with a header line (`Event Log`). Each following non-empty line has this token layout:

| Field | Example | Meaning |
| --- | --- | --- |
| 0 | `0`, `10`, `20` | Timestamp |
| 1 | `j0`, `j1` | Job identifier |
| 2 | `IN` | Literal keyword |
| 3 | `q0` | Queue identifier |
| 4+ | `ARRIVE-VIA in0.wl` / `DEPART-VIA out0.wl` | Event action and metadata |

Only `ARRIVE-VIA` / `DEPART-VIA` lines whose via-target ends in `.wl` affect the metrics. Other event kinds (`ENTERS-SERVER`, `TASK-START`, …) may appear and are skipped.

Example input:

```text
Event Log
0 j0 IN q0 ARRIVE-VIA in0.wl
0 j0 IN q0 ENTERS-SERVER 0
20 j0 IN q0 EXITS-SERVER 0
20 j0 IN q0 DEPART-VIA out0.wl
10 j1 IN q0 ARRIVE-VIA in0.wl
30 j1 IN q0 DEPART-VIA out0.wl
```

---

## 3. Output CSV

Exact header:

```csv
time-step,Avg-response-time,median-response-time,10-response-time,90-response-time,throughput
```

One output row per distinct sink-exit time-step (chronological). Time-steps with no sink departures are omitted.

---

## 4. Command-Line Interface

```bash
./derive-sys-metrics <input.log> <output.csv>
./derive-sys-metrics --help
```

| Argument | Meaning |
| --- | --- |
| `input.log` | Source event-trace file |
| `output.csv` | Derived system-metrics output file |
| `--help`, `-h` | Show usage text |

Example:

```bash
./derive-sys-metrics jobin.log sysmetric.csv
```

---

## 5. Build and Test (Linux)

Requires `g++` (C++20), `make`, and Valgrind for the test suite.

```bash
make -C implementation/obj-c/derive-sys-metrics build
make -C implementation/obj-c/derive-sys-metrics test
```

---

## 6. Implementation Notes

- Language and standard: C++20 with `g++`, matching Obj-A / Obj-B / sibling Obj-C tools.
- Response time: first source `ARRIVE-VIA *.wl` per job to matching sink `DEPART-VIA *.wl`.
- Percentiles: nearest-rank on the sorted per-step sample (`rank = ceil(p/100 · n)`).
- Median of an even count: rounded mean of the two central values.
- Tests: Valgrind memcheck plus optional `gcov` coverage, same pattern as filter-by-job / compute-use.

---

## 7. Related Documentation

- Implementation README: [`/implementation/obj-c/derive-sys-metrics/README.md`](/implementation/obj-c/derive-sys-metrics/README.md)
- Upstream event log: [`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)
- Sibling filter: [`/designs/obj-c/filter-by-job/README.md`](/designs/obj-c/filter-by-job/README.md)
- Repository overview: [`/README.md`](/README.md)
