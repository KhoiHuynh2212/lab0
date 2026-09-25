# Obj-A Examples

This directory contains workload files and plots for the workload generator and visualizer.

## Workload files

- [`wl-example.csv`](./wl-example.csv) - small three-job source workload using the format shown in the lab slides.
- [`wl-completed-example.csv`](./wl-completed-example.csv) - completed-job workload where `Tasks` is empty and the arrival-time field stores the time the job left the system.
- [`wl-uniform.csv`](./wl-uniform.csv) - generated with a uniform arrival distribution.
- [`wl-normal.csv`](./wl-normal.csv) - generated with a normal arrival distribution.
- [`wl-geometric.csv`](./wl-geometric.csv) - generated with a geometric arrival distribution and distributed task work.
- [`wl-poisson.csv`](./wl-poisson.csv) - generated with a Poisson arrival distribution and distributed task work.

Every workload uses the same columns expected by Obj-B:

```text
Job Id,arrival time,Tasks
```

## Recreate the plots

Build the visualizer first, then run:

```bash
implementation/obj-a/workload-visualizer/workload-visualizer \
    examples/obj-a/wl-poisson.csv \
    --hist="arrival time" \
    --bins=10 \
    --title="Poisson Arrival Times" \
    --out=examples/obj-a/hist-arrival-time.png

implementation/obj-a/workload-visualizer/workload-visualizer \
    examples/obj-a/wl-example.csv \
    --hist="len(Tasks)" \
    --bins=3 \
    --title="Tasks Per Job" \
    --out=examples/obj-a/hist-task-count.png

implementation/obj-a/workload-visualizer/workload-visualizer \
    examples/obj-a/wl-example.csv \
    --scat="arrival time,len(Tasks)" \
    --title="Arrival Time vs Task Count" \
    --out=examples/obj-a/scatter-arrival-vs-tasks.png
```
