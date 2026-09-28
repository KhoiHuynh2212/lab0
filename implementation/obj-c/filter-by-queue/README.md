# Objective C: filter-by-queue

`filter-by-queue` keeps only selected `queue-sys-id` rows from a queue metrics CSV.

The program is written in C++ and lives in:

[`/implementation/obj-c/filter-by-queue/`](/implementation/obj-c/filter-by-queue/)

The design document is here:

[`/designs/obj-c/filter-by-queue/README.md`](/designs/obj-c/filter-by-queue/README.md)

---

## Build and run (Linux)

```bash
make -C implementation/obj-c/filter-by-queue build

./implementation/obj-c/filter-by-queue/filter-by-queue \
    qmetin.csv qmetout.csv --queues=0
```

```bash
make -C implementation/obj-c/filter-by-queue test
```

---

## Usage

```bash
./filter-by-queue <input.csv> <output.csv> --queue=<id>[,<id>...]
./filter-by-queue <input.csv> <output.csv> --queues=<id>[,<id>...]
./filter-by-queue --help
```
