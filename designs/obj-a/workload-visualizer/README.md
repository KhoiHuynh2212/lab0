# `workload-visualizer.py`

Reads a workload file and creates a graph that makes the workload easier to understand. The program can create either a histogram of one workload value or a scatter plot comparing two values.

## Psuedocode

```txt
INPUT:
    Command-line arguments:
        1. workload filename
        2. output filename
        3. plot mode
            - histogram
            - scatter

        IF histogram mode:
            4. column name
            - bin count

        IF scatter mode:
            4. x-axis column name
            5. y-axis column name

        - x-axis limits
        - y-axis limits
        - plot title


OUTPUT:
    Image file containing inputted graph


MAIN:
    Read and validate command-line arguments

    Read the workload file

    IF the plot uses len(Tasks):
        calculate the number of tasks for each job

    IF plot mode is histogram:
        Make sure the requested column exists

        Create a histogram using that column

        IF a bin count was provided:
            use the requested number of bins

    ELSE IF plot mode is scatter:
        Make sure both requested columns exist

        Create a scatter plot using:
            x-axis column
            y-axis column

    IF x-axis limits were provided:
        apply them

    IF y-axis limits were provided:
        apply them

    IF a title was provided:
        apply it

    Save the graph to the output filename
```

## Worked Example

### Input CSV

```csv

```

### Histogram

```csv
Job Id,arrival time,Tasks
j0,0,"['0: W 10', '1: W 10']"
j1,10,"['0: W 10']"
j2,20,"['0: W 10', '1: W 10', '2: W 10']"
```

#### Input

```bash
python workload-visualizer.py workload.csv --hist="arrival time" --out="arrival-time.png"
```

#### Output

`arrival-time.png`

```bash
Histogram of arrival time:
    0  -> 1 job
    10 -> 1 job
    20 -> 1 job
```

### Scatterplot

#### Input

```bash
python workload-visualizer.py workload.csv --scat="arrival time,len(Tasks)" --out="arrival-vs-tasks.png"
```

#### Output

`arrival-vs-tasks.png`

```bash
Points plotted as (arrival time, len(Tasks)):

(0, 2)
(10, 1)
(20, 3)
```

## Resources

- Queue Lab slides, slide 2: workload format that the visualizer reads.
- Queue Lab slides, slide 5: histogram and scatter plot requirements and examples
- [`pandas.read_csv` documentation](https://pandas.pydata.org/docs/reference/api/pandas.read_csv.html)
- [Mathplotlib histogram documentation](https://matplotlib.org/stable/api/_as_gen/matplotlib.pyplot.hist.html)
- [Mathplotlib scatter documnetation](https://matplotlib.org/stable/api/_as_gen/matplotlib.pyplot.scatter.html)
- [Python `argparse` documentation](https://docs.python.org/3/library/argparse.html): Command line inputs
