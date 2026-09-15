# 🔬 Lab 0

This lab involves building a queueing system simulator and supporting workload, visualization, and analysis tools, then using them to model and evaluate both single-queue and multi-queue real-world systems.

- [🔬 Lab 0](#-lab-0)
- [🔧 Initial Setup](#-initial-setup)
- [🌳 Directory](#-directory)
- [📋 Objectives](#-objectives)
  - [Obj A](#obj-a)
  - [Obj B](#obj-b)
- [🔗 Links](#-links)
- [👥 Group 2](#-group-2)

# 🔧 Initial Setup

```bash
git clone git@gitlab.com:dos-team2/lab0
```

# 🌳 Directory

```
.
├── .gitlab-ci.yml
├── analysis
│   ├── multi-LARGE
│   ├── multi-MEDIUM
│   ├── multi-SMALL
│   └── simple
├── designs
│   ├── obj-a
│   │   ├── workload-generator
│   │   └── workload-visualizer
│   ├── obj-b
│   │   └── simple-queue-sim
│   ├── obj-c
│   │   ├── compute-use
│   │   ├── create-event-trace
│   │   ├── derive-event-metrics
│   │   ├── derive-sys-metrics
│   │   ├── filter-by-job
│   │   ├── filter-by-queue
│   │   ├── plot-derive-event-metrics
│   │   ├── plot-sys-metrics
│   │   └── plot-use
│   └── obj-e
│       ├── create-config-from-network
│       ├── multi-queue-sim
│       └── pretty-print-events
├── examples
│   ├── obj-a
│   ├── obj-b
│   ├── obj-c
│   └── obj-e
├── implementation
│   ├── obj-a
│   │   ├── workload-generator
│   │   └── workload-visualizer
│   ├── obj-b
│   │   └── simple-queue-sim
│   │       ├── run-all-tests.sh
│   │       └── simple-queue-sim
│   ├── obj-c
│   │   ├── compute-use
│   │   ├── create-event-trace
│   │   ├── derive-event-metrics
│   │   ├── derive-sys-metrics
│   │   ├── filter-by-job
│   │   ├── filter-by-queue
│   │   ├── plot-derive-event-metrics
│   │   ├── plot-sys-metrics
│   │   └── plot-use
│   └── obj-e
│       ├── create-config-from-network
│       ├── multi-queue-sim
│       │   └── run-all-tests.sh
│       └── pretty-print-events
├── lab
│   ├── docs
│   │   └── outline.pdf
│   └── run-all-tests.sh
└── README.md
```

> Please remove `.gitkeep` files after adding to a blank directory  
> Please remove `TODO.md` files after all tasks inside are complete

# 📋 Objectives

## Obj A

- [Workload Generator](/designs/obj-a/workload-generator/README.md): Generator will take as input a distribution, statistics, and interval then produce a synthetic workload
- [Workload Visualizer](/designs/obj-a/workload-visualizer/README.md)

## Obj B

- [Dr. Veras's Example (with added comments)](./examples/obj-b/q_ex-1.py)
  - [Explanation of Dr. Veras's Example](./examples/obj-b/q_ex-1_explained.md)

# 🔗 Links

- [Task Sheet](https://1drv.ms/x/c/470c9d6d25c55391/IQCHiMcoylyeQYELoJuQF3yGAQygDJI3jfT833fP3bGEcKg?e=Dsh32I)
- [Final Report](https://overleaf.cs.ou.edu/project/6a98c77302875a2503a2537a)
- [Presentation](https://sooners-my.sharepoint.com/:p:/g/personal/jace_ou_edu/IQAGarZVcGmxSbaPc47I2IXMAax3SK66qyfHr37jMrBJdx8?e=xniUSi)
- [Download GlobalConnect VPN](https://itsupport.ou.edu/TDClient/30/Unified/KB/Article/128/About-OU-VPN-GlobalProtect-Options-for-your-Connection-Type)

# 👥 Group 2

| Member |     Name     |   Role    |          Email           |   Discord    |
| :----: | :----------: | :-------: | :----------------------: | :----------: |
|   A    |  Joy Mosisa  |  Manager  |  <joy.j.mosia-1@ou.edu>  | stormify.exe |
|   B    | Jace Rausch  | Tech Lead |      <jace@ou.edu>       |    jace.r    |
|   C    |  Jay Singh   |  Member   | <jasakaran.singh@ou.edu> |  jsinggh512  |
|   D    |  Khoi Huyn   |  Member   | <khoi.t.huynh-1@ou.edu>  |  leokhoi22   |
|   E    | Luc Mwizerwa |  Member   |       <luc@ou.edu>       |    .luc23    |
