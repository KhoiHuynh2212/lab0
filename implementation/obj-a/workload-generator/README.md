# Obj-A Workload Generator

`workload-generator` creates a synthetic workload for the queue simulator.

Each output row contains:

```text
Job Id, arrival time, Tasks
```

Each task is written as:

```text
<task-id>: W <positive work amount>
```

The output jobs are sorted by arrival time and can be used directly as the `source` workload for Obj-B.

## Build

```bash
cd implementation/obj-a/workload-generator
make
```

The executable is:

```text
./workload-generator
```

## Supported distributions

The number of jobs arriving at each time step is drawn from one of these distributions:

- `uniform`
- `normal`
- `geometric`
- `poisson`

The time interval is inclusive. For example, `--interval 0,20` considers time steps 0 through 20.

## Basic command format

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,20 \
    --distribution uniform \
    --min 0 \
    --max 2 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 7
```

Run this for a complete option list:

```bash
./workload-generator --help
```

## Distribution statistics

### Uniform

```bash
--distribution uniform --min 0 --max 3
```

`--min` and `--max` are inclusive integer limits.

### Normal

```bash
--distribution normal --mean 2.0 --stddev 1.0
```

The sampled value is rounded to an integer. Negative values become zero because a negative number of arrivals is not possible.

### Geometric

```bash
--distribution geometric --probability 0.4
```

The probability must be greater than 0 and at most 1.

### Poisson

```bash
--distribution poisson --lambda 1.5
```

Lambda must be positive.

## Task work

Fixed task work gives every task the same work amount:

```bash
--task-work fixed --fixed-task-work 10
```

Distribution task work draws each task's work amount from the same distribution used for arrival counts:

```bash
--task-work distribution
```

Task work must always be positive. If a distribution draws zero, another value is drawn for that task.

The number of tasks in every generated job is controlled by:

```bash
--tasks-per-job 3
```

## Reproducible workloads

Use a seed when you want the same command to produce the same workload again:

```bash
--seed 42
```

Without a seed, the generator uses a random seed.

## Example output

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,1,"['0: W 10', '1: W 10']"
j2,2,"['0: W 10', '1: W 10']"
```

## Tests

```bash
./run-all-tests.sh
```

The test script checks workload generation, validation, CSV output, command-line behavior, and memory errors.
