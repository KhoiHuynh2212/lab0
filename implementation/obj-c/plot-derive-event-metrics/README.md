# Objective C: plot-derive-event-metrics

Histogram of one `emetric.csv` column, or scatter plot of two.
Rows with an empty value in a chosen column are skipped.

```bash
./plot-derive-event-metrics emetric.csv plot.png --hist="arrival-time" --bins=20
./plot-derive-event-metrics emetric.csv plot.png --scat="arrival-time,response-time"
```

Options: `--bins=N`, `--title="..."`, `--xlim=low,high`, `--ylim=low,high`

## Build and Test

```bash
make build
make test
make clean
```
