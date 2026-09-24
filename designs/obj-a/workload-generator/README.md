# Workload Generator Design

The workload generator creates a synthetic workload over an inclusive time interval. At each time step, it draws the number of arriving jobs from the selected distribution. Every job is given a sequential job ID and a task list.

The output format is:

```text
Job Id,arrival time,Tasks
```

Each task has the form:

```text
<task-id>: W <positive number>
```

## Inputs

The generator accepts all of its settings through command-line arguments.

Required inputs are:

- output filename
- interval start and end
- distribution type
- statistics required by that distribution

Supported distributions are:

- uniform
- normal
- geometric
- Poisson

Task work can either be fixed or drawn from the same distribution.

## Pseudocode

```text
read command-line arguments
validate interval, distribution statistics, and task settings
create random-number engine
create empty workload
set next job number to 0

for each time step from interval start through interval end:
    draw number of arriving jobs from selected distribution
    convert the result to a non-negative integer

    repeat once for each arriving job:
        create job ID using next job number
        create empty task list

        for each task assigned to the job:
            if task work is fixed:
                use fixed work amount
            else:
                draw from selected distribution until work is positive

            add task as "<task-id>: W <work>"

        add job with current time as arrival time
        increase next job number

sort jobs by arrival time
write CSV header
write every job to output workload
```

## Distribution behavior

### Uniform

Draw an integer from an inclusive minimum and maximum.

### Normal

Draw a real value from the requested mean and standard deviation, round it to an integer, and use zero if the rounded arrival count would be negative.

### Geometric

Use the probability parameter to draw the number of failures before a success. The result is already a non-negative integer.

### Poisson

Use lambda as the expected event count. The result is already a non-negative integer.

## Worked example

Command:

```bash
./workload-generator \
    --out workload.csv \
    --interval 0,2 \
    --distribution uniform \
    --min 1 \
    --max 1 \
    --tasks-per-job 2 \
    --task-work fixed \
    --fixed-task-work 10 \
    --seed 1
```

Because the uniform range is exactly 1 through 1, one job arrives at every time step.

Output:

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,1,"['0: W 10', '1: W 10']"
j2,2,"['0: W 10', '1: W 10']"
```

The output is already sorted by arrival time and can be used as an Obj-B source workload.

## C++ resources

The implementation uses the C++ standard random library:

- `std::uniform_int_distribution`
- `std::normal_distribution`
- `std::geometric_distribution`
- `std::poisson_distribution`
- `std::mt19937_64`

The CSV writer uses only the C++ standard library.
