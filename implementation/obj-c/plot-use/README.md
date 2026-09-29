# Objective C: plot-use

`plot-use` turns a USE-metrics CSV (`quse.csv` from `compute-use`) into three separate PNG plots: Utilization, Saturation, and Error Rate versus time-step.

The program is written in **Python** (matplotlib) and lives in:

[`/implementation/obj-c/plot-use/`](/implementation/obj-c/plot-use/)

The design document is here:

[`/designs/obj-c/plot-use/README.md`](/designs/obj-c/plot-use/README.md)

---

## Build and run (Linux)

```bash
make -C implementation/obj-c/plot-use build

./implementation/obj-c/plot-use/plot-use \
    implementation/obj-c/plot-use/tests/fixtures/quse.csv
```

PNGs are written to `implementation/obj-c/plot-use/results/` by default:

- `utilization.png`
- `saturation.png`
- `error-rate.png`

```bash
make -C implementation/obj-c/plot-use test
```

---

## Usage

```bash
./plot-use <quse.csv>
./plot-use <quse.csv> <results_dir>
./plot-use --help
```
