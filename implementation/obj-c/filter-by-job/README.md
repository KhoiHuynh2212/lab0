# Objective C: filter-by-job

`filter-by-job` keeps only selected job identifiers from an event-trace log.

The program is written in C++ and lives in:

[`/implementation/obj-c/filter-by-job/`](/implementation/obj-c/filter-by-job/)

The design document is here:

[`/designs/obj-c/filter-by-job/README.md`](/designs/obj-c/filter-by-job/README.md)

---

## Build and run (Linux)

```bash
make -C implementation/obj-c/filter-by-job build

./implementation/obj-c/filter-by-job/filter-by-job \
    jobin.log jobout.log --jobs=0
```

```bash
make -C implementation/obj-c/filter-by-job test
```

---

## Usage

```bash
./filter-by-job <input.log> <output.log> --job=<id>[,<id>...]
./filter-by-job <input.log> <output.log> --jobs=<id>[,<id>...]
./filter-by-job --help
```
