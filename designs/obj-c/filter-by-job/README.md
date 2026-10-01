# 📌 Objective C: filter-by-job

`filter-by-job` keeps only selected job identifiers from an event-trace log such as the event log produced by Objective B / create-event-trace.

---

# 1. What the Program Does

The tool reads an event-trace log, inspects the job identifier token on each line (`j0`, `j1`, …), and writes a new log with the same header and only the lines whose job id appears in the filter list.

```txt
┌──────────────┐     ┌───────────────┐     ┌───────────────┐
│  jobin.log   │ ──→ │ filter-by-job │ ──→ │  jobout.log   │
└──────────────┘     └───────────────┘     └───────────────┘
```

---

# 2. Input Event Log

The file must begin with a header line (`Event Log`). Each following non-empty line has this token layout:

| Field |       Example       | Meaning                   |
| :---: | :-----------------: | :------------------------ |
|   0   |   `0`, `10`, `20`   | Timestamp                 |
|   1   |     `j0`, `j1`      | Job identifier            |
|   2   |        `IN`         | Literal keyword           |
|   3   |        `q0`         | Queue identifier          |
|  4+   | `ARRIVE-VIA in0.wl` | Event action and metadata |

Example input:

```txt
Event Log
0 j0 IN q0 ARRIVE-VIA in0.wl
0 j0 IN q0 ENTERS-SERVER 0
10 j1 IN q0 ARRIVE-VIA in0.wl
20 j0 IN q0 EXITS-SERVER 0
20 j2 IN q0 ARRIVE-VIA in0.wl
```

---

# 3. Command-Line Interface

```bash
./filter-by-job <input.log> <output.log> --job=<id>[,<id>...]
./filter-by-job <input.log> <output.log> --jobs=<id>[,<id>...]
./filter-by-job --help
```

|       Argument       | Meaning                                     |
| :------------------: | :------------------------------------------ |
|     `input.log`      | Source event-trace file                     |
|     `output.log`     | Filtered output file                        |
| `--job=` / `--jobs=` | One or more comma-separated job ids to keep |
|    `--help`, `-h`    | Show usage text                             |

Job ids may be written with or without the leading `j` (for example `0` and `j0` both select job `j0`).

Examples:

```bash
./filter-by-job jobin.log jobout.log --jobs=0
./filter-by-job jobin.log jobout.log --job=0,1,2
```

With `--jobs=0`, only lines whose second token is `j0` are retained (plus the header).

---

# 4. Build and Test

```bash
make -C implementation/obj-c/filter-by-job build
make -C implementation/obj-c/filter-by-job test
```

```bash
./implementation/obj-c/filter-by-job/filter-by-job \
    jobin.log jobout.log --jobs=0
```

---

# 5. Implementation Notes

- Language and standard: C++20 with `g++`, matching Obj-A / Obj-B / filter-by-queue.
- I/O: standard library `<fstream>` / `<iostream>` (no platform-specific APIs).
- Filtering: whitespace-token parse; match field 1 against a normalized `j<id>` set.
- Tests: Valgrind memcheck plus optional `gcov` coverage, same pattern as Obj-B / filter-by-queue.

---

# 6. Related Documentation

- Implementation README: [`/implementation/obj-c/filter-by-job/README.md`](/implementation/obj-c/filter-by-job/README.md)
- Sibling tool: [`/designs/obj-c/filter-by-queue/README.md`](/designs/obj-c/filter-by-queue/README.md)
- Repository overview: [`/README.md`](/README.md)
