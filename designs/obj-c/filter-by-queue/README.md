# Objective C: filter-by-queue

`filter-by-queue` keeps only the rows for selected queue systems in a queue metrics CSV file such as `qmet.csv` from Objective B.

The program is written in **C++** (same toolchain as Objectives A and B) and lives in:

[`/implementation/obj-c/filter-by-queue/`](/implementation/obj-c/filter-by-queue/)

---

## 1. What the Program Does

The tool reads a metrics CSV, finds the `queue-sys-id` column, and writes a new CSV with the same header and only the rows whose queue id appears in the filter list.

```text
┌──────────────┐     ┌─────────────────┐     ┌───────────────┐
│  qmetin.csv  │ ──→ │ filter-by-queue │ ──→ │  qmetout.csv  │
└──────────────┘     └─────────────────┘     └───────────────┘
```

---

## 2. Input CSV

The file must contain a header row with a `queue-sys-id` column (case and punctuation in the header name are ignored when locating the column).

Example input:

```csv
time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors
0,0,1,0,0
0,1,2,1,0
1,0,0,1,0
1,1,1,0,0
```

This matches the metrics format written by [`simple-queue-sim`](/implementation/obj-b/simple-queue-sim/).

---

## 3. Command-Line Interface

```bash
./filter-by-queue <input.csv> <output.csv> --queue=<id>[,<id>...]
./filter-by-queue <input.csv> <output.csv> --queues=<id>[,<id>...]
./filter-by-queue --help
```

| Argument | Meaning |
| --- | --- |
| `input.csv` | Source metrics file |
| `output.csv` | Filtered output file |
| `--queue=` / `--queues=` | One or more comma-separated `queue-sys-id` values to keep |
| `--help`, `-h` | Show usage text |

Examples:

```bash
./filter-by-queue qmetin.csv qmetout.csv --queues=0
./filter-by-queue qmetin.csv qmetout.csv --queue=0,2,3
```

---

## 4. Build and Test (Linux)

Requires `g++` (C++20), `make`, and Valgrind for the test suite.

```bash
make -C implementation/obj-c/filter-by-queue build
make -C implementation/obj-c/filter-by-queue test
```

```bash
./implementation/obj-c/filter-by-queue/filter-by-queue \
    qmetin.csv qmetout.csv --queues=0
```

---

## 5. Implementation Notes

- Language and standard: C++20 with `g++`, matching Obj-A / Obj-B.
- I/O: standard library `<fstream>` / `<iostream>` (no platform-specific APIs).
- Header matching: same alphanumeric-normalized column lookup style as Obj-B.
- Tests: Valgrind memcheck plus optional `gcov` coverage, same pattern as Obj-B.

---

## 6. Related Documentation

- Objective B metrics output: [`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)
- Implementation README: [`/implementation/obj-c/filter-by-queue/README.md`](/implementation/obj-c/filter-by-queue/README.md)
- Repository overview: [`/README.md`](/README.md)
