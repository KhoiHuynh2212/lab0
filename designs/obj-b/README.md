# Objective B Design

Objective B is the simple queue simulator.

The simulator takes a queue configuration and a source workload, then advances the system one timestep at a time.

```text
┌───────────────────────────────────────┐
│ queue configuration + source workload │
└───────────────────┬───────────────────┘
                    │
                    ↓
            ┌──────────────────┐
            │ simple-queue-sim │
            └────────┬─────────┘
                     │
         ┌───────────┼───────────┐
         │           │           │
         ↓           ↓           ↓
┌─────────────┐ ┌──────────┐ ┌──────────┐
│sink workload│ │ qmet.csv │ │ job.log  │
└─────────────┘ └──────────┘ └──────────┘
```

The detailed design is in:

[`/designs/obj-b/simple-queue-sim/README.md`](/designs/obj-b/simple-queue-sim/README.md)

The working C++ implementation is in:

[`/implementation/obj-b/simple-queue-sim/`](/implementation/obj-b/simple-queue-sim/)

The provided queue example and its explanation are in:

- [`/examples/obj-b/q_ex-1.py`](/examples/obj-b/q_ex-1.py)
- [`/examples/obj-b/q_ex-1_explained.md`](/examples/obj-b/q_ex-1_explained.md)

The provided Python example is useful for understanding the simulation clock, FIFO behavior, waiting jobs, and server work. The C++ Obj-B implementation adds the configurable queue, multiple servers, task lists, server functions, files, metrics, and event logging required by the larger simulator.
