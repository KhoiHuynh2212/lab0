# Objective C: derive-sys-metrics

`derive-sys-metrics` turns an event-trace log into a CSV of system response-time and throughput statistics per exit time-step.

The program is written in C++ and lives in:

[`/implementation/obj-c/derive-sys-metrics/`](/implementation/obj-c/derive-sys-metrics/)

The design document is here:

[`/designs/obj-c/derive-sys-metrics/README.md`](/designs/obj-c/derive-sys-metrics/README.md)

---

## Build and run (Linux)

```bash
make -C implementation/obj-c/derive-sys-metrics build

./implementation/obj-c/derive-sys-metrics/derive-sys-metrics \
    jobin.log sysmetric.csv
```

```bash
make -C implementation/obj-c/derive-sys-metrics test
```

---

## Usage

```bash
./derive-sys-metrics <input.log> <output.csv>
./derive-sys-metrics --help
```
