#!/usr/bin/env python3
"""Run the car-wash manifest with unchanged Objective B and verify raw accounting."""

import argparse
import ast
import csv
from collections import Counter
from datetime import datetime, timezone
import os
from pathlib import Path
import platform
import re
import shutil
import sys
import time

from prepare_workloads import DEMANDS, HORIZONS, MANIFEST_FIELDS, write_csv
from validate_carwash import REPO, SIMPLE, TASKS, command, read_csv, relative, require, save_json, sha256


def load_manifest(path):
    require(path.is_relative_to(SIMPLE), "Manifest must be inside Objective D.")
    rows = read_csv(path)
    require(bool(rows), "Manifest is empty")
    require(all(set(r) == set(MANIFEST_FIELDS) and None not in r.values() for r in rows),
            "Unexpected manifest columns")
    by_id = {r["run_id"]: r for r in rows}
    require(len(by_id) == len(rows), "Duplicate run IDs")
    inputs = {}
    for row in rows:
        require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", row["run_id"]), "Unsafe run ID")
        require(row["demand"] in DEMANDS and float(row["arrival_lambda"]) == float(DEMANDS[row["demand"]]),
                "Unexpected demand parameter")
        require(int(row["arrival_horizon"]) in HORIZONS and 0 <= int(row["seed"]) < 2**64,
                "Invalid horizon or seed")
        require((row["server_function"], row["server_function_rate"], row["stop_mode"], row["time_unit_seconds"])
                == ("Sub-1*", "1", "drain", "10"), "Unsupported car-wash service or stop settings")
        workload = (path.parent / row["workload_file"]).resolve()
        require(workload.is_relative_to(SIMPLE) and workload.is_file(), "Invalid workload path")
        require(sha256(workload) == row["workload_sha256"], f"Input hash mismatch: {workload}")
        if workload not in inputs:
            source = read_csv(workload)
            require(bool(source), "This experiment runner expects a nonempty car-wash workload")
            require(all(set(r) == {"Job Id", "arrival time", "Tasks"} and None not in r.values() for r in source),
                    "Malformed source workload")
            require(len({r["Job Id"] for r in source}) == len(source)
                    and all(r["Job Id"] and not any(c.isspace() for c in r["Job Id"]) for r in source),
                    "Invalid or duplicate input IDs")
            require(all(ast.literal_eval(r["Tasks"]) == TASKS for r in source), "Unexpected wash stages")
            inputs[workload] = source
        times = [int(r["arrival time"]) for r in inputs[workload]]
        require(times == sorted(times) and all(0 <= t < int(row["arrival_horizon"]) for t in times),
                "Source arrivals outside horizon or out of order")
        baseline = by_id.get(row["baseline_run_id"])
        require(baseline is not None and baseline["family"] == "baseline", "Missing baseline")
        for field in ["workload_id", "workload_file", "workload_sha256", "arrival_lambda",
                      "arrival_horizon", "seed", "demand"]:
            require(row[field] == baseline[field], f"Unpaired comparison: {field}")
        size, bays, pct = int(row["queue_size"]), int(row["num_servers"]), int(row["resource_change_percent"])
        if row["family"] == "baseline":
            require((size, bays, pct, row["changed_resource"]) == (20, 1, 0, "none")
                    and row["run_id"] == row["baseline_run_id"], "Invalid baseline")
        elif row["family"] == "waiting-space":
            require(bays == 1 and pct in {-30, -20, -10, 25, 50, 100}
                    and (size - 20) * 5 == pct and row["changed_resource"] == "waiting_spaces"
                    and int(row["arrival_horizon"]) == HORIZONS[-1] and row["demand"] in {"near", "over"},
                    "Invalid waiting-space variant")
        elif row["family"] == "additional-bay":
            require((size, bays, pct, row["changed_resource"]) == (20, 2, 100, "bays")
                    and int(row["arrival_horizon"]) == HORIZONS[-1] and row["demand"] in {"near", "over"},
                    "Invalid bay variant")
        else:
            raise RuntimeError(f"Unknown experiment family: {row['family']}")
    return rows, inputs


def build_simulator(build, env):
    compiler = shutil.which(os.environ.get("CXX", "g++"))
    require(compiler is not None, "Put g++ on PATH or set CXX.")
    component = REPO / "implementation/obj-b/simple-queue-sim"
    hashes = {relative(p): sha256(p) for p in sorted(component.rglob("*")) if p.suffix in {".cpp", ".hpp"}}
    executable = build / ("simple-queue-sim.exe" if os.name == "nt" else "simple-queue-sim")
    args = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-O2",
            "-I" + relative(component / "include"), relative(component / "src/main.cpp"),
            relative(component / "src/queue_sim.cpp"), "-o", relative(executable)]
    print("Building unchanged Objective B simulator into D's local build directory", flush=True)
    report = {"compiler": command([compiler, "--version"], REPO, env),
              "build_command": command(args, REPO, env), "source_sha256": hashes,
              "executable_sha256": sha256(executable)}
    return executable, report


def verify_run(folder, row, source):
    """Check consistency of source, events, sink, and every measurement row; no C replacement."""
    expected = {r["Job Id"]: int(r["arrival time"]) for r in source}
    arrived, rejected, starts, exits, departed = {}, {}, {}, {}, {}
    active = {}
    start_order = []
    task_starts, task_stops = Counter(), Counter()
    arrivals_at, errors_at, starts_at, exits_at, departures_at = (Counter() for _ in range(5))
    last_time, event_rows = -1, 0
    bays, queue_size, horizon = int(row["num_servers"]), int(row["queue_size"]), int(row["arrival_horizon"])
    with (folder / "job.log").open(encoding="utf-8") as stream:
        for line in stream:
            parts = line.split(maxsplit=5)
            require(len(parts) == 6 and parts[2:4] == ["IN", "q0"], "Malformed event")
            timestamp, job, _, _, action, detail = parts
            t = int(timestamp)
            detail = detail.strip()
            require(t >= last_time and job in expected, "Out-of-order event or unknown car")
            last_time, event_rows = t, event_rows + 1
            if action == "ARRIVE-VIA":
                require(job not in arrived and t == expected[job], "Wrong/duplicate arrival")
                arrived[job] = t
                arrivals_at[t] += 1
            else:
                require(job in arrived, "Event before arrival")
                if action == "ERROR":
                    require(detail == "QUEUE-FULL" and job not in rejected and job not in starts
                            and t == expected[job], "Unexpected or duplicate rejection")
                    rejected[job] = t
                    errors_at[t] += 1
                elif action == "ENTERS-SERVER":
                    server = int(detail)
                    require(job not in starts and job not in rejected and 0 <= server < bays
                            and server not in active and t >= expected[job], "Invalid service start")
                    starts[job] = (t, server)
                    active[server] = job
                    start_order.append(job)
                    starts_at[t] += 1
                elif action == "EXITS-SERVER":
                    server = int(detail)
                    require(job in starts and job not in exits and active.get(server) == job
                            and starts[job] == (t - 34, server), "Wrong bay, duration, or duplicate service exit")
                    exits[job] = t
                    del active[server]
                    exits_at[t] += 1
                elif action == "DEPART-VIA":
                    require(detail == "completed.csv" and job not in departed and exits.get(job) == t,
                            "Invalid sink departure")
                    departed[job] = t
                    departures_at[t] += 1
                elif action in {"TASK-START", "TASK-STOP"}:
                    require(job in starts and job not in exits, "Task event outside service")
                    (task_starts if action == "TASK-START" else task_stops)[job] += 1
                else:
                    raise RuntimeError(f"Unexpected event action: {action}")
    accepted_ids = [r["Job Id"] for r in source if r["Job Id"] not in rejected]
    require(arrived == expected, "Missing arrivals")
    require(start_order == accepted_ids, "FIFO order mismatch")
    require(set(departed) == set(accepted_ids) == set(starts) == set(exits) and not active,
            "Missing, duplicated, or unfinished admitted cars")
    require(all(task_starts[j] == task_stops[j] == 34 for j in accepted_ids), "Missing operation events")
    sink = read_csv(folder / "completed.csv")
    require(len(sink) == len(departed) and len({r["Job Id"] for r in sink}) == len(sink), "Sink count mismatch")
    require(all(r["Job Id"] in departed and int(r["arrival time"]) == departed[r["Job Id"]]
                and ast.literal_eval(r["Tasks"]) == [] for r in sink), "Sink/events disagree")
    offered, completed = len(source), len(sink)
    require(offered == completed + len(rejected), "Cars are unaccounted for after draining")
    arrived_total = errors_total = started_total = exited_total = completed_total = 0
    maximum_queue = maximum_busy = 0
    cutoff_queue = cutoff_busy = 0
    metric_rows, final_state = 0, None
    with (folder / "qmet.csv").open(encoding="utf-8", newline="") as stream:
        for metric_rows, m in enumerate(csv.DictReader(stream), start=1):
            t = int(m["time-step"])
            q, busy, errors = int(m["num-jobs-in-queue"]), int(m["num-jobs-in-service"]), int(m["num-of-errors"])
            require(t == metric_rows - 1 and m["queue-sys-id"] == "q0", "Missing metric timestep or wrong queue")
            require(0 <= q <= queue_size and 0 <= busy <= bays, "Capacity bounds violated")
            arrived_total += arrivals_at[t]
            errors_total += errors_at[t]
            started_total += starts_at[t]
            exited_total += exits_at[t]
            completed_total += departures_at[t]
            require(errors == errors_total and busy == started_total - exited_total
                    and q + busy == arrived_total - errors_total - completed_total,
                    "Event/measurement conservation mismatch")
            maximum_queue, maximum_busy = max(maximum_queue, q), max(maximum_busy, busy)
            if t == horizon - 1:
                cutoff_queue, cutoff_busy = q, busy
            final_state = (q, busy)
    last_departure = max(departed.values(), default=0)
    require(metric_rows > 0 and metric_rows - 1 == max(last_time, max(expected.values()))
            and final_state == (0, 0), "Unexpected final time or nonempty final state")
    before_cutoff = sum(t < horizon for t in departed.values())
    unfinished = completed - before_cutoff
    require(unfinished == cutoff_queue + cutoff_busy, "Cutoff accounting mismatch")
    summary = {key: row[key] for key in ["run_id", "family", "workload_id", "demand", "seed",
                                        "arrival_horizon", "queue_size", "num_servers"]}
    summary.update({"status": "passed", "offered": offered, "accepted": completed,
                    "completed": completed, "rejected": len(rejected),
                    "rejection_fraction": f"{len(rejected) / offered:.8f}",
                    "completed_before_cutoff": before_cutoff, "unfinished_at_cutoff": unfinished,
                    "waiting_at_cutoff": cutoff_queue, "busy_at_cutoff": cutoff_busy,
                    "last_departure": last_departure, "maximum_waiting": maximum_queue,
                    "maximum_busy": maximum_busy, "metric_rows": metric_rows, "event_rows": event_rows})
    verification = {"summary": summary, "cutoff_definition": "After timestep H-1, before events at H; departures at H count as unfinished at this boundary.",
                    "checks": ["input IDs and arrival timestamps", "each car completed or rejected exactly once",
                               "FIFO admission to service", "34 service steps per accepted car",
                               "one active car per bay", "34 task-start and task-stop operations per accepted car",
                               "sink/event agreement and empty completed tasks", "queue/bay capacity bounds",
                               "cumulative errors equal rejection events at every timestep",
                               "event/metric conservation at every timestep", "empty final system",
                               "arrival-cutoff accounting"]}
    return summary, verification


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=SIMPLE / "experiments.csv")
    parser.add_argument("--output-root", type=Path, default=SIMPLE,
                        help="D directory to receive runs/experiments and results/tables/run-summary.csv")
    parser.add_argument("--run-ids", nargs="+", help="Optional subset for a pilot; default: every manifest row")
    args = parser.parse_args()
    manifest_path, output = args.manifest.resolve(), args.output_root.resolve()
    require(output.is_relative_to(SIMPLE), "Output must stay inside Objective D.")
    run_root, summary_path = output / "runs/experiments", output / "results/tables/run-summary.csv"
    require(not run_root.exists() and not summary_path.exists(), "Refusing to overwrite experiment evidence; choose a new --output-root.")
    rows, inputs = load_manifest(manifest_path)
    ids = {r["run_id"] for r in rows}
    if args.run_ids:
        require(len(set(args.run_ids)) == len(args.run_ids) and set(args.run_ids) <= ids, "Unknown or duplicate requested run IDs")
    selected = [r for r in rows if not args.run_ids or r["run_id"] in args.run_ids]
    build = SIMPLE / ".build/experiments"
    build.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    executable, build_report = build_simulator(build, env)
    run_root.mkdir(parents=True)
    report = {"layout_version": 2, "status": "running", "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
              "platform": platform.platform(), "python": sys.version,
              "source_commit": command(["git", "rev-parse", "HEAD"], REPO, env)["stdout"].strip(),
              "shared_source_changes": command(["git", "status", "--porcelain", "--", "implementation/obj-b"], REPO, env)["stdout"],
              "build": build_report, "manifest": relative(manifest_path), "manifest_sha256": sha256(manifest_path),
              "script_sha256": {relative(p): sha256(p) for p in [Path(__file__), SIMPLE / "scripts/prepare_workloads.py", SIMPLE / "scripts/validate_carwash.py"]},
              "selected_run_ids": [r["run_id"] for r in selected], "completed_run_ids": [], "runs": {},
              "limitations": ["Raw accounting and correctness checks only; Objective C derived analysis remains pending.",
                              "Native execution; no new sanitizer, Valgrind, or coverage claim."]}
    report_path = run_root / "execution.json"
    save_json(report_path, report)
    template = read_csv(SIMPLE / "configs/carwash-template.csv")
    require(len(template) == 1, "Invalid configuration template")
    summaries = []
    for index, row in enumerate(selected, start=1):
        folder = run_root / row["run_id"]
        folder.mkdir()
        record = {"metadata": {"run": row}, "verification": {"status": "running"}}
        report["runs"][row["run_id"]] = record
        try:
            workload = (manifest_path.parent / row["workload_file"]).resolve()
            require(sha256(workload) == row["workload_sha256"], "Input changed since preflight")
            config = {**template[0], "Max Queue Size": row["queue_size"], "# of servers": row["num_servers"],
                      "Server Function": row["server_function"], "Server Function Rate": row["server_function_rate"],
                      "source": relative(workload, folder), "sink": "completed.csv"}
            write_csv(folder / "config.csv", list(config), [config])
            argv = [executable, "config.csv", "--metrics", "qmet.csv", "--events", "job.log"]
            start = time.perf_counter()
            result = command(argv, folder, env, expected_success=False)
            elapsed = time.perf_counter() - start
            metadata = {"run": row, "source_commit": report["source_commit"], "command": result,
                        "workload": relative(workload), "workload_sha256": sha256(workload),
                        "config_sha256": sha256(folder / "config.csv"), "execution_report": "../execution.json",
                        "simulator_sha256": build_report["executable_sha256"], "execution_wall_seconds": elapsed,
                        "recorded_at_utc": datetime.now(timezone.utc).isoformat()}
            record["metadata"] = metadata
            save_json(report_path, report)
            require(result["exit_code"] == 0, f"B failed; see runs[{row['run_id']!r}].metadata.command.stderr in {report_path}")
            summary, verification = verify_run(folder, row, inputs[workload])
            require(sha256(workload) == row["workload_sha256"], "Input changed during execution")
            metadata["output_sha256"] = {name: sha256(folder / name) for name in ["completed.csv", "qmet.csv", "job.log"]}
            record["verification"] = verification
            summaries.append(summary)
            write_csv(summary_path, list(summary), summaries)
            report["completed_run_ids"].append(row["run_id"])
            save_json(report_path, report)
            print(f"[{index}/{len(selected)}] {row['run_id']}: {summary['completed']}/{summary['offered']} completed, "
                  f"{summary['rejected']} rejected, {summary['unfinished_at_cutoff']} unfinished at cutoff; verified", flush=True)
        except Exception as exc:
            record["verification"] = {"status": "failed", "error": str(exc)}
            report.update({"status": "failed", "failed_run_id": row["run_id"], "error": str(exc)})
            save_json(report_path, report)
            raise
    unchanged = all(sha256(REPO / p) == h for p, h in build_report["source_sha256"].items())
    report.update({"status": "passed" if unchanged else "failed", "shared_source_hashes_unchanged": unchanged,
                   "completed_at_utc": datetime.now(timezone.utc).isoformat(), "summary_sha256": sha256(summary_path)})
    save_json(report_path, report)
    require(unchanged, "Shared simulator source changed during execution")
    print(f"Completed and verified {len(summaries)} experiments. Summary: {summary_path}")
    print("Objective C analysis has not been performed.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, SyntaxError) as exc:
        print(f"Experiment execution failed: {exc}", file=sys.stderr)
        sys.exit(1)
