# Objective C Designs

Objective C turns simulator output into smaller data sets, derived metrics, plots, and event traces. The design documents in this directory explain what each tool expects, what it produces, and the algorithm behind it before focusing on implementation details.

---

# Tool Map

```txt
                         qmet.csv
                            │
               ┌────────────┴────────────┐
               ↓                         ↓
       filter-by-queue               compute-use
               │                         │
               │ filtered qmet.csv       │ quse.csv
               │                         ↓
               │                      plot-use
               │
job.log ───────┼──────────────┬─────────────────────┐
   │           │              │                     │
   ↓           ↓              ↓                     ↓
filter-by-job  derive-       derive-event-       create-event-
               sys-metrics   metrics             trace
                  │              │                   │
                  ↓              ↓                   ↓
             sysmetric.csv   emetric.csv          PNG trace
                  │              │
                  ↓              ↓
             plot-sys-       plot-derive-
             metrics         event-metrics
                  │              │
                  ↓              ↓
                 PNG            PNG
```

The tools do not all have to be used in one run. Choose the branch that answers the question you are investigating.

---

# Design Index

|            Tool             | Design                                                     | Main input        | Main output                |
| :-------------------------: | :--------------------------------------------------------- | ----------------- | -------------------------- |
|      `filter-by-queue`      | [Link](/designs/obj-c/filter-by-queue/README.md)           | `qmet.csv`        | filtered queue metrics CSV |
|       `filter-by-job`       | [Link](/designs/obj-c/filter-by-job/README.md)             | `job.log`         | filtered event log         |
|        `compute-use`        | [Link](/designs/obj-c/compute-use/README.md)               | queue metrics CSV | `quse.csv`                 |
|         `plot-use`          | [Link](/designs/obj-c/plot-use/README.md)                  | `quse.csv`        | three PNG plots            |
|    `derive-sys-metrics`     | [Link](/designs/obj-c/derive-sys-metrics/README.md)        | `job.log`         | `sysmetric.csv`            |
|     `plot-sys-metrics`      | [Link](/designs/obj-c/plot-sys-metrics/README.md)          | `sysmetric.csv`   | PNG plot                   |
|   `derive-event-metrics`    | [Link](/designs/obj-c/derive-event-metrics/README.md)      | `job.log`         | `emetric.csv`              |
| `plot-derive-event-metrics` | [Link](/designs/obj-c/plot-derive-event-metrics/README.md) | `emetric.csv`     | histogram or scatter PNG   |
|    `create-event-trace`     | [Link](/designs/obj-c/create-event-trace/README.md)        | `job.log`         | event-trace PNG            |

---

# 🔄 Relationship to the Simulators

Objective B and Objective E produce the two files Objective C uses most often:

```txt
┌──────────────────────┐
│ queue simulator      │
│ simple or multi      │
└──────────┬───────────┘
           │
     ┌─────┴─────┐
     ↓           ↓
┌──────────┐  ┌──────────┐
│ qmet.csv │  │ job.log  │
└────┬─────┘  └────┬─────┘
     │             │
     └──────┬──────┘
            ↓
┌──────────────────────┐
│ Objective C tools    │
└──────────────────────┘
```
