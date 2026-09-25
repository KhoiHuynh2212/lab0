# Objective A Workload Generator

`workload-generator` creates synthetic workload CSV files for the queue simulator.

The program is written in C++ and lives in:

[`/implementation/obj-a/workload-generator/`](/implementation/obj-a/workload-generator/)

Example workloads are in:

[`/examples/obj-a/`](/examples/obj-a/)

---

## 1. What the Program Does

The generator walks through an inclusive time interval one timestep at a time.

At each timestep it:

1. draws how many jobs arrive
2. creates that many jobs
3. gives every job a task list
4. writes the final workload in arrival-time order

The basic flow is:

```text
┌───────────────────────────────────────┐
│ interval + distribution + task        │
│ settings                              │
└───────────────────┬───────────────────┘
                    │
                    ↓
          ┌────────────────────┐
          │ workload-generator │
          └─────────┬──────────┘
                    │
                    ↓
          ┌────────────────────┐
          │    workload.csv    │
          └────────────────────┘
```

---

## 2. Input

All input is given through command-line options.

The generator needs:

- an output filename
- an interval
- a distribution
- the statistics required by that distribution

Optional settings control:

- tasks per job
- whether task work is fixed or random
- fixed task work
- random seed
- job-ID prefix

### Interval

The normal form is:

```bash
--interval 0,20
```

The interval is inclusive, so this uses timesteps `0` through `20`.

The implementation also accepts separate start and end options:

```bash
--start 0 --end 20
```

Use either `--interval` or the `--start`/`--end` pair.

---

## 3. Output

The output is a workload CSV:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,1,"['0: W 10', '1: W 10']"
j2,2,"['0: W 10', '1: W 10']"
```

Each row represents one job.

|     Column     | Meaning                                 |
| :------------: | :-------------------------------------- |
|    `Job Id`    | unique job name                         |
| `arrival time` | timestep when the job enters the system |
|    `Tasks`     | ordered list of work for the job        |

Each task is written as:

```txt
<task-id>: W <positive work amount>
```

For example:

```txt
0: W 10
```

The generator creates parent directories for the output path when needed.

---

## 4. Build

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
make -C implementation/obj-a/workload-generator
```

or:

```bash
make -C implementation/obj-a/workload-generator build
```

The executable is:

```txt
implementation/obj-a/workload-generator/workload-generator
```

---

## 5. Clean

From this directory:

```bash
make clean
```

From the repository root:

```bash
make -C implementation/obj-a/workload-generator clean
```

---

## 6. General Command Format

From this directory:

```bash
./workload-generator \
    --out <file> \
    --interval <start,end> \
    --distribution <uniform|normal|geometric|poisson> \
    <distribution options> \
    [task options] \
    [other options]
```

From the repository root:

```bash
implementation/obj-a/workload-generator/workload-generator \
    --out <file> \
    --interval <start,end> \
    --distribution <uniform|normal|geometric|poisson> \
    <distribution options>
```

Options may be written either way:

```bash
--seed 42
```

or:

```bash
--seed=42
```

The same is true for the other `--name value` options.

---

## 7. Uniform Distribution

Uniform needs an inclusive minimum and maximum:

```bash
--distribution uniform --min 0 --max 3
```

Full example:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,20 \
    --distribution uniform \
    --min 0 \
    --max 3 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 7
```

At each timestep, the number of arriving jobs can be any integer from `0` through `3`.

---

## 8. Normal Distribution

Normal needs a mean and a positive standard deviation:

```bash
--distribution normal --mean 2.0 --stddev 1.0
```

Full example:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,20 \
    --distribution normal \
    --mean 2.0 \
    --stddev 1.0 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 7
```

The sampled number is rounded to an integer. A negative arrival result becomes `0`.

---

## 9. Geometric Distribution

Geometric needs a probability greater than `0` and at most `1`:

```bash
--distribution geometric --probability 0.4
```

Full example:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,20 \
    --distribution geometric \
    --probability 0.4 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 7
```

When geometric is also used for task work, probability `1` is not allowed because that setting always draws `0` and task work must be positive.

---

## 10. Poisson Distribution

Poisson needs a positive lambda:

```bash
--distribution poisson --lambda 1.5
```

Full example:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,50 \
    --distribution poisson \
    --lambda 1.5 \
    --tasks-per-job 3 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 42
```

---

## 11. Task Options

### Number of Tasks Per Job

The default is one task per job:

```bash
--tasks-per-job 1
```

For three tasks per job:

```bash
--tasks-per-job 3
```

The number must be at least `1`.

### Fixed Task Work

Fixed work gives every task the same work amount:

```bash
--task-work fixed --fixed-task-work 10
```

This is the default mode. If `--fixed-task-work` is omitted, the default work amount is `10`.

Example task list:

```txt
['0: W 10', '1: W 10']
```

### Distributed Task Work

To draw each task's work from the same distribution used for arrivals:

```bash
--task-work distribution
```

Example:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,20 \
    --distribution poisson \
    --lambda 1.5 \
    --tasks-per-job 2 \
    --task-work distribution \
    --seed 42
```

A task must have positive work. If a draw produces `0`, the generator draws again for that task.

---

## 12. Random Seed

Use a seed when the same command should create the same random workload again:

```bash
--seed 42
```

Example:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,10 \
    --distribution uniform \
    --min 0 \
    --max 2 \
    --seed 42
```

Without `--seed`, the program uses a random seed.

---

## 13. Job-ID Prefix

The default job prefix is:

```txt
j
```

so jobs look like:

```txt
j0
j1
j2
```

To use another prefix:

```bash
--job-prefix job
```

then the IDs look like:

```txt
job0
job1
job2
```

---

## 14. Help

From this directory:

```bash
./workload-generator --help
```

or:

```bash
./workload-generator -h
```

From the repository root:

```bash
implementation/obj-a/workload-generator/workload-generator --help
```

---

## 15. Tests

The Makefile has these test targets:

```bash
make test
make run-tests-coverage
make run-tests-leak
```

From the repository root:

```bash
make -C implementation/obj-a/workload-generator test
make -C implementation/obj-a/workload-generator run-tests-coverage
make -C implementation/obj-a/workload-generator run-tests-leak
```

The Makefile test targets require Valgrind.

The component test script can also be run directly:

```bash
implementation/obj-a/workload-generator/run-all-tests.sh
```

---

## 16. Connection to Obj-B

The generated file is already in the source-workload format used by Obj-B:

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
          ↓
┌────────────────────┐
│  simple-queue-sim  │
└────────────────────┘
```

A queue configuration can point its `source` column at the generated file:

```csv
Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out
q0,8,1,Sub-10*,10,workload.csv,out.csv,[],[]
```

For a complete Obj-A-to-Obj-B example, see:

[`/README.md#5-running-obj-a-and-obj-b-together`](/README.md#5-running-obj-a-and-obj-b-together)

For generator design details, see:

[`/designs/obj-a/workload-generator/README.md`](/designs/obj-a/workload-generator/README.md)
