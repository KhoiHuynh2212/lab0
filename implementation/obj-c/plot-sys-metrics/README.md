# Objective C: plot-sys-metrics

Plots `sysmetric.csv` from `derive-sys-metrics`: response time (avg, median, P10, P90)
on top, throughput bars below.

```bash
./plot-sys-metrics sysmetric.csv plot.png
./plot-sys-metrics sysmetric.csv plot.png --title="Run 1"
```

Expected columns:

```csv
time-step,Avg-response-time,median-response-time,10-response-time,90-response-time,throughput
```

## Build and Test

```bash
make build
make test
make clean
```
