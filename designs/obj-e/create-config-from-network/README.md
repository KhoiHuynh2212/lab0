# Create Config from Network Design

`create-config-from-network` converts a graph edge list into the queue-configuration CSV consumed by `multi-queue-sim`. It supports plain SNAP-style edge lists and Matrix Market `.mtx` files.

---

# Purpose

Writing hundreds of queue-system rows by hand is error-prone. A network file already says which nodes connect, so this tool maps each node to a queue system and each graph edge to a queue-system edge.

```txt
┌──────────────────┐
│ network file     │
│ u v edge pairs   │
└────────┬─────────┘
         ↓
┌──────────────────────────┐
│ create-config-from-network│
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ multi-queue config CSV   │
└──────────────────────────┘
```

---

# Supported Network Inputs

## SNAP-style edge list

Comments begin with `#`. Each data line starts with two integer node IDs:

```txt
# source target
1 2
2 3
1 3
```

Commas are also accepted as separators because commas are changed to spaces before splitting.

## Matrix Market

Lines beginning with `%` are comments. The matrix-size line after the header is skipped, then each remaining row begins with two integer node IDs and may contain an extra weight.

```txt
%%MatrixMarket matrix coordinate real general
% comment
3 3 2
1 2 1.0
2 3 1.0
```

Self-loops and duplicate directed edges are removed before the configuration is written.

---

# Graph Construction

```txt
READ every edge pair in file order
COLLECT every distinct node id
SORT node ids

IF --max-nodes N was supplied
    KEEP only the N lowest node ids
END IF

FOR each edge u -> v
    IGNORE self loops
    IGNORE edges whose endpoint was removed
    IF --acyclic and u > v
        IGNORE the edge
    END IF
    IGNORE duplicate u -> v edges
    ADD v to u's outgoing list
    ADD u to v's incoming list
END FOR
```

`--acyclic` is useful for doubled undirected SNAP data. Keeping only edges where `u < v` creates a directed acyclic orientation, which prevents a job from circulating forever solely because both directions of every undirected edge were present.

---

# Configuration Mapping

Every retained node `n` becomes queue `q<n>`.

The output header is:

```csv
queue_sys_id,max_queue_size,num_servers,server_function,server_function_rate,source,sink,edges_in,edges_out
```

All rows use the command-line defaults or overrides for queue size, servers, server function, and function rate.

Source and sink assignment follows graph structure:

- node with no incoming edges → source queue,
- node with no outgoing edges → sink queue.

Source filenames rotate through `in0.wl` to `in<S-1>.wl`. Sink filenames rotate through `out0.wl` to `out<K-1>.wl`.

---

# Pseudocode

```txt
PARSE options
READ edge list
FAIL if there are no edges

BUILD cleaned graph
OPEN output CSV
WRITE header

entry_counter = 0
exit_counter = 0

FOR node in sorted retained nodes
    source = empty
    sink = empty

    IF node has no incoming edges
        source = "in" + (entry_counter mod source_count) + ".wl"
        entry_counter += 1
    END IF

    IF node has no outgoing edges
        sink = "out" + (exit_counter mod sink_count) + ".wl"
        exit_counter += 1
    END IF

    WRITE one queue row
        queue id = q<node>
        edges in = semicolon-joined predecessor queue ids
        edges out = semicolon-joined successor queue ids
END FOR
```

---

# Worked Example

Input network:

```txt
1 2
1 3
2 3
```

Using default queue settings produces the same graph shape:

```txt
q1 ──→ q2 ──→ q3
 └──────────→ q3
```

`q1` has no incoming edges, so it gets `in0.wl`. `q3` has no outgoing edges, so it gets `out0.wl`.

Conceptual output:

```csv
queue_sys_id,max_queue_size,num_servers,server_function,server_function_rate,source,sink,edges_in,edges_out
q1,10,1,Sub-10*,10,in0.wl,,,q2;q3
q2,10,1,Sub-10*,10,,,q1,q3
q3,10,1,Sub-10*,10,,out0.wl,q1;q2,
```

---

# Configuration Controls

|       Option       |  Default  | Meaning                                                |
| :----------------: | :-------: | :----------------------------------------------------- |
| `--max-queue-size` |   `10`    | Waiting slots per queue system.                        |
|    `--servers`     |    `1`    | Servers per queue system.                              |
|  `--server-func`   | `Sub-10*` | Server function text written to every row.             |
|      `--rate`      |   `10`    | Function-step duration written to every row.           |
|    `--sources`     |    `1`    | Number of source workload filenames to rotate through. |
|     `--sinks`      |    `1`    | Number of sink workload filenames to rotate through.   |
|   `--max-nodes`    |   none    | Keep only the N lowest node IDs.                       |
|    `--acyclic`     |    off    | Keep only directed edges where `u < v`.                |

---

# Edge Cases

- Reject a file with no usable edges.
- Reject non-integer node IDs.
- Reject malformed lines with fewer than two fields.
- Remove self-loops and duplicate directed edges.
- Positive-only options reject zero where a count must be at least one.
- `max_queue_size` allows zero because a queue may intentionally have no waiting capacity.
- A graph with cycles remains cyclic unless `--acyclic` happens to remove the reverse edges that cause them.

---

# Resources and Connections

- [Implementation and exact CLI](/implementation/obj-e/create-config-from-network/README.md)
- [Multi-queue simulator design](/designs/obj-e/multi-queue-sim/README.md)
- [AS-733 network example](/examples/obj-e/as-733/README.md)
- [Matrix Market format](https://math.nist.gov/MatrixMarket/formats.html)
- [SNAP network data](https://snap.stanford.edu/data/)
