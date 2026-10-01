# Objective C: plot-use

`plot-use` turns rolling USE-method history from `compute-use` into three separate PNG plots for analysis: Utilization, Saturation, and Error Rate versus time-step. Window sizes 10 / 100 / 1000 are drawn as shades of each metric’s color family.

---

# 1. What the Program Does

```txt
┌──────────────┐     ┌───────────┐     ┌─────────────────────────┐
│   quse.csv   │ ──→ │ plot-use  │ ──→ │ results/utilization.png │
└──────────────┘     └───────────┘     │ results/saturation.png  │
                                       │ results/error-rate.png  │
                                       └─────────────────────────┘
```

|    Output file    |   Y-axis    | Series colors                           |
| :---------------: | :---------: | :-------------------------------------- |
| `utilization.png` | Utilization | Blue shades for windows 10 / 100 / 1000 |
| `saturation.png`  | Saturation  | Yellow shades (filled under the curves) |
| `error-rate.png`  | Error Rate  | Red shades                              |

Shorter windows use darker shades so fast-moving signals stay visually prominent. Prefer feeding a single queue’s `quse.csv` (after `filter-by-queue` → `compute-use`).

These USE plots are meant to sit alongside later Job response-time and throughput plots for the same time axis.

---

# 2. Input CSV

Exact header produced by [`compute-use`](/designs/obj-c/compute-use/README.md):

```csv
time-step,Utilization(10),Saturation(10),Errors(10),Utilization(100),Saturation(100),Errors(100),Utilization(1000),Saturation(1000),Errors(1000)
0,0,0,0,0,0,0,0,0,0
1,1,1,0,0.8,0.5,0,0.6,0.4,0
```

Column names are matched with alphanumeric case-insensitive lookup. Numeric fields must be non-negative.

---

# 3. Output PNGs

By default, plots are written under [`/implementation/obj-c/plot-use/results/`](/implementation/obj-c/plot-use/results/):

- `utilization.png`
- `saturation.png`
- `error-rate.png`

---

# 4. Command-Line Interface

```bash
./plot-use <quse.csv>
./plot-use <quse.csv> <results_dir>
./plot-use --help
```

|    Argument    | Meaning                                                 |
| :------------: | :------------------------------------------------------ |
|   `quse.csv`   | USE-metrics input from `compute-use`                    |
| `results_dir`  | Optional output directory (default: `plot-use/results`) |
| `--help`, `-h` | Show usage text                                         |

Example:

```bash
./plot-use tests/fixtures/quse.csv
```

---

# 5. Build and Test (Linux)

Requires `python3` with `matplotlib`.

```bash
make -C implementation/obj-c/plot-use build
make -C implementation/obj-c/plot-use test
```

Install dependencies if needed:

```bash
python3 -m pip install matplotlib
```

---

# 6. Implementation Notes

- Language: Python 3 with matplotlib (Agg backend) and the standard library `csv` module.
- Color guidance from the course slides: one color family per metric, shades for the 10 / 100 / 1000 windows.
- Tests: fixture-driven PNG signature checks plus CLI failure cases.

---

# 7. Related Documentation

- Upstream metrics: [`/designs/obj-c/compute-use/README.md`](/designs/obj-c/compute-use/README.md)
- USE method: [https://www.brendangregg.com/usemethod.html](https://www.brendangregg.com/usemethod.html)
- Latency / throughput under load: [https://blog.danslimmon.com/2019/03/25/latency-and-throughput-optimized-clusters-under-load/](https://blog.danslimmon.com/2019/03/25/latency-and-throughput-optimized-clusters-under-load/)
- Implementation README: [`/implementation/obj-c/plot-use/README.md`](/implementation/obj-c/plot-use/README.md)
- Repository overview: [`/README.md`](/README.md)
