# Objective D: Car Wash Modeling and Analysis

Status: the car-wash study, all 60 experiments, nine Objective C tool demonstrations, and final interpretation are complete under the documented measurement limitations. Start with the [page-26 answer map](#81-assignment-answer-map) and [measured conclusions](#82-demand-and-scale-results). See the [analysis guide](results/analysis/README.md) for C's actual output conventions: reported utilization is a rounded activity indicator, not a capacity percentage. Numerical settings are assumptions for a synthetic study, not measurements from an operating car wash.

This document describes the Objective D model, experiment design, validation evidence, and reproduction instructions. It uses the existing Objective A and B tools without modifying their implementations.

## 1. Introduction and research questions

We model a car wash with one shared waiting line feeding identical wash bays. A car occupies its bay for the entire wash. The baseline has one bay and space for 20 waiting cars. The lot size allows every required percentage change to be represented exactly; it is not a surveyed site capacity.

The study asks:

1. How do light, near-capacity, and over-capacity arrival demand affect waiting, completed washes, and rejected cars?
2. Does additional waiting space improve the customer experience, or mainly change which cars are admitted?
3. How does adding a second bay compare with enlarging the waiting lot?
4. Do short and longer experiments give consistent explanations of system behavior?
5. What recommendations are supported by the results, and what remains outside the model?

These questions address the modeling, metrics, workload justification, scale comparisons, resource sensitivity, and practical takeaways required by Objective D on page 26 of the course handout, **Queue Lab.pdf**.

## 2. Model boundary and assumptions

```txt
Arriving cars -> One FIFO waiting line -> Available wash bay -> Completed cars
                                              |
                            Pre-rinse -> Soap -> Scrub -> Rinse -> Dry
```

The five wash stages happen sequentially in the same bay. This is not a conveyor tunnel with separate stations washing several cars at once. A second bay is a second server behind the same queue, so it remains a single-queue model suitable for D.

| Real process | Simulator representation |
|---|---|
| One car | One job with a unique ID |
| Waiting driveway/lot | Bounded FIFO queue `q0` |
| One wash bay | One server |
| Wash stages | Five ordered tasks |
| Car turned away from a full lot | `ERROR QUEUE-FULL` |
| Completed wash | Departure to the sink with no remaining tasks |

Assumptions:

- One timestep represents 10 seconds.
- One work unit requires one timestep with `Sub-1*` and operation duration 1.
- All cars receive the same package and service requirement.
- Cars do not abandon the queue; rejected cars do not retry.
- Payment, entering/leaving the bay, equipment failure, staffing, and water constraints are not modeled separately.
- A car stays assigned to its bay until every task finishes; service is not preempted.
- Random arrival counts are independent across timesteps within a scenario.
- The longer horizon represents continuous operating time, not days separated by overnight closures.

### Wash stages

| Task ID | Stage | Work | Baseline duration |
|---|---|---:|---:|
| 0 | Pre-rinse | 5 | 50 seconds |
| 1 | Soap | 8 | 80 seconds |
| 2 | Scrub | 10 | 100 seconds |
| 3 | Rinse | 6 | 60 seconds |
| 4 | Dry | 5 | 50 seconds |
| | Total | 34 | 340 seconds: 5 minutes 40 seconds |

These are assumed stage durations, not validated industry averages.

## 3. Theory and metrics

### Arrival demand and capacity

This is a discrete-time model with Poisson arrival counts, deterministic service, and finite waiting capacity. It resembles a finite-capacity M/D/1 queue, but arrivals are grouped into integer timesteps rather than simulated at continuous times.

Let `lambda` be offered cars per timestep, `c` be bay count, and `S = 34` be service duration. Nominal processing capacity is `c / 34` cars per timestep. One bay can process about 0.0294 cars per timestep, or 10.59 cars per hour.

Offered load is `lambda * 34 / c`. This ratio can exceed 1; measured utilization cannot exceed 100%.

A finite queue cannot grow beyond its limit. Persistent overload fills the lot and can cause rejection. Even demand below nominal capacity can produce rejections during random bursts.

### Measurements

| Metric | Definition and purpose |
|---|---|
| Capacity utilization (desired metric) | Busy-bay time divided by available bay time; normalize by bay count. The supplied C implementation does not produce this measurement; see section 6 |
| Saturation | Number of waiting cars; excludes cars in bays |
| Rejections | Count and fraction of offered cars rejected because the lot is full |
| Error rate | New errors divided by interval duration; B's error column is cumulative. The current C output, after input adaptation, is a window count rather than a rate |
| Throughput | Completed cars per unit of elapsed simulated time |
| Waiting time | First service start minus arrival |
| Service time | Final service exit minus first service start; expected to be 34 steps |
| Total time / turnaround | Final sink departure minus original arrival |
| Unfinished cars | Accepted cars still waiting or in service at an observation boundary |

C uses "response time" differently in its job and system metrics. Job response time is time until first service; system response time is arrival-to-departure time. Label the actual interval explicitly in tables and figures.

USE measurements use the assignment's 10-, 100-, and 1000-step windows. System metrics include mean, median, 10th percentile, and 90th percentile total times for departures at each timestep, plus throughput. No departures means latency is undefined for that timestep, not a measured zero.

The analysis uses C unchanged and distinguishes these desired definitions from its actual outputs. Its `Utilization(W)` is 0 or 1 after rounding the fraction of samples with at least one busy bay; its saturation is a rounded waiting average. The meaning of every reported field and the count-versus-rate distinction are documented in the [analysis guide](results/analysis/README.md).

Always report rejection alongside latency: lower waiting among completed cars can coexist with more customers turned away.

### Little's Law and interpretation

For consistently defined system boundaries and observations, Little's Law relates average occupancy, accepted arrival rate, and average total time: `L = lambda_accepted * W`. Occupancy includes waiting and in-service cars. Immediately rejected arrivals are not accepted demand.

A finite-run cross-check can start empty, follow all accepted cars through completion, and compare summed total times with occupancy integrated over the same interval. Keep time boundaries consistent; the terminal empty metrics row is not an extra elapsed interval.

Extra waiting space does not increase bay processing capacity. It can change acceptance, waiting, and observed completions. Extra bays raise nominal capacity, but their measured benefit depends on demand and the observation interval. These mechanisms motivate experiments rather than establish the results in advance.

## 4. Mapping to the simulator

### Baseline configuration

The [configuration template](configs/carwash-template.csv) defines the baseline fields. The validation script replaces its `WORKLOAD_PATH` and `COMPLETED_PATH` placeholders and writes a runnable configuration for each case. Do not run the template directly. For the saved six-car run, [runs/smoke/config.csv](runs/smoke/config.csv) contains:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,20,1,Sub-1*,1,../../workloads/smoke.csv,completed.csv,[],[]
```

`Sub-1*` repeats one-unit processing until all tasks finish. Without `*`, the car would leave service after one operation with unfinished tasks and produce `NO-OUT-EDGE`, since this model has no outgoing edges.

`Server Function Rate = 1` means one timestep per operation, not one completed car per timestep. The queue limit counts waiting cars only: 20 waiting plus one in service means 21 cars can be present.

Source and sink paths are relative to the configuration file. Metrics and event paths supplied to B are relative to the working directory. The D scripts resolve these paths explicitly and isolate every run's outputs.

### Small validation workload

[workloads/smoke.csv](workloads/smoke.csv) contains six hand-selected arrivals. It is a correctness example, not a generated Poisson workload or measured customer data.

These hand-calculated expectations were verified against B's actual output by the car-wash validation script:

| Car | Arrival | Expected start | Expected finish | Expected wait | Expected total time |
|---|---:|---:|---:|---:|---:|
| j0 | 0 | 0 | 34 | 0 | 34 |
| j1 | 4 | 34 | 68 | 30 | 64 |
| j2 | 9 | 68 | 102 | 59 | 93 |
| j3 | 10 | 102 | 136 | 92 | 126 |
| j4 | 15 | 136 | 170 | 121 | 155 |
| j5 | 22 | 170 | 204 | 148 | 182 |

Acceptance criteria: six completed cars, no errors, FIFO service, 34 service steps per car, and last departure at 204.

### Experimental workloads using A

A draws the number of arrivals at each integer timestep. Its fixed-work mode assigns the same amount to every task and cannot directly express our five different stage amounts.

The [preparation script](scripts/prepare_workloads.py) performs these steps:

1. Invoke A with a scenario's Poisson parameter, inclusive interval, and seed.
2. Preserve the generated job IDs and arrival times.
3. Replace each task list with `['0: W 5', '1: W 8', '2: W 10', '3: W 6', '4: W 5']`.
4. Save the original A output, transformed workload, generator command, and seed.

It invokes A with one fixed placeholder task of work 1 per car; that placeholder is replaced, not added to the car-wash tasks. A generates all randomness. The Python transformation does not sample arrivals or service times.

Every prepared workload is stored in `workloads/generated/<workload-id>/` with:

- `generator-output.csv`: original, unmodified output from A.
- `workload.csv`: the same IDs and arrival times, with five wash tasks totaling 34 work units.
- `metadata.json`: parameters, exact commands, hashes, count, and verification results.

The script generates each original workload twice and compares its bytes, then checks the transformed file's IDs, arrival order/range, task list, and total work. All 18 workloads passed these checks. The six seed-42 workloads also matched an earlier pilot generated in a different output directory. Repeatability is established for this compiler/runtime environment; standard-library distribution implementations can differ across toolchains. The saved CSV files are the reference inputs for paired resource comparisons.

Build provenance and generation settings are in [workloads/preparation.json](workloads/preparation.json). The script compiles A into a local ignored build directory without changing shared code or invoking B.

Poisson counts represent assumed independent random demand, not a claim about measured car-wash arrivals. Fixed service isolates the effects of arrivals and resources. A's visualizer can plot arrivals now; the task-count scatter will be constant at five.

## 5. Experiment design

### Demand and scale

| Scenario | Poisson lambda: cars/step | Expected offered cars/hour | Offered load: one bay |
|---|---:|---:|---:|
| Light | 0.010 | 3.6 | 34% |
| Near capacity | 0.025 | 9.0 | 85% |
| Over capacity | 0.040 | 14.4 | 136% |

Use arrival horizons of 2,880 steps (8 operating hours) and 28,800 steps (80 continuous operating hours). For horizon `H`, A's inclusive interval is `0,H-1`. These are chosen scales; D does not prescribe exact job counts.

Start with seed 42 while validating the workflow. Final comparisons use seeds 42, 43, and 44. Reuse the exact workload file across resource variants within each demand/horizon/seed combination. Report each seed and the spread; three seeds support an exploratory study, not strong statistical certainty.

The inputs for all three seeds are now prepared. For the same demand and seed, the short workload is the prefix of the longer arrival sequence. Horizon comparisons are therefore paired extensions, not independent replications. Different demand scenarios also reuse the listed seed values; do not treat them as additional independent samples.

Run B until the queue drains. Distinguish the arrival window `[0,H)` from the later clearing period. At the arrival cutoff, report unfinished cars. After full drain, account for every offered car as completed or rejected. Every throughput rate must identify its time denominator. If B drains before `H`, arrival-window calculations must include the remaining idle time.

### Required waiting-space sensitivity

Hold bays, wash duration, and workload fixed; vary waiting space only:

| Change | Waiting spaces |
|---|---:|
| Baseline | 20 |
| +25% | 25 |
| +50% | 30 |
| +100% | 40 |
| -10% | 18 |
| -20% | 16 |
| -30% | 14 |

These changes meet the required percentages exactly. Use the longer horizon, near-capacity and over-capacity demand, and all three seeds.

### Separate bay comparison

Compare one versus two identical bays with 20 waiting spaces. Use the same longer-horizon near-capacity and over-capacity workloads and seeds. This distinguishes doubling service resources from doubling waiting space.

### Experiment manifest

[experiments.csv](experiments.csv) specifies the 60 runs. Their completed execution status and raw counts are recorded separately in [run-summary.csv](results/tables/run-summary.csv); the manifest remains the input specification.

| Family | Combinations | Planned runs |
|---|---|---:|
| Baseline | 3 demands x 2 horizons x 3 seeds | 18 |
| Waiting-space sensitivity | 6 nonbaseline capacities x 2 demands x 3 seeds, long horizon | 36 |
| Additional bay | Two-bay setting x 2 demands x 3 seeds, long horizon | 6 |
| Total | Baselines reused for comparison | 60 |

Each row names its input file and SHA-256 hash, lambda, horizon, seed, queue size, bay count, function, operation duration, stop mode, and physical time unit. `workload_file` is relative to the directory containing the manifest, not the current working directory. `baseline_run_id` identifies the matching one-bay/20-space run; baseline rows refer to themselves. `resource_change_percent` applies only to `changed_resource`, not to a combined measure of all resources.

All variants use exactly the same workload path and hash as their baseline. The six small validation cars and the overflow fixture are separate from these 60 planned experiments.

### Hypotheses, not results

- Higher demand should increase busy time and pressure on the waiting lot.
- A larger lot may admit more cars but permit longer waiting; it does not speed up a wash.
- A smaller lot may reduce measured waiting among completed cars while rejecting more arrivals.
- A second bay should relieve congestion when one bay limits processing, but may remain partly idle under low demand.
- Longer runs should help distinguish transient behavior from sustained overload.

If a required resource change has little effect, report and explain that outcome rather than force the expected direction.

## 6. Analysis workflow and Objective C contract

The [analysis script](scripts/analyze_results.py) uses C commit `c65b1516fc98a5fc30115c4827a851fecadbc0d4` unchanged. It prepares compatible input copies, invokes the tools, checks results against the original B evidence, and consolidates outputs. Original B files and schemas remain unchanged.

| Objective C tool | Completed demonstration |
|---|---|
| `filter-by-queue` | Select `q0` from queue measurements |
| `filter-by-job` | Select immediate-service, waiting, and rejected cars |
| `compute-use` | Compute C's rounded activity indicator, rounded saturation, and new-error counts with 10/100/1000-step windows |
| `plot-use` | Plot USE measurements over time |
| `derive-sys-metrics` | Derive departure throughput and total-time statistics |
| `plot-sys-metrics` | Plot throughput and total time alongside congestion |
| `derive-event-metrics` | Derive waiting, service, and turnaround per car |
| `plot-derive-event-metrics` | Plot distributions and arrival-versus-wait/turnaround comparisons |
| `create-event-trace` | Trace the small example and selected congestion events |

The [analysis guide](results/analysis/README.md) links every tool's retained evidence. The [analysis execution report](results/analysis/execution.json) records exact commands, source/input/output hashes, the three small validation cases, all 60 runs, and figure captions. Even the single-queue filter is executed and checked.

Compatibility copies convert B's cumulative errors to per-step increments; add the header expected by C's job filter; and add a header plus recorded `.wl` source/sink aliases for C's system parser. Known idle time is appended when a run drains before the arrival horizon. Rejected cars remain explicitly classified and are excluded from completed-customer timing statistics. The adapters reject unexpected boundaries and decreasing counters instead of silently changing their meaning.

C's activity indicator cannot distinguish 60% from 90% busy time after rounding and cannot normalize two-bay capacity. Its error plot says "Error Rate" but displays window counts. These are disclosed limitations; D does not silently replace C's calculations or claim accurate capacity percentages. All 60 saved B experiments were reused. Only an additional four-car/two-bay validation case was newly simulated to check tied departures and cutoff behavior.

## 7. Input and execution evidence

Raw counts and correctness evidence are available below. Derived timing, throughput, and C's documented USE outputs are in the [analysis package](results/analysis/README.md). The evidence includes:

1. The validated six-car example and event trace.
2. Demand/horizon comparisons: offered, completed, rejected, unfinished at cutoff, and performance measurements.
3. Waiting-space sensitivity with paired workloads and exact resource changes.
4. One-bay versus two-bay comparisons.
5. Evidence demonstrating all required accessory tools.
6. Cross-checks among job timings, occupancy and counts, plus verification of C's actual rolling-window formulas.

Section 8 connects the settings and evidence to the assignment questions and car-wash implications. The earlier hypotheses remain separate from these measured findings.

### 7.1 Prepared input measurements

The [workload summary](results/tables/workloads.csv) records expected and actual arrival counts, observed input rates, first/last arrivals, and file hashes. Actual counts fluctuate because Poisson generation does not force exactly `lambda * horizon` cars.

| Demand | Horizon | Expected cars | Seed 42 | Seed 43 | Seed 44 |
|---|---:|---:|---:|---:|---:|
| Light | 2,880 | 28.8 | 33 | 30 | 30 |
| Light | 28,800 | 288 | 295 | 286 | 268 |
| Near capacity | 2,880 | 72 | 76 | 56 | 79 |
| Near capacity | 28,800 | 720 | 696 | 692 | 661 |
| Over capacity | 2,880 | 115.2 | 114 | 93 | 118 |
| Over capacity | 28,800 | 1,152 | 1,107 | 1,111 | 1,112 |

These are input measurements, not throughput, completed-car counts, or simulator performance results.

Objective A created the following plots using the longer-horizon seed-42 inputs. Each arrival histogram uses the same axes and 20 bins of 1,440 steps (4 hours), so the heights can be compared directly. Histograms show cars arriving during time intervals, not a fitted Poisson probability distribution. They do not establish how long cars wait.

| Plot | What it demonstrates |
|---|---|
| [Light-demand arrivals](results/figures/workloads/light-h28800-seed42-arrivals.png) | Arrival counts over time at light demand |
| [Near-capacity arrivals](results/figures/workloads/near-h28800-seed42-arrivals.png) | Greater demand on the same time and count scales |
| [Over-capacity arrivals](results/figures/workloads/over-h28800-seed42-arrivals.png) | Higher input demand; this input plot alone does not measure rejection or waiting |
| [Tasks per car](results/figures/workloads/near-h28800-seed42-tasks.png) | All transformed jobs have five tasks; equal counts alone do not establish task durations |

Plot commands, input IDs, image hashes, and axis settings are recorded in [plot metadata](results/figures/workloads/metadata.json). The workload checks separately verify the exact stage durations and 34-unit total.

### 7.2 Measured validation results

Both cases were run using the unchanged Objective B implementation at source commit `53a42b6`, compiled locally as described below. These are deterministic correctness cases, not statistical experiments.

| Case | Offered | Completed | Rejected | Last departure | Result |
|---|---:|---:|---:|---:|---|
| Six-car FIFO example | 6 | 6 | 0 | 204 steps | Matches every expected start and finish |
| Full-lot boundary: 22 cars at time 0 | 22 | 21 | 1 | 714 steps | One car in service plus 20 waiting; `j21` rejected |

Every admitted car received exactly 34 service steps. Completed task lists were empty, waiting/busy counts stayed within bounds, all admitted work drained, and there were no `NO-OUT-EDGE` errors. The checks compare event arrivals, service entries/exits, departures, sink rows, and cumulative errors; they do not simply trust the terminal summary.

The overflow case demonstrates that queue capacity excludes the car already in service. The six-car case demonstrates that a car occupies its bay continuously through all five stages. A correct simulation can now support the later experiments, but these two cases do not establish a recommended lot size or bay count.

Evidence:

- Six-car [workload](workloads/smoke.csv), [configuration](runs/smoke/config.csv), [completion file](runs/smoke/completed.csv), [queue measurements](runs/smoke/qmet.csv), [events](runs/smoke/job.log), and [verification](runs/smoke/verification.json).
- Overflow [workload](workloads/overflow.csv), [configuration](runs/overflow/config.csv), [completion file](runs/overflow/completed.csv), [queue measurements](runs/overflow/qmet.csv), [events](runs/overflow/job.log), and [verification](runs/overflow/verification.json).
- Run provenance: [six-car metadata](runs/smoke/metadata.json), [overflow metadata](runs/overflow/metadata.json), and [build/test evidence](runs/build-checks.json). Each run also contains `stdout.txt` and `stderr.txt`.

### 7.3 Completed experiments: accounting and cutoff behavior

All 60 experiments completed and passed the run verifier: 18 baselines, 36 waiting-space variants, and six two-bay variants. The [execution report](runs/experiments/execution.json) records the build, source hashes, selected/completed IDs, and overall status. The [run summary](results/tables/run-summary.csv) records raw counts and bounds for each run.

The verifier checked each car's arrival, FIFO service order, 34-step service duration, operation-event counts, bay assignment, and final outcome. It also reconciled events with every measurement timestep: `waiting + busy = arrivals - rejections - departures`, busy bays equal service entries minus exits, and cumulative errors equal rejection events. Every completed sink row matched its departure event and had no remaining tasks. No unexpected `NO-OUT-EDGE` errors occurred.

Across the 60 runs, 44,510 offered-car instances were accounted for as 39,339 completions and 5,171 rejections. These totals include repeated workloads under different resource settings; they are not 44,510 distinct customers in one system. The saved evidence contains 1,513,962 queue-measurement rows and 2,842,750 event lines.

**Arrival window versus draining.** For horizon `H`, arrivals occur at timestamps in `[0,H)`. `completed_before_cutoff` counts departures strictly before `H`. `unfinished_at_cutoff` is the state after timestep `H-1`, before processing events at `H`; a departure exactly at `H` is therefore outside that window. The verifier checks:

```txt
offered = completed + rejected                 (after the system drains)
completed = completed_before_cutoff + unfinished_at_cutoff
unfinished_at_cutoff = waiting_at_cutoff + busy_at_cutoff
```

All `unfinished_at_cutoff` cars eventually finish in these drain runs. A simulator that drains before `H` has zero unfinished cars; any remaining time up to `H` is idle. `metric_rows` includes the final empty row and must not be mistaken for elapsed duration in a throughput calculation. `last_departure` is a simulated timestamp, while the metadata's `execution_wall_seconds` measures the program's runtime on the host computer.

For the 28,800-step overloaded inputs, the following selected comparisons illustrate the distinction:

| Seed | Waiting spaces / bays | Offered | Completed before cutoff | Unfinished at cutoff | Completed after draining | Rejected |
|---|---|---:|---:|---:|---:|---:|
| 42 | 20 / 1 | 1,107 | 843 | 18 | 861 | 246 |
| 42 | 40 / 1 | 1,107 | 843 | 38 | 881 | 226 |
| 42 | 20 / 2 | 1,107 | 1,107 | 0 | 1,107 | 0 |
| 43 | 20 / 1 | 1,111 | 843 | 20 | 863 | 248 |
| 43 | 40 / 1 | 1,111 | 843 | 40 | 883 | 228 |
| 43 | 20 / 2 | 1,111 | 1,111 | 0 | 1,111 | 0 |
| 44 | 20 / 1 | 1,112 | 842 | 20 | 862 | 250 |
| 44 | 40 / 1 | 1,112 | 842 | 40 | 882 | 230 |
| 44 | 20 / 2 | 1,112 | 1,108 | 4 | 1,112 | 0 |

For these overloaded inputs, doubling waiting space admits 20 additional cars but does not change how many finish before the cutoff. The additional completions occur while clearing the larger backlog. With two bays, no cars are rejected in these cases, though seed 44 still has four unfinished cars at cutoff. This is evidence for distinguishing storage capacity from processing capacity; the [derived timing results](results/analysis/README.md) quantify the waiting-time tradeoff.

At near-capacity demand, the longer one-bay baselines reached maximum waiting counts of 11, 9, and 7 for seeds 42, 43, and 44. All are below the smallest tested lot of 14, so changing waiting capacity caused no rejections or change in completion counts in those cases. A prescribed resource change need not produce a measurable effect when the resource is not limiting.

### 7.4 Evidence files and verification tests

Each directory under [runs/experiments](runs/experiments/) is named by the manifest's `run_id` and contains four files:

| File | Purpose |
|---|---|
| `config.csv` | Exact B configuration, with source relative to this run directory and local sink |
| `completed.csv` | B's completed-car output; its time column contains departure timestamps |
| `qmet.csv` | B's original per-timestep waiting, busy, and cumulative-error measurements |
| `job.log` | B's original arrival, task, service, departure, and error events |

The shared [execution report](runs/experiments/execution.json) contains `runs`, keyed by `run_id`. Each entry has `metadata` (settings, command including captured `stdout`/`stderr`, source/input/output hashes, timestamps, runtime, and build reference) and `verification` (accounting/timing checks, cutoff definition, and summary). A failed run records its error there while preserving earlier successful records and any raw output. The report references the existing [manifest](experiments.csv) by path and SHA-256 hash instead of duplicating it. The compact [run summary](results/tables/run-summary.csv) remains the convenient comparison table.

The 241 raw/provenance files total approximately 131 MB before Git compression; they preserve the evidence needed by C. Consolidating bookkeeping reduces the file count, while the detailed measurements and event logs still account for most storage. The largest individual file is approximately 3 MB. Existing six-car/overflow validation outputs retain their original layout.

The saved report uses `layout_version: 2`. These existing results were consolidated without rerunning simulations: all 240 configuration/raw-output files and the run-summary bytes were preserved. Original execution dates and script hashes still describe the code that actually ran; `evidence_packaging` separately records the consolidation. For these migrated records, `captured_stream_files_sha256` preserves historical hashes of the former stdout/stderr files, whose text remains in `metadata.command.stdout` and `metadata.command.stderr`. Newly executed runs record the captured text directly without creating separate text files.

The detailed logs are large because B records each `Sub-1` operation: an accepted car produces 34 `TASK-START` and 34 `TASK-STOP` events, in addition to arrival, server entry/exit, and departure. The five physical wash stages therefore do not imply only five start/stop pairs. A rejected car produces an arrival and an error. The raw event format is preserved for C; whole-car service duration is checked using server entry and exit.

The five preliminary runs matched the final runs' completed and metrics files byte-for-byte. Their event content also matched, allowing for the different relative source-path text in the preliminary output location.

Twelve focused tests in [test_experiment_runner.py](tests/test_experiment_runner.py) passed. They cover known valid examples, the exact cutoff boundary, an idle tail, duplicate sink cars, incorrect error counts, incorrect queue counts that are still within capacity, unexpected error types, a missing task event, a bad input hash, and protection of existing output. Two orchestration tests use saved fixture outputs as a stand-in for B to check the four-file layout, shared successful records, and preservation of diagnostics and earlier results when a later run fails. Tests use temporary copies rather than modifying saved evidence; these orchestration tests do not rerun the simulator.

These checks establish consistency and correctness for this model. They are not a replacement for C's required derived metrics and plots, nor a new claim of Valgrind, sanitizer, or coverage testing.

### 7.5 Derived measurements using unchanged C

All 60 runs passed the analysis checks under C's documented measurement contract. The [performance summary](results/analysis/tables/performance-summary.csv) contains per-run completed-customer timing statistics and rates; [paired comparisons](results/analysis/tables/paired-comparisons.csv) quantify all 42 nonbaseline resource changes. The package consolidates 44,510 per-car rows, 39,284 departure-time rows, 300 selected USE rows, twelve native example outputs, and sixteen figures in approximately 6.4 MB.

For overloaded long-horizon seed 42, mean completed-customer waiting was 99.29 minutes with 20 spaces, 199.57 minutes with 40 spaces, and 2.18 minutes with two bays and 20 spaces. Both one-bay cases completed 843 cars before the cutoff; two bays completed 1,107. Rejections were 246, 226, and zero respectively. Mean waits under every near-capacity waiting-space variant were identical to the matching baseline, consistent with their unused lot capacity. These are measured conditional timing/count comparisons, not conclusions derived from C's coarse activity indicator.

The adapter/analysis and earlier runner tests total 25 passing Python tests. Five existing C++ unit-test programs passed in isolated native builds. Each C-derived customer timing and system departure statistic was checked against original events/sink rows; waiting/service durations were reconciled with occupancy. All 1,514,440 experimental USE rows were checked at all three window sizes. Measurement limitations and figure interpretation are explained in the [analysis guide](results/analysis/README.md).

## 8. Assignment answers and measured conclusions

### 8.1 Assignment answer map

This table answers every prompt on page 26 of **Queue Lab.pdf** and points to the supporting model, measurements, or interpretation. The [analysis guide](results/analysis/README.md#5-nine-tool-coverage-and-validation) also demonstrates all nine accessory tools required by the repository checklist.

| Assignment prompt | Answer and supporting evidence |
|---|---|
| Pick a real-world single-queue process | A car wash with one shared FIFO waiting lot and identical full-service bays; [process and scope](#2-model-boundary-and-assumptions). |
| Map the process to a queueing problem | Cars are jobs, waiting spaces form the bounded queue, bays are servers, and wash stages are ordered tasks. A full lot rejects arrivals; [mapping table](#2-model-boundary-and-assumptions). |
| Which metrics matter? | Waiting and total visit times, throughput, rejection, saturation, unfinished cars, and capacity utilization. C only supplies a coarse activity indicator for the last metric; [definitions](#measurements) and [actual USE interpretation](#85-interpreting-use-throughput-and-latency). |
| Map it to the simulator and configure files | Queue `q0`, 20 waiting spaces, one `Sub-1*` server with rate 1, five tasks totaling 34 steps, and explicit source/sink paths; [configuration and verified example](#4-mapping-to-the-simulator). |
| Generate the workload and explain why | A generates Poisson arrivals at three assumed demand levels; D preserves arrival times and assigns the fixed wash stages. Two horizons and three seeds test demand and scale; [workload method](#experimental-workloads-using-a) and [experiment design](#5-experiment-design). |
| Interpret USE, throughput, and response time/latency | Read congestion and rejection alongside completed-customer waits; distinguish window counts from rates and waiting from whole-visit time; [interpretation](#85-interpreting-use-throughput-and-latency). |
| Run at various scales; explain behavior and whether it makes sense | All 60 runs completed. Persistent overload increases waiting and rejection while wash throughput approaches the one-bay limit; short samples can conceal this. [All baseline seeds and horizons](#82-demand-and-scale-results). |
| Increase resources by 25%, 50%, and 100% | Waiting spaces increase from 20 to 25, 30, and 40. Under overload this admits more cars but increases waiting without increasing arrival-window throughput. Doubling bays is a separate service-capacity comparison; [lot sensitivity](#83-waiting-space-sensitivity) and [bay sensitivity](#84-one-bay-versus-two-bays). |
| Decrease resources by 10%, 20%, and 30% | Waiting spaces decrease to 18, 16, and 14. Under overload more cars are rejected, while the completed-customer mean wait decreases. Near-capacity lot variants have no measured effect; [results and explanation](#83-waiting-space-sensitivity). |
| What would you tell someone building this process? | Identify the limiting resource, evaluate admission and timing together, allow for clearing after arrivals stop, and collect operational/cost data before choosing a design; [recommendations and limits](#87-practical-recommendations-and-limits). |

### 8.2 Demand and scale results

The table below reports each baseline seed separately. All baselines use 20 waiting spaces and one bay. Waiting statistics include every admitted car followed through final completion, even if it finishes after arrivals stop. Throughput uses the common arrival window, not the extra clearing period. Rejected cars are excluded from timing statistics. Counts and more precise values are in the [60-run performance summary](results/analysis/tables/performance-summary.csv).

| Demand | Arrival hours | Seed | Mean wait (min) | P90 wait (min) | Washes/hour before cutoff | Rejected / offered | Unfinished at cutoff |
|---|---:|---:|---:|---:|---:|---:|---:|
| Light | 8 | 42 | 1.51 | 5.33 | 4.0000 | 0 / 33 | 1 |
| Light | 8 | 43 | 1.45 | 4.83 | 3.7500 | 0 / 30 | 0 |
| Light | 8 | 44 | 0.69 | 3.00 | 3.7500 | 0 / 30 | 0 |
| Light | 80 | 42 | 1.76 | 6.83 | 3.6875 | 0 / 295 | 0 |
| Light | 80 | 43 | 1.27 | 5.17 | 3.5750 | 0 / 286 | 0 |
| Light | 80 | 44 | 1.04 | 4.33 | 3.3500 | 0 / 268 | 0 |
| Near capacity | 8 | 42 | 18.55 | 47.67 | 8.5000 | 0 / 76 | 8 |
| Near capacity | 8 | 43 | 3.27 | 8.67 | 6.8750 | 0 / 56 | 1 |
| Near capacity | 8 | 44 | 9.16 | 19.00 | 9.2500 | 0 / 79 | 5 |
| Near capacity | 80 | 42 | 11.45 | 26.83 | 8.7000 | 0 / 696 | 0 |
| Near capacity | 80 | 43 | 11.35 | 30.67 | 8.6125 | 0 / 692 | 3 |
| Near capacity | 80 | 44 | 6.87 | 16.83 | 8.2250 | 0 / 661 | 3 |
| Over capacity | 8 | 42 | 65.65 | 109.50 | 10.0000 | 13 / 114 | 21 |
| Over capacity | 8 | 43 | 52.35 | 86.67 | 10.0000 | 0 / 93 | 13 |
| Over capacity | 8 | 44 | 62.45 | 111.67 | 9.8750 | 19 / 118 | 20 |
| Over capacity | 80 | 42 | 99.29 | 112.00 | 10.5375 | 246 / 1,107 | 18 |
| Over capacity | 80 | 43 | 98.90 | 112.33 | 10.5375 | 248 / 1,111 | 20 |
| Over capacity | 80 | 44 | 97.45 | 112.33 | 10.5250 | 250 / 1,112 | 20 |

Every completed wash adds exactly 5 minutes 40 seconds of service, so its whole visit equals its wait plus 5 minutes 40 seconds. The same constant shifts each mean, median, and percentile. For example, the long seed-42 mean visit is 7.43 minutes under light demand, 17.11 near capacity, and 104.96 under overload. The CSV includes mean, median, P10, P90, and maximum for both waiting and total visit time; these are customer distributions, not averages of C's per-timestamp percentiles.

**Demand effect.** Across the three long runs, mean waits span 1.04-1.76 minutes under light demand, 6.87-11.45 near capacity, and 97.45-99.29 under overload. Long light/near cases reject no cars. Overloaded baselines reject 22.22%, 22.32%, and 22.48% for seeds 42/43/44. Their arrival-window throughput spans 10.5250-10.5375 cars/hour, close to the nominal one-bay capacity of 10.59. The lot stores excess demand until full; it cannot make the bay wash faster. These mechanisms fit the original car-wash process.

**Scale effect.** Long overloaded waits exceed the corresponding short waits for all three seeds. The short seed-43 run even has zero rejections, whereas its longer continuation rejects 248 cars. Starting empty and observing a finite random sample can conceal sustained congestion. Longer observation does not always increase mean waiting: near-capacity seed 42 drops from 18.55 to 11.45 minutes, and seed 44 drops from 9.16 to 6.87. The longer sequence includes additional quieter and busier periods. No warm-up interval was removed, and these are whole-run results rather than a claim that steady state was reached.

The short input is the corresponding long input's prefix. Comparisons across horizons are paired extensions, and the three reported seed values/ranges are exploratory evidence, not confidence intervals. The 80-hour experiment means continuous operation, not ten separate eight-hour days.

**Arrival window and clearing.** Long overloaded seed 42 has 1,107 offered cars: 246 rejected, 843 finished before cutoff, and 18 still present. All 18 eventually finish, giving 861 completions after draining. The last completion is at step 29,408, or 101.33 minutes after arrivals stop at step 28,800. Arrival-window throughput is `843 / 80 = 10.5375` cars/hour; drained throughput uses `861 / (29408 / 360)`, a different interval. If B finishes before the arrival horizon, the remaining idle time still belongs in the arrival-window denominator.

Supporting figures: [light, long](results/analysis/figures/baseline-light-h28800-seed42-system.png), [near capacity, long](results/analysis/figures/baseline-near-h28800-seed42-system.png), [overload, short](results/analysis/figures/baseline-over-h2880-seed42-system.png), and [overload, long](results/analysis/figures/baseline-over-h28800-seed42-system.png). These native C plots use steps and per-timestamp departure counts; their axes are explained in the [figure guide](results/analysis/README.md#4-figure-guide-and-captions).

### 8.3 Waiting-space sensitivity

All six required percentage changes apply to waiting spaces, with service and the exact workload held fixed. The following long overloaded results retain each seed separately. Rejection triplets are ordered 42 / 43 / 44; those workloads contain 1,107 / 1,111 / 1,112 offered cars. Every row has the same arrival-window throughput triplet, **10.5375 / 10.5375 / 10.5250 cars/hour**, and the same before-cutoff completion counts, **843 / 843 / 842**.

| Change | Waiting spaces | Mean wait, seed 42 (min) | Mean wait, seed 43 (min) | Mean wait, seed 44 (min) | Rejected, seeds 42 / 43 / 44 |
|---|---:|---:|---:|---:|---|
| Baseline | 20 | 99.29 | 98.90 | 97.45 | 246 / 248 / 250 |
| +25% | 25 | 125.10 | 123.39 | 123.21 | 241 / 243 / 245 |
| +50% | 30 | 150.54 | 147.20 | 148.56 | 236 / 238 / 240 |
| +100% | 40 | 199.57 | 192.25 | 198.38 | 226 / 228 / 230 |
| -10% | 18 | 88.91 | 88.96 | 86.97 | 248 / 250 / 252 |
| -20% | 16 | 78.47 | 78.43 | 76.47 | 250 / 252 / 254 |
| -30% | 14 | 67.79 | 67.82 | 65.91 | 252 / 254 / 256 |

**Increasing waiting resources.** More space reduces rejections, but it does not improve response time or arrival-window throughput in these overloaded runs. For seed 42, doubling space changes rejection from 22.22% to 20.42%, while mean wait rises from 99.29 to 199.57 minutes and P90 rises from 112.00 to 225.33 minutes. Every seed admits 20 extra cars with 40 spaces; those extra completions occur after cutoff. The extra storage permits a longer backlog behind the same processing bottleneck.

**Decreasing waiting resources.** Smaller lots reject more customers and reduce eventual completions. The -30% setting has 252/254/256 rejections, or 22.76%/22.86%/23.02%, versus 246/248/250 at baseline. Its mean wait spans 65.91-67.82 minutes instead of 97.45-99.29. This is a tradeoff: shorter queues and different admitted populations yield lower completed-customer waits, while more potential customers receive no service. It is not evidence that reducing capacity improves every customer's experience.

**When lot capacity is not limiting.** In every long near-capacity case, all six lot variants have exactly the baseline's customer timings, throughput, rejections, and cutoff counts. Baseline peak waiting is only 11/9/7 cars, below the smallest 14-space lot. Changing an unused limit cannot change these trajectories.

The assignment asks how resource additions improve and reductions harm response time/throughput. Here the experiment shows why that direction is not guaranteed: queue depth controls admission/storage, while bays control processing. We report the unchanged or opposite effects rather than claiming a speed improvement from parking space. The separate bay experiment below tests the processing resource. All 42 paired changes, including absolute and percentage changes in timing/throughput and rejection percentage points, are in [paired-comparisons.csv](results/analysis/tables/paired-comparisons.csv).

Supporting waiting distributions: [20 spaces](results/analysis/figures/baseline-over-h28800-seed42-waiting.png) and [40 spaces](results/analysis/figures/queue40-over-h28800-seed42-waiting.png). Their bins and axes differ; compare values on the time axis and the numerical summaries rather than bar heights.

### 8.4 One bay versus two bays

This comparison doubles bays while holding waiting spaces at 20 and reusing the baseline workload. All rows use the 80-hour arrival window; arrows show one bay to two bays.

| Demand | Seed | Mean wait (min), 1 to 2 bays | P90 wait (min), 1 to 2 bays | Washes/hour before cutoff, 1 to 2 bays | Rejected, 1 to 2 bays |
|---|---:|---|---|---|---|
| Near capacity | 42 | 11.45 -> 0.72 | 26.83 -> 3.00 | 8.7000 -> 8.7000 | 0 -> 0 |
| Near capacity | 43 | 11.35 -> 0.66 | 30.67 -> 3.00 | 8.6125 -> 8.6375 | 0 -> 0 |
| Near capacity | 44 | 6.87 -> 0.49 | 16.83 -> 2.00 | 8.2250 -> 8.2500 | 0 -> 0 |
| Over capacity | 42 | 99.29 -> 2.18 | 112.00 -> 6.33 | 10.5375 -> 13.8375 | 246 -> 0 |
| Over capacity | 43 | 98.90 -> 2.09 | 112.33 -> 6.17 | 10.5375 -> 13.8875 | 248 -> 0 |
| Over capacity | 44 | 97.45 -> 2.38 | 112.33 -> 7.67 | 10.5250 -> 13.8500 | 250 -> 0 |

Two bays reduce waiting in all six tested comparisons. Under overload, mean wait spans 2.09-2.38 minutes, mean total visit spans 7.75-8.05 minutes, and no cars are rejected. Arrival-window throughput rises to 13.8375-13.8875 cars/hour. This is below the two-bay nominal capacity of 21.18 cars/hour because offered demand, its timing, and cutoff now limit observed completions. Seed 44 still has four unfinished cars at cutoff; zero rejection does not imply zero waiting or an empty system at closing.

Near capacity, every offered car was already admitted with one bay, so eventual completion counts remain unchanged. Two bays primarily improve timing. Seeds 43/44 finish two additional cars before cutoff, leaving one unfinished instead of three; this explains the small 0.0250 cars/hour increase. Seed 42 already finished all arrivals before cutoff, so its throughput is unchanged despite much shorter waiting. This illustrates why a timing benefit need not produce a large throughput increase.

The [two-bay waiting distribution](results/analysis/figures/bays2-over-h28800-seed42-waiting.png) shows the overload timing improvement. The experiment supports considering service capacity for this synthetic demand, but does not establish that another bay is financially justified.

### 8.5 Interpreting USE, throughput, and latency

The 10/100/1000-step windows correspond to 100 seconds, 16 minutes 40 seconds, and 2 hours 46 minutes 40 seconds. They are trailing windows with shorter prefixes at startup. Short windows expose brief bursts; long windows retain recent history and smooth the displayed congestion. They are overlapping observations, not independent samples.

- **Utilization:** the native [near-capacity](results/analysis/figures/near-use/utilization.png) and [overload](results/analysis/figures/over-use/utilization.png) plots show C's rounded activity indicator. A value of 1 means at least half of the effective window had at least one busy bay, not 100% capacity usage. The long overloaded plateau is consistent with sustained activity, but cannot quantify exact utilization or distinguish one from two occupied bays. Offered loads of 34%/85%/136% are theoretical demand-to-capacity ratios, not measurements from these plots.
- **Saturation:** compare [near capacity](results/analysis/figures/near-use/saturation.png) with [overload](results/analysis/figures/over-use/saturation.png). The former shows temporary queues; the latter shows sustained pressure close to the 20-space limit after startup, followed by clearing. Values are rounded average waiting cars, excluding service. The [arrival-versus-wait scatter](results/analysis/figures/baseline-over-h28800-seed42-arrival-wait.png) provides customer-level evidence of congestion developing after the empty start.
- **Errors:** the [overload error plot](results/analysis/figures/over-use/error-rate.png) shows rejection counts within each window after differencing B's cumulative counter. Larger-window totals are not directly comparable as rates. A positive value after arrivals stop can refer to an earlier rejection still inside the window. Do not add overlapping window counts to estimate total rejected cars; use distinct rejected outcomes. The [near-capacity plot](results/analysis/figures/near-use/error-rate.png) remains zero for its illustrated run.
- **Throughput and latency:** C's system bars count completions at departure timestamps. Their total divided by the stated elapsed interval gives throughput. Its system response time is the full visit; its job response time is waiting until first service. Per-timestamp mean/median/P10/P90 coincide in the single-bay figures because there is one departure per timestamp. The complete-customer distribution remains varied, and no departure means undefined latency for that timestamp.

The checked outputs support the conclusions about waiting, admission, and throughput. They do not supply a precise bay-capacity utilization measurement. That part of the assignment's intended USE interpretation remains limited by the supplied C implementation, as documented in the [measurement contract](results/analysis/README.md#2-input-compatibility-and-measured-semantics).

### 8.6 Occupancy and total-time cross-check

All 60 runs passed a finite-run conservation check: the sum of waiting occupancy equals summed customer waiting times, and the sum of busy-bay occupancy equals 34 times the number of completed cars. Their sum equals all admitted customers' total visit times. Units are car-steps on both sides. These checks use the original `qmet.csv` and C-derived timings verified against events/sink rows; they do not use C's rounded activity indicator.

For long overloaded baseline seed 42, the run starts empty and fully drains at `T = 29408`. It completes `N = 861` admitted cars. The integrated waiting-plus-service occupancy is `A = 542222` car-steps:

```txt
Mean occupancy L     = A / T = 542222 / 29408 = 18.437908 cars
Accepted rate        = N / T = 861 / 29408    = 0.02927775 cars/step
Mean complete visit W = A / N = 542222 / 861  = 629.758420 steps
Accepted rate * W    = 18.437908 cars = L
```

The mean visit is 104.959737 minutes, matching the saved summary. The occupancy samples represent intervals `[t,t+1)` through `T-1`; the final empty row contributes zero occupancy and no extra elapsed interval. Immediate rejections contribute no accepted occupancy or visit time. The exact relationship comes from accounting for every accepted visit within matching empty-to-empty boundaries; it is not an assumption that an overloaded finite run is in steady state. Combining occupancy only before H with full visit times extending past H would require accounting for the remaining unfinished work. No such mixed-boundary equality is claimed.

### 8.7 Practical recommendations and limits

1. **Choose the resource that addresses the observed constraint.** In the overloaded one-bay model, service capacity limits completed washes. Expanding from 20 to 40 waiting spaces retains 20 more cars per tested long workload but roughly doubles the mean wait, with no arrival-window throughput gain. More parking should be justified as additional admission/storage, not faster washing.
2. **Assess waiting and lost customers together.** A 14-space lot produces shorter completed-customer waits under overload while rejecting more arrivals. Decide acceptable waiting and rejection targets before choosing capacity. This study supplies the tradeoff; it does not define those business targets.
3. **Consider another bay when service demand warrants it.** Two bays lower overloaded mean wait to about 2.1-2.4 minutes and reject no cars in these three inputs. Near capacity they primarily reduce waiting rather than increase total customers served. Cost, staffing, equipment, water, and site constraints are needed before recommending a purchase.
4. **Plan the end of the operating window.** Stopping arrivals does not immediately clear the lot. State whether customers may finish after closing, whether overtime is acceptable, and how rejected/unfinished customers are handled. The simulated 80-hour continuous run does not model daily shutdowns.
5. **Validate with operational data before sizing a real facility.** Collect arrival patterns, package/service-time variation, abandonment and retry behavior, downtime, and financial constraints. Revisit the fixed-service and independent-arrival assumptions and test more seeds and operating schedules.

The model uses synthetic demand and wash durations, integer ten-second steps, an empty initial state, fixed service, no abandonment/retries, and no overnight reset. It excludes payment, bay movement, breakdowns, and shared resource limits. Only three seeds were tested; reported ranges are not uncertainty bounds for a real business. Resource variants can admit different customers, so their timing comparisons are conditional on completion. C further limits precise utilization interpretation and labels window error counts as a rate. These limits prevent claims of an optimal lot/bay design, guaranteed no rejection, measured industry performance, or financial return.

### 8.8 Findings for the group report or presentation

The single FIFO car-wash model was validated with small timing/rejection examples and studied in 60 paired experiments. Long overloaded one-bay runs had mean waits of 97.45-99.29 minutes and rejected about 22.2-22.5% of offered cars. Doubling waiting space admitted more cars but increased mean wait to 192.25-199.57 minutes without increasing arrival-window throughput. Doubling bays instead reduced mean wait to 2.09-2.38 minutes and eliminated rejection in the tested inputs. Near-capacity lot-size changes had no effect because the lot never filled. These findings distinguish waiting storage from service capacity and must be presented with the synthetic assumptions and C's measurement limitations.

Use the [all-seed baseline table](#82-demand-and-scale-results), [lot comparison](#83-waiting-space-sensitivity), [bay comparison](#84-one-bay-versus-two-bays), and [figure captions](results/analysis/README.md#4-figure-guide-and-captions) as the supporting evidence. This is the D contribution; the shared report and presentation are assembled separately.

## 9. Reproduction and submission

The Objective D package lives in `analysis/simple/`. Its configurations, workloads, scripts, and evidence support the single-queue modeling and analysis requirements.

Every completed run preserves its configuration, workload reference/hash, seed, commands, simulator version, stdout/stderr, completed workload, queue measurements, and events. Derived tables and figures remain separate from raw output.

The manifest specifies the simulation work, and its execution evidence is now saved. Experimental run metadata, diagnostics, and verification records are consolidated in `runs/experiments/execution.json`; raw files stay in their respective run directories. C analysis reused those outputs and has its own shared report under `results/analysis/`. The verified findings can support the group report and presentation.

### 9.1 Validation environment and checks

Validated on Windows with Python 3.14.3 and MSYS2 UCRT64 GCC 14.2.0. The compiler must be available as `g++` on PATH, or selected through `CXX`. The compiler's runtime libraries must also be on PATH. Git is needed to record the source revision.

The [validation script](scripts/validate_carwash.py) compiles the existing A generator, A visualizer, and B simulator plus their existing C++ tests using `-std=c++20 -Wall -Wextra -Wpedantic -O2`. Executables and test temporary files live in the ignored D-only `.build/` directory. No shared component sources, tests, Makefiles, or build directories are changed.

| Existing test program | Result |
|---|---|
| A workload generator | 37 checks passed |
| A workload visualizer | 28 checks passed |
| B simple queue simulator | 106 checks passed |
| Each of the three command-line programs | Help succeeds; missing required arguments fail |

These are native functional tests, not the full team shell-runner result. WSL access was denied in this environment and Valgrind was unavailable. No Valgrind leak check, ASan/UBSan run, or coverage result is claimed. The script invokes the existing test sources directly so their runners do not write into the team's directories.

### 9.2 Reproduce without overwriting saved evidence

From the repository root, with Python 3.10 or newer, Git, and a compatible C++20 GCC compiler available:

```txt
python analysis/simple/scripts/validate_carwash.py --output-root analysis/simple/.build/replay-01
```

This builds the tools, runs their existing C++ tests and basic CLI checks, then runs and verifies both car-wash cases. Choose another new output directory for another replay. The script refuses any output directory that already exists and requires evidence to stay within `analysis/simple/`. The saved `runs/` directory is intentionally preserved; running the script with its default output while `runs/` exists fails before any build or simulation.

The template uses placeholder source/sink fields. The script fills them with paths appropriate to the run location; inputs are not resolved from an accidental working directory. It runs B without a time limit using each run directory as the working directory:

```txt
<built-simple-queue-sim> config.csv --metrics qmet.csv --events job.log
```

The exact recorded executable paths, arguments, working directories, input hashes, and exit statuses appear in the metadata. Build/test evidence records the compiler version and hashes of the unchanged A/B C++ source/header files. Workload fixtures are hand-constructed, so their seed is recorded as null.

The saved validation artifacts provide reproducible examples of correct model behavior. Compiled binaries and temporary test/replay files are excluded by the local `.gitignore`. Raw summaries and the [C-derived analysis package](results/analysis/README.md) are available separately.

### 9.3 Reproduce the prepared inputs

Run commands from the Git repository root: the folder containing `analysis/` and `implementation/`, not the outer course `Lab0` folder. The preparation script uses Python's standard library, Git, and the same GCC requirements as the validation script. It reuses a few common path/CSV helpers from `validate_carwash.py`; it does not run its validation cases.

The saved workloads and manifest already exist in this checkout. To regenerate them for comparison without overwriting them:

```txt
python analysis/simple/scripts/prepare_workloads.py --output-root analysis/simple/.build/workloads-replay-01
```

The default seeds are 42, 43, and 44. This produces 18 workloads, 60 manifest rows, four input figures, and provenance/summary files in the new output root. For a smaller six-workload/20-row pilot:

```txt
python analysis/simple/scripts/prepare_workloads.py --output-root analysis/simple/.build/workloads-pilot-01 --seeds 42
```

Choose another new output directory for another replay. Without `--output-root`, the script prepares the standard locations under `analysis/simple/`; it refuses to overwrite any existing generated directory, manifest, preparation report, workload summary, or workload-figure directory. It does not overwrite the deterministic validation runs.

The preparation process builds the unchanged A generator/visualizer, checks repeated generation, transforms and validates all workloads, writes the comparison manifest, and uses A to generate the figures. Its final message explicitly says that no experimental simulations were run. Successful preparation makes the inputs ready for B; it is not evidence that the 60 experiments have completed.

The standard output layout is:

```txt
experiments.csv
workloads/preparation.json
workloads/generated/<workload-id>/generator-output.csv
workloads/generated/<workload-id>/workload.csv
workloads/generated/<workload-id>/metadata.json
results/tables/workloads.csv
results/figures/workloads/*.png
results/figures/workloads/metadata.json
```

Keep original and transformed CSVs, the manifest, summaries, and figures together for reproducibility. Binaries, repeated scratch generation, and local replay directories stay under ignored `.build/` and are not submission artifacts.

The local `.gitattributes` preserves the exact bytes of generated CSVs, the manifest, and the workload summary. This prevents Git's platform-dependent line-ending conversion from invalidating their recorded hashes and retains A's unmodified output.

### 9.4 Reproduce the experiment runs

[run_experiments.py](scripts/run_experiments.py) reads the manifest, verifies all workload hashes and paired settings, builds the unchanged B sources in `.build/experiments/`, writes a configuration per run, executes B without a time limit, and verifies its raw output. It imports common CSV/path helpers from the other D scripts without executing their preparation or validation workflows.

From the repository root, rerun all 60 experiments into a new ignored directory:

```txt
python analysis/simple/scripts/run_experiments.py --output-root analysis/simple/.build/experiments-replay-01
```

To check one case first:

```txt
python analysis/simple/scripts/run_experiments.py --output-root analysis/simple/.build/experiments-sample-01 --run-ids baseline-over-h28800-seed42
```

`--run-ids` accepts one or more IDs present in the manifest. `--manifest` optionally selects a different manifest within D; workload paths are resolved from that manifest's directory. Without `--output-root`, the standard destinations are `runs/experiments/` and `results/tables/run-summary.csv` under D. Those destinations already contain saved evidence, so the script refuses to overwrite them. Use a new output root for a rerun.

The script verifies the entire manifest before launching the requested subset. It writes four files per run and saves each run's metadata, captured terminal output, and verification in the shared execution report. If a run or verification fails, it records the failure, preserves any outputs and earlier successful summaries, and stops. A final `passed` execution report means every selected run completed and shared B source hashes stayed unchanged. The report's selected IDs determine whether it describes the full matrix or only a subset.

To run the accounting-verifier tests without building or rerunning simulations:

```txt
python -m unittest discover -s analysis/simple/tests -v
```

Tests use the saved deterministic examples and create temporary copies under ignored `.build/`. The local `.gitattributes` also preserves raw experimental output and run-summary bytes so their hashes remain valid after checkout. No edits to A, B, or C are required to execute this workflow.

### 9.5 Reproduce the C-based analysis

With Python 3.10+ including matplotlib and Pillow, Git, and a C++20 GCC compiler, run from the repository root:

```txt
python analysis/simple/scripts/analyze_results.py --output-root analysis/simple/.build/analysis-replay-01
```

The [analysis reproduction guide](results/analysis/README.md) explains the pinned C revision, smaller paired subsets, interpreter used here, generated artifacts, and no-overwrite behavior. C is exported from Git into ignored D scratch space and built without editing team files. Results are derived from existing B runs, with one additional small two-bay validation simulation. Personal drafts, copied sources, binaries, adapted intermediate inputs, and replay outputs are excluded from the submission.

A fresh-output replay of long overloaded seed 42 with the baseline, 40 spaces, and two bays passed all nine tool demonstrations and the three small validation cases. All five consolidated tables matched the corresponding saved rows; twelve native example files and ten reproduced figures matched byte-for-byte. This verifies the documented subset workflow without replacing the full 60-run evidence. Final documentation checks also reconciled all 60 occupancy/time identities, checked the 31 new comparison-table rows against the saved CSVs, and confirmed original evidence hashes.

## References

- Course handout **Queue Lab.pdf**, C pages 16–25 and D page 26 (distributed separately from the repository).
- Repository requirements: [analysis checklist](../TODO.md) and [repository README](../../README.md).
- [A generator guide](../../implementation/obj-a/workload-generator/README.md), [A visualizer guide](../../implementation/obj-a/workload-visualizer/README.md), and [B simulator guide](../../implementation/obj-b/simple-queue-sim/README.md).
- [Mor Harchol-Balter: A Little Bit of Queueing Theory](https://www.cs.cmu.edu/~harchol/Probability/chapters/chpt27.pdf).
- [Brendan Gregg: The USE Method](https://www.brendangregg.com/usemethod.html).
