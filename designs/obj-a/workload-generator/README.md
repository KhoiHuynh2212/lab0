# `workload-generator.py`

Generates a synthetic workload of jobs over a given time interval. The number of jobs arriving is based on a selected distribution, and each job contains work tasks. The finished workload is sorted by arrival time.

## Psuedocode

```txt
INPUT:
    Command-line arguments:
        1. output filename
        2. start time
        3. end time
        4. distribution type
            - uniform
            - normal
            - geometric
        5. parameters required by selected distribution
        - task work mode
            - fixed
            - distribution
        - fixed task work value, if fixed mode is selected
        - random seed


OUTPUT:
    Workload fole sorted by arrival time, containing:
        1. Job Id
        2. arrival time
        3. Tasks


MAIN:
    Read and validate command-line arguments

    Create an empty workload
    Set the next job ID to 0

    FOR each time step from start time through end time:
        Draw the number of arriving jobs from the selected distribution

        Make sure the number of arriving jobs is a non-negative integer

        FOR each arriving job:
            Create a new job ID

            Create an empty task list

            Determine the tasks for the job

            FOR each task:
                IF task work mode is fixed:
                    use the fixed work value

                ELSE:
                    draw a positive work value from the selected distribution

                Add task in the form:
                    "<task-id>: W <work>"

            Add the job to the workload with:
                job ID
                current time as arrival time
                task list

            Increase the next job ID

    Sort the workload by arrival time

    Write the workload to the output file
```

## Worked Example

### Input

```txt
Interval:
    start time = 0
    end time = 20

Distribution:
    poisson

Task work:
    fixed at 10
```

### Output

```txt
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

> Each task follows the format:  
> `{task-id}: W {positive number}`

## Resources

- Queue Lab slides, slide 2: workload format and task format.
- Queue Lab slides, slide 3: format of completed jobs.
- Queue Lab slides, slide 4: workload generator requirements and supported distributions.
- [Python `argparse` documentation](https://docs.python.org/3/library/argparse.html): Command line inputs
