# Obj-A: Workload Tools

Obj-A contains two C++ programs:

- [`workload-generator`](./workload-generator/) creates synthetic workload CSV files.
- [`workload-visualizer`](./workload-visualizer/) creates PNG histograms and scatter plots from workload CSV files.

Both tools use the same workload format as Obj-B.

## Build all of Obj-A

From this directory:

```bash
make
```

Or from the repository root:

```bash
make -C implementation/obj-a
```

The executables are:

- [`implementation/obj-a/workload-generator/workload-generator`](/implementation/obj-a/workload-generator/workload-generator)
- [`implementation/obj-a/workload-visualizer/workload-visualizer`](/implementation/obj-a/workload-generator/workload-generator)

## Run the generator

```bash
implementation/obj-a/workload-generator/workload-generator \
    --out examples/obj-a/generated.csv \
    --interval 0,20 \
    --distribution poisson \
    --lambda 1.5 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 42
```

## Run the visualizer

Histogram:

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    examples/obj-a/generated.csv \
    --hist="arrival time" \
    --bins=10 \
    --out=examples/obj-a/generated-arrivals.png
```

Scatter plot:

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    examples/obj-a/generated.csv \
    --scat="arrival time,len(Tasks)" \
    --out=examples/obj-a/generated-scatter.png
```

## Connection to Obj-B

The generator writes:

```text
Job Id,arrival time,Tasks
```

That is the source workload format read by Obj-B. A generated file can therefore be placed in the `source` column of an Obj-B queue configuration.

Example configuration:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,generated.csv,out.csv,[],[]
```

Obj-B writes completed jobs back to the same workload format. The visualizer can read either a generated source workload or an Obj-B sink workload.

## Tests

Run both Obj-A suites plus an Obj-A-to-Obj-B compatibility test:

```bash
./run-all-tests.sh
```
