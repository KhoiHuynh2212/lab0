# Car-wash analysis with unchanged Objective C

All 60 saved experiments were analyzed using C commit `c65b1516fc98a5fc30115c4827a851fecadbc0d4`. C source was exported from Git and built in ignored D scratch space without modification. The [execution report](execution.json) records commands, source/input/output hashes, adapters, validation cases, tool coverage, and captions for all figures. Its status is `passed_with_documented_measurement_limitations`: consistency checks passed, but C's utilization and error-plot labels have the limitations explained below.

See the [model and experiment design](../../README.md) for assumptions, workload generation, resource settings, and the original simulation evidence. The main document now contains the [page-26 answers](../../README.md#81-assignment-answer-map), [all-seed demand and scale results](../../README.md#82-demand-and-scale-results), and [practical recommendations](../../README.md#87-practical-recommendations-and-limits). This guide explains the supporting files and tool conventions. All times are simulation steps unless a column explicitly says minutes or hours. One step is 10 seconds; six steps are one minute.

## 1. Start with the comparison tables

| File | Contents | Interpretation |
|---|---|---|
| [Performance summary](tables/performance-summary.csv) | 60 rows, one per run | Counts, throughput, and mean/median/P10/P90/maximum waiting and total visit times |
| [Paired resource comparisons](tables/paired-comparisons.csv) | 42 rows, one per nonbaseline resource variant | Variant minus matching baseline; relative changes use the same demand/horizon/seed/input |
| [Per-car measurements](tables/job-metrics.csv) | 44,510 rows | C's native fields plus `run_id` and verified `outcome`; includes 39,339 completed and 5,171 rejected car instances |
| [System measurements](tables/system-metrics.csv) | 39,284 rows | C's native departure-time statistics plus `run_id`; tied departures share one row |
| [USE samples](tables/use-samples.csv) | 300 rows | Exact C values at steps 0, 9, 99, 999, and H-1 for each experiment; not a complete time series |

These totals repeat workloads under different resource settings; they do not represent distinct customers in one system. The complete USE streams stay in ignored scratch space and are reproducible using the script. All 1,514,440 experimental USE rows were checked at all three window sizes before selecting the 300 saved samples.

Completed-customer timing statistics follow all admitted cars through draining, including those unfinished at the arrival cutoff. Rejected customers are excluded from these timing distributions, but remain in the per-car table with `outcome=rejected`. C gives immediate rejections blank response time and zero service/queue/turnaround; those zeros are not successful washes.

Run summaries use ordinary arithmetic means, the midpoint median for even sample counts, and nearest-rank P10/P90 over completed customers. Native C system means and medians are instead rounded to whole steps within each departure timestamp. Do not average per-timestamp means to obtain a whole-run mean without accounting for the number of departures.

Arrival-window throughput is `completed_before_cutoff * 360 / H` cars/hour for the interval [0,H). Drained throughput is `completed * 360 / last_departure` for elapsed time from zero through final completion. The final empty measurement row does not add another elapsed interval. These two rates have different denominators and answer different questions.

Paired changes are variant minus baseline. A negative waiting-time change means less waiting; a negative rejection-percentage-point change means fewer rejected arrivals. Percentage change is blank when the baseline value is zero. Runs remain separate by seed; the three seeds support an exploratory comparison, not strong statistical certainty. Accepted customer populations can differ between capacity variants, so conditional timing statistics must be interpreted alongside rejection.

## 2. Input compatibility and measured semantics

The original B configurations, workloads, sink files, measurements, and event logs are unchanged. D prepares separate copies:

1. **Queue input:** select `q0`, verify the filter preserved every row, then difference cumulative errors into new errors per timestep. Reject a decreasing counter. If B drained before H, append known empty/idle rows through H-1, with zero new errors; the original measurement rows remain represented. A total of 478 idle rows were appended across this matrix.
2. **System-log input:** add a header and replace only the known source/sink labels with recorded `source.wl`/`sink.wl` aliases. C consumes a header and recognizes `.wl` boundaries. These aliases do not create new simulated workloads. Job IDs, times, queues, actions, and all other event details are preserved.
3. **Job-filter input:** add a header while retaining original source/sink labels. C copies the first line as a header; without this adaptation it can leak the first car's arrival into a selection that excludes that car.
4. **Timing plots:** select completed customers before plotting waiting/turnaround distributions. The full consolidated table retains rejected customers and their outcome labels.

| C output | What this implementation actually measures | What must not be claimed |
|---|---|---|
| `Utilization(W)` | Rounded mean of `busy servers > 0`, with values 0 or 1; 1 means at least half the effective window had some service activity | It is not a utilization percentage and does not normalize by one versus two available bays |
| `Saturation(W)` | Rolling average waiting cars, rounded to the nearest whole car | Fractional queue averages are not retained |
| `Errors(W)` after adaptation | Number of new errors in the effective window | It is not errors per unit time; the unchanged plot title "Error Rate" does not perform a rate conversion |
| Job `response-time` | Arrival to first service entry | It is not the whole visit |
| Job `turn-around-time` / system response time | Arrival to completion for completed cars | Rejected cars' zero durations must not be included as completed-customer performance |

Before a window is full, C averages/sums the available prefix. Error windows are overlapping; do not sum their totals to obtain total rejections. All input series here are a single queue with contiguous timestamps beginning at zero. If a future C revision changes input conventions, revalidate the adapters rather than applying error differencing twice.

The assignment's text defines percentage utilization, but the displayed example table does not establish a consistent calculation from its displayed input. This study reports the supplied implementation's actual behavior. It does not resolve that ambiguity or claim an accurate bay-capacity utilization measurement. Throughput, waiting, queue occupancy, and rejection comparisons remain available under the verified definitions above.

## 3. Measured comparisons

The following illustration uses long-horizon seed 42. Waiting and total times are conditional on completion after full drain; throughput uses the common 80-hour arrival window. Other seeds and every resource variant are retained in the tables.

| Demand / resources | Mean wait (min) | P90 wait (min) | Mean visit (min) | Washes/hour before cutoff | Rejected |
|---|---:|---:|---:|---:|---:|
| Light, 20 spaces / 1 bay | 1.76 | 6.83 | 7.43 | 3.6875 | 0 |
| Near capacity, 20 / 1 | 11.45 | 26.83 | 17.11 | 8.7000 | 0 |
| Over capacity, 20 / 1 | 99.29 | 112.00 | 104.96 | 10.5375 | 246 |
| Over capacity, 40 / 1 | 199.57 | 225.33 | 205.24 | 10.5375 | 226 |
| Over capacity, 14 / 1 | 67.79 | 78.17 | 73.46 | 10.5375 | 252 |
| Over capacity, 20 / 2 | 2.18 | 6.33 | 7.84 | 13.8375 | 0 |

For this overloaded input, doubling waiting space reduced rejections by 20 but roughly doubled mean waiting among completed customers without increasing arrival-window throughput. Reducing space lowered the conditional wait while rejecting more customers. Two bays increased arrival-window throughput and reduced waiting with no rejections in this case. These are tradeoffs under the synthetic model, not a cost-based recommendation.

Across all three long overloaded seeds, baseline mean waits were 97.45-99.29 minutes, 40-space waits were 192.25-199.57 minutes, and two-bay waits were 2.09-2.38 minutes. For long near-capacity demand, every waiting-space variant had identical customer timings to its matching baseline: the lot limit was never reached. The two-bay variants still reduced waiting there. The main document explains [short versus long behavior](../../README.md#82-demand-and-scale-results), [every prescribed lot-size change](../../README.md#83-waiting-space-sensitivity), and [one versus two bays](../../README.md#84-one-bay-versus-two-bays), with each seed reported separately. Its recommendations follow these measured tradeoffs and model limits.

## 4. Figure guide and captions

All sixteen images were produced by unchanged C plotting tools. Exact commands and per-image captions are recorded in the execution report. Seed 42 illustrates the plots; numeric comparisons include all three seeds.

### USE plots

| Scenario | Activity indicator | Rounded waiting average | Window error counts |
|---|---|---|---|
| Near capacity, long baseline | [0/1 activity](figures/near-use/utilization.png) | [Waiting](figures/near-use/saturation.png) | [Errors](figures/near-use/error-rate.png) |
| Over capacity, long baseline | [0/1 activity](figures/over-use/utilization.png) | [Waiting](figures/over-use/saturation.png) | [Errors](figures/over-use/error-rate.png) |

**Read these labels with section 2:** "Utilization" is the rounded 0/1 activity indicator, not percent capacity usage. "Error Rate" shows counts per window. Each curve uses 10, 100, or 1000 steps; the plots include clearing after arrivals stop and any explicitly appended idle tail. A remaining error count during draining can refer to a rejection still inside a trailing window, not a new rejection during draining.

### System timing and throughput

- [Light, long](figures/baseline-light-h28800-seed42-system.png)
- [Near capacity, long](figures/baseline-near-h28800-seed42-system.png)
- [Over capacity, long](figures/baseline-over-h28800-seed42-system.png)
- [Over capacity, short](figures/baseline-over-h2880-seed42-system.png)

The upper panels measure whole visits in steps, at completion timestamps. In these single-bay cases only one car completes at a timestamp, so the per-timestamp mean, median, P10 and P90 coincide. That does not mean whole-run customer times are identical. Connecting lines are visual guides between observed departure times; they do not define latency at timestamps without departures. Lower bars count departures at each timestamp, not cars/hour. Very narrow bars are hard to resolve over the full 80-hour axis; use the numeric tables for counts/rates. Axes are chosen by the native tool, so compare axis values rather than apparent line heights across figures.

### Completed-customer timing distributions

- [Overload, 20 waiting spaces](figures/baseline-over-h28800-seed42-waiting.png)
- [Overload, 40 waiting spaces](figures/queue40-over-h28800-seed42-waiting.png)
- [Overload, two bays](figures/bays2-over-h28800-seed42-waiting.png)
- [Overload, arrival versus wait](figures/baseline-over-h28800-seed42-arrival-wait.png)

Here `response-time` means waiting before first service, in steps. Multiply by 1/6 for minutes. All completed customers are followed through draining; rejected customers are excluded. Histograms use 20 bins chosen by C separately for each dataset, so their bin widths/axes differ. Compare the summary statistics and x-axis units rather than raw bar heights across plots. The scatter shows when admitted customers arrived and how long they waited.

### Selected event traces

- [Immediate service versus waiting: smoke j0/j5](figures/smoke-trace.png)
- [Acceptance versus rejection: overflow j0/j21](figures/overflow-trace.png)

Squares show time waiting, circles show service, and x shows rejection. Job IDs are on the vertical axis and steps on the horizontal axis. Markers can overlap at this scale; the underlying filtered logs and checked timings give exact boundaries. These are selected examples, not all cars in a long experiment.

## 5. Nine-tool coverage and validation

| C tool | Demonstration / retained evidence |
|---|---|
| filter-by-queue | q0 selected for each run; identical rows checked against original qmet.csv, with commands and hashes in the report |
| filter-by-job | [Smoke selection](examples/smoke/selected.log), [overflow selection](examples/overflow/selected.log), and [two-bay selection excluding j0](examples/two-bay/selected.log) |
| compute-use | [Smoke native output](examples/smoke/quse.csv), [overflow](examples/overflow/quse.csv), [two-bay](examples/two-bay/quse.csv); full matrix windows checked and sampled |
| plot-use | Six USE images above, with the implementation's limitations explicitly labeled |
| derive-event-metrics | [Smoke native output](examples/smoke/emetric.csv), [overflow](examples/overflow/emetric.csv), [two-bay](examples/two-bay/emetric.csv); full matrix consolidated by run |
| plot-derive-event-metrics | Three waiting histograms and one arrival/wait scatter |
| derive-sys-metrics | [Smoke native output](examples/smoke/sysmetric.csv), [overflow](examples/overflow/sysmetric.csv), [two-bay](examples/two-bay/sysmetric.csv); full matrix consolidated by run |
| plot-sys-metrics | Four demand/horizon figures |
| create-event-trace | Two selected event traces |

The smoke case verifies waits 0/30/59/92/121/148 and 34-step service. Overflow verifies 21 completed visits plus one immediate rejection. The additional two-bay validation has arrivals 0/0/1/2 and departures 34/34/68/68: two simultaneous completions at each departure time. The final bucket has visit times 66 and 67; C rounds its mean and median to 67, with P10=66 and P90=67. A cutoff at 68 includes only the first two completions, demonstrating the half-open observation interval. Only this additional validation scenario was newly simulated; none of the 60 saved experiments was rerun.

All five existing C++ unit-test programs passed under a native build. The 25 D Python tests passed, including changed counters, preserving adapter inputs, a missing system departure, wrong timing, completed/rejected populations, cutoff boundaries, zero-baseline percentage changes, and no-overwrite protection. All 16 final figures matched the inspected pilot figures byte-for-byte. Each run's C timings were checked against raw events and sink rows, and summed waiting/service time was reconciled with queue/server occupancy. Every full USE row was checked against C's actual formula, not against a claimed percentage it does not compute.

## 6. Reproduce

Requirements: Python 3.10+ with `matplotlib` and `Pillow`, Git with the recorded C commit available, and a C++20 GCC compiler on PATH (or `CXX`). The saved execution used bundled Python 3.12.14 on Windows. Source export, binaries, test temporary files, adapted inputs and full per-run C streams stay in ignored `.build/` directories. The script builds directly without invoking shared Makefile clean targets.

From the repository root:

```text
python analysis/simple/scripts/analyze_results.py --output-root analysis/simple/.build/analysis-replay-01
```

For a smaller paired comparison:

```text
python analysis/simple/scripts/analyze_results.py --output-root analysis/simple/.build/analysis-pilot-new --run-ids baseline-over-h28800-seed42 queue40-over-h28800-seed42 bays2-over-h28800-seed42
```

The default destination `analysis/simple/results/analysis/` already exists, so omit it only for a first execution in a checkout without saved analysis. Existing destinations are refused. A selected resource variant must include its baseline. `--c-ref` can select another revision, but changed C semantics cause the checks to stop for review instead of silently changing the study's interpretation. A subset still validates all three small examples and demonstrates all nine tools; its report lists the exact selected experiments.

Use an interpreter that has both plotting dependencies installed. In PowerShell, an explicit interpreter path uses the form `& 'path/to/python.exe'` followed by the same script and arguments. The recorded execution report identifies the interpreter used for the saved evidence; another teammate does not need that machine's absolute path.

Run the D tests independently with:

```text
python -m unittest discover -s analysis/simple/tests -v
```

Generated results comprise one execution report, five consolidated tables, twelve native example files, and sixteen figures. This README supplies their interpretation. The reproduction script regenerates the data/report/figures, while this explanatory document is maintained with the study. A failed run preserves its report and any partial output; use a new destination after investigating the recorded failure.
