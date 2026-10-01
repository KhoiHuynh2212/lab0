# 🌐 Objective E Designs

Objective E extends the single-queue simulator into a network of connected queue systems. The three designs here cover configuration generation, simulation, and readable event-log output.

[Lab 0 overview](/README.md) · [Objective E implementation](/implementation/obj-e/README.md) · [Objective E examples](/examples/obj-e/README.md)

---

# 🧭 Component Flow

```txt
┌─────────────────────┐
│ network edge list   │
│ SNAP or .mtx        │
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│ create-config-      │
│ from-network        │
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│ queue config CSV    │
└──────────┬──────────┘
           │ + source workload files
           ↓
┌─────────────────────┐
│ multi-queue-sim     │
└───────┬─────────────┘
        │
   ┌────┴─────┐
   ↓          ↓
qmet.csv    job.log
              │
              ↓
┌─────────────────────┐
│ pretty-print-events │
└─────────────────────┘
```

---

# 📚 Design Index

| Component | Design | Purpose |
| --- | --- | --- |
| `multi-queue-sim` | [Design README](/designs/obj-e/multi-queue-sim/README.md) | Run all queue-system rows together and route jobs through edges. |
| `pretty-print-events` | [Design README](/designs/obj-e/pretty-print-events/README.md) | Lay out a large event log as a queue-by-time table. |
| `create-config-from-network` | [Design README](/designs/obj-e/create-config-from-network/README.md) | Convert a network edge list into a multi-queue configuration CSV. |

---

# 🧩 Shared Configuration Idea

Each row of a multi-queue configuration describes one queue system. `Edges out` names the queue systems that may receive a departing job. `Edges in` records the reverse relationship for the configuration.

A queue can also have a source workload, a sink workload, both, or neither. The simulator resolves source and sink paths relative to the configuration CSV, which makes an example directory self-contained when the config and workload files are stored together.

See the [AS-733 example](/examples/obj-e/as-733/README.md) for a concrete network-to-simulation workflow.
