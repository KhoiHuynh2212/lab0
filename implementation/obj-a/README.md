# Objective A: Workload Tools

Objective A contains two C++ programs:

- [`workload-generator/`](./workload-generator/) creates synthetic workload CSV files.
- [`workload-visualizer/`](./workload-visualizer/) creates PNG histograms and scatter plots from workload CSV files.

Both programs use the same workload format as Objective B.

```text
             ┌────────────────────┐
             │ workload-generator │
             └─────────┬──────────┘
                       │
                       ↓
             ┌────────────────────┐
             │    workload.csv    │
             └─────────┬──────────┘
                       │
              ┌────────┴────────┐
              │                 │
              ↓                 ↓
┌─────────────────────┐  ┌─────────────────┐
│ workload-visualizer │  │      Obj-B      │
└──────────┬──────────┘  └────────┬────────┘
           │                      │
           ↓                      ↓
┌─────────────────────┐  ┌─────────────────┐
│      plot.png       │  │  queue outputs  │
└─────────────────────┘  └─────────────────┘
```

---

## 1. Input and Output

### Workload Generator

Input is given through command-line options such as:

- interval
- distribution
- distribution statistics
- tasks per job
- task-work mode
- optional seed

Output is a CSV workload:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
```

### Workload Visualizer

Input is a workload CSV plus plot options.

Output is a PNG image:

```text
┌──────────────┐     ┌─────────────────────┐     ┌──────────┐
│ workload.csv │ ──→ │ workload-visualizer │ ──→ │ plot.png │
└──────────────┘     └─────────────────────┘     └──────────┘
```

---

## 2. Build All of Obj-A

From this directory:

```bash
make
```

or:

```bash
make build
```

From the repository root:

```bash
make -C implementation/obj-a
```

or:

```bash
make -C implementation/obj-a build
```

The executables are:

```txt
implementation/obj-a/workload-generator/workload-generator
implementation/obj-a/workload-visualizer/workload-visualizer
```

---

## 3. Clean Obj-A

From this directory:

```bash
make clean
```

From the repository root:

```bash
make -C implementation/obj-a clean
```

---

## 4. Run the Workload Generator

A uniform example:

```bash
implementation/obj-a/workload-generator/workload-generator \
    --out generated.csv \
    --interval 0,20 \
    --distribution uniform \
    --min 0 \
    --max 2 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 7
```

A Poisson example:

```bash
implementation/obj-a/workload-generator/workload-generator \
    --out generated.csv \
    --interval 0,50 \
    --distribution poisson \
    --lambda 1.5 \
    --tasks-per-job 1 \
    --task-work distribution \
    --seed 42
```

For every generator option and every supported distribution, see:

[`/implementation/obj-a/workload-generator/README.md`](/implementation/obj-a/workload-generator/README.md)

---

## 5. Run the Workload Visualizer

### Histogram

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    generated.csv \
    --hist="arrival time" \
    --bins=10 \
    --out=generated-arrivals.png
```

### Scatter Plot

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    generated.csv \
    --scat="arrival time,len(Tasks)" \
    --out=generated-scatter.png
```

For titles, limits, image size, and every plot option, see:

[`/implementation/obj-a/workload-visualizer/README.md`](/implementation/obj-a/workload-visualizer/README.md)

---

## 6. Run the Obj-A Tests

The Makefile supports these test commands:

```bash
make test
make run-tests-coverage
make run-tests-leak
```

From the repository root:

```bash
make -C implementation/obj-a test
make -C implementation/obj-a run-tests-coverage
make -C implementation/obj-a run-tests-leak
```

The tests use Valgrind, so Valgrind must be available before using the Makefile test targets.

The test script can also be run directly:

```bash
implementation/obj-a/run-all-tests.sh
implementation/obj-a/run-all-tests.sh all
implementation/obj-a/run-all-tests.sh coverage
implementation/obj-a/run-all-tests.sh leak
```

The Obj-A test script runs:

```text
┌──────────────────────────┐
│ workload-generator tests │
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│ workload-visualizer tests│
└────────────┬─────────────┘
             │
             ↓
┌──────────────────────────┐
│ Obj-A → Obj-B            │
│ compatibility test       │
└──────────────────────────┘
```

The compatibility test confirms that a generated Obj-A workload can be read by Obj-B and that the visualizer can read the same workload format.

---

## 7. Connection to Obj-B

The generator writes:

```txt
Job Id,arrival time,Tasks
```

Obj-B expects the same format for its source workload.

For example:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,generated.csv,out.csv,[],[]
```

The full connection is:

```text
             ┌─────────────────┐
             │ Obj-A Generator │
             └────────┬────────┘
                      │
                      ↓
             ┌─────────────────┐
             │  generated.csv  │
             └────────┬────────┘
                      │
                      ↓
             ┌─────────────────┐
             │ Obj-B Simulator │
             └────────┬────────┘
                      │
             ┌────────┼────────┐
             │        │        │
             ↓        ↓        ↓
       ┌─────────┐ ┌──────────┐ ┌─────────┐
       │ out.csv │ │ qmet.csv │ │ job.log │
       └─────────┘ └──────────┘ └─────────┘
```

Obj-B writes completed jobs back to the same workload format. The visualizer can therefore read both:

- source workloads created by Obj-A
- sink workloads created by Obj-B

For a full Obj-A-to-Obj-B walkthrough, see the main [`/README.md`](/README.md#5-running-obj-a-and-obj-b-together).

---

## 8. Related Documentation

- [Obj-A design](/designs/obj-a/README.md)
- [Generator design](/designs/obj-a/workload-generator/README.md)
- [Visualizer design](/designs/obj-a/workload-visualizer/README.md)
- [Obj-A examples](/examples/obj-a/README.md)
- [Obj-B simulator](/implementation/obj-b/simple-queue-sim/README.md)
