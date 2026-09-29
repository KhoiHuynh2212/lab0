#!/usr/bin/env python3

import argparse
import csv
import sys

FIELDNAMES = [
    "queue_sys_id",
    "max_queue_size",
    "num_servers",
    "server_function",
    "server_function_rate",
    "source",
    "sink",
    "edges_in",
    "edges_out",
]


def read_edges(path):
    """Parse an edge list. Handles SNAP text ('# comments', 'u v') and
    Matrix Market ('%' comments, a 'rows cols nnz' size line, then 'u v [w]').
    Returns a list of (u, v) integer pairs in file order."""
    edges = []
    is_mtx = path.lower().endswith(".mtx")
    size_line_pending = is_mtx

    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line_number, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            if line.startswith("%%MatrixMarket"):
                is_mtx = True
                size_line_pending = True
                continue
            if line[0] in "#%":
                continue

            parts = line.replace(",", " ").split()
            if size_line_pending:
                # Matrix Market size header: rows cols nnz -- not an edge.
                size_line_pending = False
                continue
            if len(parts) < 2:
                raise ValueError(f"{path}:{line_number}: expected 'u v [w]', got: {line!r}")
            try:
                u, v = int(parts[0]), int(parts[1])
            except ValueError:
                raise ValueError(f"{path}:{line_number}: non-integer node id in: {line!r}")
            edges.append((u, v))
    return edges


def build_graph(edges, max_nodes=None, acyclic=False):
    """Return (sorted_node_ids, out_adj, in_adj) with duplicates and
    self-loops removed, optionally truncated to the first max_nodes ids."""
    nodes = sorted({n for e in edges for n in e})
    if max_nodes is not None:
        nodes = nodes[:max_nodes]
    keep = set(nodes)

    out_adj = {n: [] for n in nodes}
    in_adj = {n: [] for n in nodes}
    seen = set()

    for u, v in edges:
        if u == v or u not in keep or v not in keep:
            continue
        if acyclic and u > v:
            # SNAP "undirected" files list every edge twice; keeping only
            # low->high yields a DAG so jobs cannot circulate forever.
            continue
        if (u, v) in seen:
            continue
        seen.add((u, v))
        out_adj[u].append(v)
        in_adj[v].append(u)
    return nodes, out_adj, in_adj


def write_config(path, nodes, out_adj, in_adj, args):
    entry_counter = 0
    exit_counter = 0
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        writer.writeheader()
        for n in nodes:
            source = ""
            sink = ""
            if not in_adj[n]:
                source = f"in{entry_counter % args.sources}.wl"
                entry_counter += 1
            if not out_adj[n]:
                sink = f"out{exit_counter % args.sinks}.wl"
                exit_counter += 1
            writer.writerow({
                "queue_sys_id": f"q{n}",
                "max_queue_size": args.max_queue_size,
                "num_servers": args.servers,
                "server_function": args.server_func,
                "server_function_rate": args.rate,
                "source": source,
                "sink": sink,
                "edges_in": ";".join(f"q{m}" for m in in_adj[n]),
                "edges_out": ";".join(f"q{m}" for m in out_adj[n]),
            })
    return entry_counter, exit_counter


def positive_int(text):
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return value


def nonnegative_int(text):
    value = int(text)
    if value < 0:
        raise argparse.ArgumentTypeError("must be >= 0")
    return value


def main(argv=None):
    p = argparse.ArgumentParser(
        description="Create a multi-queue-sim configuration CSV from a network file.")
    p.add_argument("network_file", help="SNAP edge list or Matrix Market (.mtx) file")
    p.add_argument("output_csv", help="queue configuration CSV to write")
    p.add_argument("--max-queue-size", type=nonnegative_int, default=10)
    p.add_argument("--servers", type=positive_int, default=1)
    p.add_argument("--server-func", default="Sub-10*",
                   help='server function chain, e.g. "Sub-10*" or "Sub-5,Sub-10"')
    p.add_argument("--rate", type=positive_int, default=10,
                   help="server function rate (time steps per function step)")
    p.add_argument("--sources", type=positive_int, default=1,
                   help="number of source workload files (in0.wl ...)")
    p.add_argument("--sinks", type=positive_int, default=1,
                   help="number of sink workload files (out0.wl ...)")
    p.add_argument("--max-nodes", type=positive_int, default=None,
                   help="keep only the N lowest node ids (and edges among them)")
    p.add_argument("--acyclic", action="store_true",
                   help="keep only edges u->v with u<v (turns doubled undirected "
                        "edges into a DAG so jobs cannot loop forever)")
    args = p.parse_args(argv)

    try:
        edges = read_edges(args.network_file)
    except (OSError, ValueError) as error:
        print(f"create-config-from-network: {error}", file=sys.stderr)
        return 1
    if not edges:
        print("create-config-from-network: no edges found in input", file=sys.stderr)
        return 1

    nodes, out_adj, in_adj = build_graph(edges, args.max_nodes, args.acyclic)
    entries, exits = write_config(args.output_csv, nodes, out_adj, in_adj, args)

    edge_count = sum(len(v) for v in out_adj.values())
    print(f"wrote {args.output_csv}: {len(nodes)} queue systems, {edge_count} edges, "
          f"{entries} sourced, {exits} sinked")
    return 0


if __name__ == "__main__":
    sys.exit(main())