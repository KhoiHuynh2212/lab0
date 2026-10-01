# Objective F: exit-directed road-network study

This folder contains F's shared model, validation workflow and prepared inputs from [SNAP roadNet-TX](https://snap.stanford.edu/data/roadNet-TX.html). Small hand-calculated networks establish compatibility with unchanged E and C. Texas inputs and feasibility pilots are prepared; the SMALL, MEDIUM, and LARGE scale studies are complete in `analysis/multi-SMALL/`, `analysis/multi-MEDIUM/`, and `analysis/multi-LARGE/`.

## Model

A vehicle is a job, an intersection is a bounded FIFO queue, and servers represent abstract simultaneous intersection-processing capacity. Edges represent allowed transfers; their traversal has no additional travel time in E. Service duration is measured in simulator steps. No real lane count, signal timing, turning behavior, travel time, or demand is inferred from road connectivity.

For a sampled undirected road graph, choose exits from nodes with original connections outside the sample. Record the original sample, boundary evidence, exit choices and permitted directions. A road endpoint or degree-one node is not automatically a traffic exit. The small fixtures here are synthetic validation graphs and make no geographic claims.

Compute each node's shortest distance `d` to a selected exit. Retain only original connections directed from distance `d` to `d-1`. Exit nodes have a sink and no outgoing queue edges. This is a synthetic scenario of flow toward exits, not a reconstruction of Texas one-way roads. E chooses among permitted outgoing edges round-robin in their recorded order.

A vehicle entering at distance `d` has one task `W (d+1)`. Each queue uses **`Sub-1` without `*`**. Each serviced visit removes one work unit, so the final unit is processed at the exit. For example:

```txt
q0, distance 2       q1, distance 1       q2, distance 0
arrive with W 3  ->  arrive with W 2  ->  arrive with W 1  ->  completed.wl
```

With operation duration 1 and no waiting, arrival at 0 produces service completions at 1, 2, and 3. Transfer occurs in the same timestep. All permitted branches reduce distance by one, so alternative routes from the same entry have the same number of serviced visits. Jobs remain nonempty during every internal transfer. A queue visit is unique per `(job, queue)` under this strictly decreasing-distance model; this validator does not support cyclic/revisiting models.

All job IDs start with lowercase `j`, matching A's usual output and C's job-filter normalization. Different sources must have globally unique IDs. Source/sink files use `.wl` so C recognizes external boundaries. Waiting capacity excludes vehicles in service. A `QUEUE-FULL` event is a modeled loss, not a physical traffic-spillback simulation.

## Files and evidence

| File                                             | Purpose                                                                                                                    |
| ------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------- |
| [scripts/support.py](scripts/support.py)         | Orient/validate graphs, write inputs, reconcile events and occupancy, check C measurements, and package/verify archives    |
| [scripts/run.py](scripts/run.py)                 | Build pinned unchanged tools in scratch, run fixtures, record provenance, and replay saved raw evidence                    |
| [scripts/prepare.py](scripts/prepare.py)         | Normalize/sample Texas roads, invoke A and E, generate paired workloads/resource manifests, and measure feasibility pilots |
| [scripts/analyze.py](scripts/analyze.py)         | Execute scale experiments, validate all runs, demonstrate all 9 C tools, generate consolidated summaries and HTML reports  |
| [source.json](source.json)                       | Dataset provenance, frozen preparation rules, workload counts/hashes, pilot measurements and execution record              |
| [tests/test_pipeline.py](tests/test_pipeline.py) | Reject invalid models, corrupted events/counts/configurations, misleading C outputs and unsafe/corrupt archives            |
| [validation.zip](validation.zip)                 | One archive containing native evidence, model inputs, analysis copies, checks, commands and member hashes                  |
| `.gitignore` / `.gitattributes`                  | Keep builds/extractions/pilots ignored and preserve archive bytes                                                          |

The archive contains `execution.json`, a `checksums.json` member index, eight small model cases and one separate diagnostic. Each case includes its model, source/configuration, raw `job.log`/`qmet.csv`/sink, validation result, adapter record, and native C outputs. These are archive members, not separate tracked files. Copied sources, binaries, temporary test data and failed pilots stay in `.build/` and are not archived.

The execution record identifies the resolved source commit, source hashes, F script/test hashes, compiler, Python interpreter, commands, exit status and captured output. Default source reference: `be3dc3a`, which contains the currently checked E/B/C implementations. The two source-hash maps distinguish exported Git bytes from unchanged working-tree files; platform line endings can differ. No shared sources or Makefiles are edited or cleaned.

## Validation cases

All listed times are steps. Fixtures use small capacities/service rates to expose behavior, not final traffic-study parameters. The fixture definitions contain independent expected completion/drop times.

| Case                        | Expected behavior                                                                                                              |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| Single queue                | Three jobs complete at 2/4/6; E and B produce identical raw files                                                              |
| Three-node path             | Two jobs complete at 3/4 after three serviced visits each; C must report six visits                                            |
| Reversed configuration rows | Reordering the same path's queue rows preserves its checked outcomes                                                           |
| Branching                   | Four jobs alternate q1/q2 and complete at 3/3/4/4; validates tied sink statistics                                              |
| Downstream loss             | One completion at 4, two downstream drops at 1/2; earlier work on dropped jobs remains in accounting                           |
| Multiple boundaries         | Two disjoint source job sets reach two exits at 2; E merges their shared sink without losing jobs                              |
| Time limit                  | At boundary 2, two jobs are unfinished and one scheduled at 8 is unarrived; no final completion yet                            |
| Window history              | One drop at 0 and an idle observation through 1001; errors expire from the 10/100/1000-step windows at the expected boundaries |

The event ledger checks actual configurations/workloads against the model, every source arrival, active visit, task work/timing, server entry/exit, transfer, loss and sink completion. At every metric timestamp it reconstructs waiting, busy servers and cumulative errors and compares them with E's original snapshots. A fabricated arrival cannot conceal a lost transfer: the raw-state checks fail.

Occupancy is also reconciled with time spent by **all observed vehicles, including vehicles later dropped**. In the loss fixture the completed vehicle contributes four car-steps, and the two dropped vehicles contribute one and two, for seven total. Checking only successful vehicles would incorrectly omit three car-steps.

Cutoffs use `[0,H)`: a departure exactly at H is outside the arrival window. The path fixture has completions at 3 and 4 and zero before H=3. Unarrived jobs are kept separate from vehicles already waiting/in service. Truncated service residence is counted to the observation boundary in the occupancy check.

## Confirmed E/C integration findings

### Missing downstream arrival records

The checked E version records external `ARRIVE-VIA` events, but not arrivals delivered from another queue. Unadapted C therefore sees only two queue visits in the two-job path, although there are six serviced visits.

F creates an **analysis copy** with a header and a same-timestep downstream `ARRIVE-VIA` immediately after each valid inter-queue departure. The inserted ordering expresses the transfer time; it is not a claim to reproduce E's entire admission-phase ordering. Per-job timing and every raw queue snapshot independently verify the inference. Already-present matching arrival events are not inserted twice. Invalid routes, duplicate arrivals or unaccounted transfers fail validation. Raw files remain unchanged; `adapter.json` lists every insertion.

The copy lets C report all six path visits correctly. Its header also prevents the job filter from treating the first vehicle's event as a header. Selecting `j1` explicitly checks that `j0` does not leak into the selection.

### Empty-job forwarding diagnostic

An independent diagnostic sends an already-empty job through `q2 -> q1 -> q0`, with configuration rows ordered q0/q1/q2. E logs q1 forwarding the job to q0 during admission, but q0 has already been processed that tick. The checked run has one offered job, no sink completion and no error. This is a confirmed accounting failure for that input.

The diagnostic is **not** counted as a passing road-model run. Its original evidence is retained with status `confirmed_loss_outside_supported_model`. F does not repair the simulator or fabricate a completion. The supported exit-distance model avoids this path: internal transfers always retain work and the job finishes at a configured sink. A future model allowing empty intermediate transit requires a separate E review.

### Measurement conventions

- C's `Utilization(W)` is rounded activity (0 or 1 based on whether any server was busy), not a precise or server-normalized capacity percentage.
- C's saturation is rounded mean waiting occupancy. F differences cumulative errors independently per queue before `compute-use`; `Errors(W)` is a window count, not a time-normalized rate.
- Each C USE row is checked for the 10/100/1000-step windows, including full-window expiry in the long idle fixture. Inputs are filtered to one queue first.
- Job response time means waiting until first service; system response time means external arrival to final completion. Internal queue departures do not count as whole-network throughput.
- C's unfinished-visit service field excludes elapsed service still in progress. The time-limit case checks this convention explicitly; it must not be used as the full observed busy-time integral. Complete final runs will be drained.
- System means/medians are rounded to whole steps at each departure timestamp; P10/P90 use nearest rank. They are checked against final sink departures, including ties. These are not whole-run customer percentiles.

Five C++ C tools are exercised here: both filters, `compute-use`, `derive-event-metrics`, and `derive-sys-metrics`. E's graph converter and pretty-printer are also demonstrated. The four C plotting tools and final scale-level figures remain part of the subsequent scale studies; this validation package does not claim the full Objective F assignment is complete.

## Reproduce

Requirements: Python 3.10+, Git containing the recorded commit, and a C++20 GCC compiler available as `g++` (or through `CXX`). Plotting packages are not needed for these checks. Run from the repository root containing `analysis/` and `implementation/`.

```txt
python analysis/multi-common/scripts/run.py validate --output-root analysis/multi-common/.build/check-01 --archive analysis/multi-common/.build/check-01.zip
```

Use a new output/archive name each time; existing paths are refused. Output directories must be below F's `.build/`. The command exports pinned source files, builds E/B and the five C++ C tools (or reuses verified cached builds), runs the existing E and C test programs plus E's Python pretty-printer tests, executes the fixtures, checks C's outputs and raw accounting, and creates/verifies a compact archive. It uses isolated temporary directories and a repository-specific, command-scoped Git ownership exception; global Git settings remain unchanged.

Rebuild E and replay the eight model fixtures from the packaged configurations and inputs:

```txt
python analysis/multi-common/scripts/run.py replay analysis/multi-common/validation.zip --output-root analysis/multi-common/.build/replay-01
```

Replay verifies every archive member before extraction, checks path safety, and compares regenerated raw metrics, events and sink files byte-for-byte. It also reruns the independent accounting checks. It does not regenerate C outputs or rerun the unsupported empty-transit diagnostic; use `validate` for those.

Run F's focused tests without building the simulator:

```txt
python -m unittest discover -s analysis/multi-common/tests -v
```

Tests extract temporary copies of `validation.zip` below `.build/`, deliberately corrupt copies, and leave the saved archive unchanged. `F_VALIDATION_ARCHIVE` may point to another validation archive when checking a new run before publication.

Verified September 29, 2026: all eight model cases passed and replayed with byte-identical raw outputs; all 25 F tests passed. The isolated build also passed E's 96 C++ checks, all five C++ C test programs, and E's 34 Python pretty-printer tests. The packaged archive is 70,863 bytes.

These are native functional checks. No Valgrind, sanitizer, memory-leak or coverage result is claimed. A failed execution preserves its report and partial evidence in scratch and cannot produce a successful validation archive.

## Subsequent study and artifact policy

The prepared networks have 20/200/2,000 queues, with workloads targeting 2,000/20,000/200,000 external vehicles. Each scale has one `inputs.zip` and one editable `experiments.csv`. Source provenance, actual counts, pilot measurements and archive hashes are recorded in `source.json`. Final sensitivity runs, plots and page-30 conclusions remain pending.

### Texas preparation rules

The download stays in ignored scratch. Normalize each road as an unordered pair, count reverse/duplicate records and self-loops, and preserve original node IDs. Starting at the smallest original ID, breadth-first traversal with ascending neighbors produces nested 20/200/2,000-node prefixes. Keep every original edge within each prefix. This selects a reproducible connected portion, not a statistically representative Texas sample or an identified city.

Choose up to one exit per 20 nodes from boundary nodes. Start at the smallest original boundary ID, then repeatedly choose the boundary farthest from the current exits, breaking ties by original ID. The topology records each boundary's outside neighbors. All non-exit nodes serve as ingresses, representing trips starting throughout the selected area; original A arrival order is assigned round-robin across them. This spatial assignment is deterministic and is not a measured origin distribution or independent per-node Poisson process.

To set demand, initially assign each ingress equal expected traffic, then propagate its fraction toward exits with equal splitting among allowed outgoing edges. With four servers per queue and one step per visit, `4 / maximum visit fraction` estimates the bottleneck's external-arrival capacity. Low/stressed/overloaded demand targets 0.5/1/1.5 times that rate. For target count N, choose `H = ceil(N / target rate)` and use A's Poisson rate `N/H` on its **inclusive** interval `0..H-1`. This preserves native Poisson arrival counts while making expected job counts equal across demand levels. The approximation ignores finite-run routing imbalance, queue losses and transients; pilot and final observations determine actual behavior.

Each scale archives nine original A files and nine transformed vehicle tables: three demand levels, each with seeds 42/43/44. IDs and arrival times stay unchanged; F assigns ingress and work. Variants reference the same transformed file/hash. IDs may repeat across independent experiments, but are unique across ingresses within one experiment. Counts are checked against the assignment range without resampling seeds.

The 27-row experiment manifest per scale specifies nine demand baselines and eighteen server variants at stressed demand. Baseline settings are four servers and 20 waiting spaces per queue, with `Sub-1` duration 1. Increases give 5/6/8 servers everywhere. Decreases remove an exact 10/20/30% of the **total server pool**, in rounds following recorded BFS order. These decreases are heterogeneous; their location can affect bottlenecks. Routes, waiting space, service time and workload bytes remain fixed.

Each inputs archive contains `topology.json` (original IDs, original edges, outside neighbors, exits, directions, ingress order and settings), `directed.txt`, unchanged `E-generated.csv`, workload metadata, exact server allocations and the A/transformed workload files. E's converter receives the explicitly directed edges with no ID-based `--acyclic` flag. F verifies its nodes/directions and generates simulation configurations with explicit ingress filenames, a shared exit sink, and BFS row order. E's automatic zero-indegree sources are not used as our trip-origin rule.

To reproduce preparation, download the linked `.txt.gz` into `.build/cache/` and run from the repository root:

```txt
python analysis/multi-common/scripts/prepare.py --dataset analysis/multi-common/.build/cache/roadNet-TX.txt.gz --output-root analysis/multi-common/.build/prepare-check
```

The command checks the recorded download hash, builds unchanged A/E and C's event-metrics tool in scratch, runs nine pilots, prepares all final input sets and verifies archive extraction. It writes candidate archives/manifests under the new scratch output; it does not overwrite published inputs or execute the 81-run matrix. A failed preparation retains diagnostics. Pilot originals remain local; their commands, counts, timings and raw hashes are retained in `source.json`. The published record additionally includes the completed input audit and extracted replay under `review_checks`.

The existing validator now builds per-queue counts once per timestep and groups residence records once, avoiding repeated scans across every vehicle for every queue. All original accounting checks remain in place. Thirty tests cover the original evidence plus graph normalization, boundary selection, nested sampling, exact resource totals and disconnected-component rejection.

### Measured preparation results

The source file contains 1,379,917 nodes and 3,843,320 records: 1,921,660 unique undirected edges, the same number of reverse/duplicate records, and no self-loops. The archived topology distinguishes original roads from permitted directions:

| Scale  | Queues | Original roads | Directed links | Boundary nodes / exits | Maximum exit distance |
| ------ | -----: | -------------: | -------------: | ---------------------: | --------------------: |
| SMALL  |     20 |             22 |             22 |                  7 / 1 |                     8 |
| MEDIUM |    200 |            247 |            216 |                23 / 10 |                    14 |
| LARGE  |  2,000 |          2,544 |          2,238 |                62 / 62 |                    45 |

Across the nine prepared workloads per scale, actual counts are 1,942–2,074 (SMALL), 19,768–20,085 (MEDIUM), and 199,353–200,493 (LARGE), all within the required ranges. The three input archives total 14.44 MB: approximately 0.15/1.30/12.99 MB respectively.

Pilots used seed 42 and a target of 2,000 vehicles **at every scale**. Each cell below gives completed / dropped vehicles. All runs drained, every offered vehicle was accounted for, every node was visited, and C's per-visit measurements matched the independent ledger.

| Scale  | Low demand | Stressed demand | Overloaded demand |
| ------ | ---------: | --------------: | ----------------: |
| SMALL  |  2,074 / 0 |      2,013 / 54 |       1,387 / 632 |
| MEDIUM |  2,052 / 0 |       2,003 / 0 |        1,918 / 64 |
| LARGE  | 2,035 / 32 |     1,761 / 250 |       1,664 / 316 |

These are preparation observations, not the final three-scale study. In particular, MEDIUM/LARGE pilot counts are below their assignment ranges. The demand labels describe nominal pressure, not a guarantee of zero loss at low demand. The LARGE low-demand pilot still dropped 32 jobs: finite queues, spatially ordered ingress assignment, unequal route lengths and short-run transients can concentrate traffic. The pilot does not isolate which mechanism caused each loss. Full workload files use the required ranges and the same declared rules.

An extracted SMALL input archive reproduced its low/seed-42 pilot's raw events, queue metrics and sink byte-for-byte. Preparation also checks every input archive's member hashes. The separate input audit verifies every original/transformed vehicle's ID, arrival, ingress and work, all nested node maps, resource totals, and paired experiment hashes.

| Planned scale batch (27 runs) | Estimated compressed raw evidence |
| ----------------------------- | --------------------------------: |
| SMALL                         |                            6.7 MB |
| MEDIUM                        |                          105.4 MB |
| LARGE                         |                           4.33 GB |

These are linear projections from pilot ZIP sizes per offered vehicle, weighted for three low, 21 stressed and three overloaded runs. They are **estimates**, excluding prepared inputs, analysis copies, native C results and figures; resource variants can also change output size. About 4.44 GB overall exceeds the provisional 200 MB repository budget, and MEDIUM/LARGE exceed the 50 MB archive target. The manifests describe the proposed 81 runs; they do not authorize that batch or settle evidence storage.

SMALL is complete. Before the larger batches, agree a team-accessible location for complete evidence or revise the optional replication plan explicitly. Even one seed may leave LARGE evidence too big for the original budget. Do not silently discard raw data or split it into many tracked files. The accounting checker still materializes metric/event records in memory; full LARGE workloads can produce tens of millions of metric rows. A streaming implementation or a measured full-workload memory check is needed before that batch. Pilot timings and peak-memory limitations are recorded in `source.json`.

Runtime projections in that record also scale fixed per-run file/setup costs, so they are rough feasibility indicators rather than scheduling estimates. Measure actual full-workload cost before committing to a large batch.

Keep shared inputs and consolidated per-run/per-queue results. Package raw per-run evidence rather than tracking loose experiment folders. Measure compressed size before large runs: fewer filenames do not make unbounded output practical. The current validation archive is small; it does not establish the cost of a 2,000-queue study.

## Efficient study execution

The study driver computes exact completed-vehicle mean latency from original arrival and final departure times. C's rounded per-departure statistics remain available as native demonstrations, with their original meaning.

All-queue terminal USE summaries now require one streaming pass over the metrics file, keeping only each queue's last 1,000 samples. Native filter/USE demonstrations and comparisons are limited to selected queues in plotted runs. For the current SMALL matrix this reduces those tool launches from 1,080 to 18 without omitting any queue summary. All 540 original native C queue results matched the streaming implementation exactly.

The driver checkpoints command history after each completed experiment and on command failure, instead of rewriting the growing record after every successful tool call. Commands and experiments record elapsed time; new run summaries distinguish simulation, validation, analysis and plotting. The published original SMALL timings are historical and are not presented as optimized timings.

Execution is finalized before evidence is packaged. The archive contains the completed case record; the external record adds the ZIP hash without introducing a circular self-hash. The input-writing helper is shared with validation instead of duplicated.

Keep subsequent work proportional: reuse saved raw outputs when only reporting changes, run focused regressions, and produce selected figures. A source/compiler/binary-verified build cache is a possible next improvement; the CLI currently still builds and tests tools for each new study. Full LARGE accounting memory and evidence storage remain separate requirements; the faster USE summary does not resolve them.

References: course handout **Queue Lab.pdf**, Objective F page 30; [analysis requirements](../TODO.md); [E implementation](../../implementation/obj-e/); [C implementation](../../implementation/obj-c/). The synthetic fixtures exercise the planned model without claiming measured Texas traffic behavior.

## Shared efficiency improvements and MEDIUM pilot

The shared runner now caches compiled programs in ignored `.build/build-cache/`. Each recipe includes hashes of all exported source/header files, ordered compiler arguments and include paths, the compiler path/version/target/hash, hashes of its compiler/assembler/linker programs, and relevant search-path environment settings. Exported sources are rehashed before each lookup; the cached executable and its copied executable must match the saved binary SHA-256. Missing, corrupt or mismatched entries rebuild. Native test programs and interface checks still run on cache hits. The preparation workflow uses the same compiler cache. The cache is a local performance aid, not portable evidence or a substitute for tests.

The adapter buffers one timestep and writes its analysis copy incrementally. The main accounting checker merges ordered events with one complete metric snapshot at a time; native visit CSVs are checked row by row and archive-member hashes are verified in 1 MiB blocks. Time order, contiguous snapshots, duplicate/missing rows, transfers, source/sink accounting, task timing and residence checks remain enforced. Fixture adapters are byte-identical to the archived copies, and their complete ledger results match after JSON serialization.

This removes whole-file event/metric tables. It is **not constant-memory validation**: the model and checked visit ledger still grow with the number of vehicles/visits, and unchanged C/plotting programs have their own memory costs. LARGE still needs its own full-size pilot.

Select an existing manifest row without modifying the manifest:

```txt
python analysis/multi-common/scripts/analyze.py --scale MEDIUM --run stressed-42-+0 --output-root analysis/multi-common/.build/medium-pilot-new
```

`--run` is repeatable; omitting it executes the full existing manifest. Selected-run reports explicitly identify their partial scope.

## Cross-Scale Synthesis (Objective G Technical Report Findings)

The completion of all three multi-queue scales (SMALL, MEDIUM, and LARGE) provides a direct empirical basis for the multi-queue analysis required in Section G of the Technical Report.

### Summary Comparison Across Scales

| Characteristic                             |       SMALL |        MEDIUM |     LARGE |
| ------------------------------------------ | ----------: | ------------: | --------: |
| **Sampled Graph Nodes (Queues)**           |          20 |           200 |     2,000 |
| **Directed Traffic Links**                 |          22 |           216 |     2,238 |
| **Boundary Exit Sinks**                    |           1 |            10 |        62 |
| **Maximum Distance to Exit (hops)**        |           8 |            14 |        45 |
| **Evaluated Workload Range (jobs)**        | 1,942–2,074 | 19,768–20,085 |   200,372 |
| **Total Queue Visits (Stressed Baseline)** |      11,801 |       184,812 | 3,424,613 |
| **Baseline Mean Response Time (steps)**    |        9.72 |          8.23 |     22.04 |
| **Theoretical Hop-Distance Floor (steps)** |        ~5.0 |          ~7.2 |     ~19.1 |
| **Peak Departures per Step**               |           4 |            38 |        49 |
| **Stressed Baseline Loss Rate**            |       2.61% |         0.01% |    13.11% |
| **+100% Resource Loss Rate**               |       0.00% |         0.00% |     4.53% |
| **−30% Resource Loss Rate**                |      23.46% |         4.87% |    15.76% |

### Core Architectural Findings for Section G

1. **Topological Diameter Dictates Inherent Latency Floor**:
   In multi-hop queueing networks, no amount of processing capacity can reduce latency below the shortest-path diameter. At stressed baseline, mean latency is 9.72 steps for SMALL (elevated by single-exit queuing), 8.23 for MEDIUM, and 22.04 for LARGE. Under +100% server allocations, queuing wait drops substantially (SMALL reaches 5.74 steps, MEDIUM reaches 7.22 steps, and LARGE reaches 19.11 steps, which retains ~1.04 steps of residual waiting per vehicle). Additional capacity yields diminishing returns as transit latency approaches the graph's topological distance floor.

2. **Exit Multiplicity Alleviates Boundary Chokepoints**:
   In SMALL, all 20 queues funneled into a single exit sink, creating an inescapable boundary bottleneck that dropped 31.3% of vehicles under overload. In MEDIUM, introducing 10 distributed boundary exits absorbed parallel departure flows, boosting peak throughput from 4 to 38 departures/step and reducing overloaded loss to 8.6%. In LARGE, 62 exit sinks supported peak throughput of 49 departures/step.

3. **Interior Arterial Confluence as the Scale Bottleneck**:
   In large networks (2,000 nodes), the primary chokepoint shifts from boundary exits to interior merge junctions. Because 1,938 ingress streams converge onto shared transit corridors before reaching the boundary, interior queues experience intense saturation spikes under baseline capacity (13.11% drop rate). Adding +50% to +100% capacity provides the critical headroom needed to prevent queue-full drop cascades at these confluence points.

4. **Computational Performance & Engineering Profiling**:
   - **Simulator Efficiency**: The compiled C++ event simulator (Objective E) exhibited near-linear scaling, completing 200,372 vehicles across 2,000 queues in 112–156 seconds (~120 MB RAM).
   - **Validation Overhead & Memory Attribution**: Python accounting validation peaked at **6.51 GB** working set for the child validation process, with the parent process using **2.26 GB**, totaling **8.64 GB** across the process tree. This demonstrates the critical systems distinction between lightweight discrete-event simulation and memory-intensive full-history event ledgers.
   - **Accessory Tool Scalability**: C++ tools (`derive-event-metrics`, `derive-sys-metrics`) processed millions of event records in 100–220 seconds, validating their algorithmic efficiency.

### Scale Studies Documentation

- [SMALL Scale Study (20 queues)](../multi-SMALL/README.md)
- [MEDIUM Scale Study (200 queues)](../multi-MEDIUM/README.md)
- [LARGE Scale Study (2,000 queues)](../multi-LARGE/README.md)
