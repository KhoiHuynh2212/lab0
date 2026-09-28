# Objective C: compute-use

`compute-use` computes Brendan Gregg’s USE-method metrics (Utilization, Saturation, Errors) over sliding windows of 10, 100, and 1000 time steps from a queue-metrics CSV.

The program is written in C++ and lives in:

[`/implementation/obj-c/compute-use/`](/implementation/obj-c/compute-use/)

The design document is here:

[`/designs/obj-c/compute-use/README.md`](/designs/obj-c/compute-use/README.md)

---

## Build and run (Linux)

```bash
make -C implementation/obj-c/compute-use build

./implementation/obj-c/compute-use/compute-use \
    qmet.csv quse.csv
```

```bash
make -C implementation/obj-c/compute-use test
```

---

## Usage

```bash
./compute-use <input.csv> <output.csv>
./compute-use --help
```
