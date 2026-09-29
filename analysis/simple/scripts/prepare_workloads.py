#!/usr/bin/env python3
"""Use Objective A to prepare car-wash workloads, input plots, and an experiment manifest."""

import argparse
import ast
import csv
from datetime import datetime, timezone
import os
from pathlib import Path
import platform
import shutil
import sys

from validate_carwash import REPO, SIMPLE, TASKS, command, read_csv, relative, require, save_json, sha256


DEMANDS = {"light": "0.010", "near": "0.025", "over": "0.040"}
HORIZONS = (2880, 28800)
QUEUE_VARIANTS = {25: 25, 30: 50, 40: 100, 18: -10, 16: -20, 14: -30}
WORKLOAD_FIELDS = ["Job Id", "arrival time", "Tasks"]
MANIFEST_FIELDS = [
    "run_id", "family", "workload_id", "workload_file", "workload_sha256", "demand",
    "arrival_lambda", "arrival_horizon", "seed", "queue_size", "num_servers",
    "server_function", "server_function_rate", "baseline_run_id", "changed_resource",
    "resource_change_percent", "stop_mode", "time_unit_seconds",
]


def write_csv(path, fields, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def build_tools(build, env):
    """Compile A into D; never invoke Makefiles that clean shared directories."""
    compiler = shutil.which(os.environ.get("CXX", "g++"))
    require(compiler is not None, "Put g++ on PATH or select a compatible compiler with CXX.")
    report = {"compiler": command([compiler, "--version"], REPO, env), "build_commands": [],
              "source_sha256": {}, "executable_sha256": {}}
    tools = {}
    for name, stem in [("workload-generator", "workload_generator"),
                       ("workload-visualizer", "workload_visualizer")]:
        source = REPO / "implementation/obj-a" / name
        binary = build / (name + (".exe" if os.name == "nt" else ""))
        for file in sorted(source.rglob("*")):
            if file.suffix in {".cpp", ".hpp"}:
                report["source_sha256"][relative(file)] = sha256(file)
        args = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-O2",
                "-I" + relative(source / "include"), relative(source / "src/main.cpp"),
                relative(source / f"src/{stem}.cpp"), "-o", relative(binary)]
        print(f"Building unchanged Objective A {name}", flush=True)
        report["build_commands"].append(command(args, REPO, env))
        report["executable_sha256"][name] = sha256(binary)
        tools[name] = binary
    return tools, report


def check_arrivals(rows, horizon):
    require(all(set(row) == set(WORKLOAD_FIELDS) and None not in row.values() for row in rows),
            "Malformed workload columns")
    ids = [row["Job Id"] for row in rows]
    arrivals = [int(row["arrival time"]) for row in rows]
    require(len(ids) == len(set(ids)) and all(ids), "Empty or duplicate job IDs")
    require(arrivals == sorted(arrivals), "Arrivals are not in chronological order")
    require(all(0 <= time < horizon for time in arrivals), "Arrival outside inclusive generator interval")
    return list(zip(ids, arrivals))


def prepare_one(generator, output, build, demand, horizon, seed, env):
    workload_id = f"{demand}-h{horizon}-seed{seed}"
    folder = output / "workloads/generated" / workload_id
    folder.mkdir(parents=True)
    raw_path = folder / "generator-output.csv"
    final_path = folder / "workload.csv"
    params = ["--interval", f"0,{horizon - 1}", "--distribution", "poisson",
              "--lambda", DEMANDS[demand], "--tasks-per-job", "1", "--task-work", "fixed",
              "--fixed-task-work", "1", "--seed", str(seed), "--job-prefix", "j"]
    first = command([generator, "--out", relative(raw_path), *params], REPO, env)
    # Repeat with the same environment/parameters; only the output path changes.
    repeat_path = build / "repeat-workloads" / f"{workload_id}.csv"
    second = command([generator, "--out", relative(repeat_path), *params], REPO, env)
    require(sha256(raw_path) == sha256(repeat_path), f"Seeded generation changed for {workload_id}")
    original = read_csv(raw_path)
    pairs = check_arrivals(original, horizon)
    require(all(ast.literal_eval(row["Tasks"]) == ["0: W 1"] for row in original),
            "Unexpected generator placeholder tasks")
    transformed = [{**row, "Tasks": str(TASKS)} for row in original]
    write_csv(final_path, WORKLOAD_FIELDS, transformed)
    reloaded = read_csv(final_path)
    require(check_arrivals(reloaded, horizon) == pairs, "Transformation changed IDs or arrival times")
    require(all(ast.literal_eval(row["Tasks"]) == TASKS for row in reloaded),
            "Transformation did not assign the five wash stages")
    require(sum(int(task.split("W ")[1]) for task in TASKS) == 34, "Invalid total service work")
    count = len(reloaded)
    summary = {
        "workload_id": workload_id, "demand": demand, "arrival_lambda": DEMANDS[demand],
        "arrival_horizon": horizon, "seed": seed, "job_count": count,
        "expected_job_count": f"{float(DEMANDS[demand]) * horizon:.2f}",
        "observed_arrivals_per_step": f"{count / horizon:.8f}",
        "observed_arrivals_per_hour": f"{count * 360 / horizon:.4f}",
        "first_arrival": pairs[0][1] if pairs else "",
        "last_arrival": pairs[-1][1] if pairs else "",
        "workload_sha256": sha256(final_path),
    }
    save_json(folder / "metadata.json", {
        **summary, "distribution": "poisson counts per integer timestep",
        "interval_inclusive": [0, horizon - 1], "time_unit_seconds": 10,
        "tasks": TASKS, "total_work_per_car": 34,
        "generator_output_sha256": sha256(raw_path), "generator_command": first,
        "repeat_command": second, "provenance": "../../preparation.json",
        "verification": {"same_seed_byte_identical": True, "arrival_ids_preserved": True,
                         "unique_ids": True, "arrivals_in_bounds_and_sorted": True,
                         "fixed_five_stage_tasks": True},
    })
    print(f"Prepared {workload_id}: {count} cars; repeated generation and task mapping verified", flush=True)
    return summary


def make_manifest(summaries):
    rows = []
    for info in summaries:
        wid = info["workload_id"]
        baseline = f"baseline-{wid}"
        common = {key: info[key] for key in ["workload_id", "workload_sha256", "demand",
                                            "arrival_lambda", "arrival_horizon", "seed"]}
        common.update({"workload_file": f"workloads/generated/{wid}/workload.csv",
                       "server_function": "Sub-1*", "server_function_rate": 1,
                       "baseline_run_id": baseline, "stop_mode": "drain", "time_unit_seconds": 10})
        rows.append({**common, "run_id": baseline, "family": "baseline", "queue_size": 20,
                     "num_servers": 1, "changed_resource": "none", "resource_change_percent": 0})
        if info["arrival_horizon"] == HORIZONS[-1] and info["demand"] in {"near", "over"}:
            for size, percentage in QUEUE_VARIANTS.items():
                rows.append({**common, "run_id": f"queue{size}-{wid}", "family": "waiting-space",
                             "queue_size": size, "num_servers": 1, "changed_resource": "waiting_spaces",
                             "resource_change_percent": percentage})
            rows.append({**common, "run_id": f"bays2-{wid}", "family": "additional-bay",
                         "queue_size": 20, "num_servers": 2, "changed_resource": "bays",
                         "resource_change_percent": 100})
    by_id = {row["run_id"]: row for row in rows}
    require(len(by_id) == len(rows), "Duplicate run ID")
    for row in rows:
        baseline = by_id[row["baseline_run_id"]]
        require(baseline["family"] == "baseline" and row["workload_sha256"] == baseline["workload_sha256"]
                and row["workload_file"] == baseline["workload_file"], "Comparison has different arrivals")
    return rows


def create_plots(visualizer, output, summaries, env, seed):
    """Show arrival counts at matched axes, plus the fixed-five-task mapping."""
    folder = output / "results/figures/workloads"
    folder.mkdir(parents=True)
    selected = [row for row in summaries if row["seed"] == seed and row["arrival_horizon"] == HORIZONS[-1]]
    bins, horizon = 20, HORIZONS[-1]
    maximum = 0
    for info in selected:
        counts = [0] * bins
        for row in read_csv(output / f"workloads/generated/{info['workload_id']}/workload.csv"):
            counts[int(row["arrival time"]) * bins // horizon] += 1
        maximum = max(maximum, max(counts))
    ymax = max(10, ((maximum + 9) // 10) * 10 + 10)
    plots = []
    for info in selected:
        wid = info["workload_id"]
        source = output / f"workloads/generated/{wid}/workload.csv"
        image = folder / f"{wid}-arrivals.png"
        args = [visualizer, relative(source), "--hist=arrival time", f"--bins={bins}",
                f"--xlim=0,{horizon}", f"--ylim=0,{ymax}", "--width=800", "--height=500",
                f"--title={info['demand'].upper()} ARRIVALS - SEED {seed} - 10 SEC/STEP", "--out=" + relative(image)]
        invocation = command(args, REPO, env)
        plots.append({"file": relative(image, output), "workload_id": wid, "command": invocation,
                      "sha256": sha256(image), "bin_width_steps": horizon // bins})
        if info["demand"] == "near":
            image = folder / f"{wid}-tasks.png"
            args = [visualizer, relative(source), "--scat=arrival time,len(Tasks)",
                    f"--xlim=0,{horizon}", "--ylim=0,6", "--width=800", "--height=500",
                    f"--title=FIVE TASKS PER CAR - SEED {seed} - 10 SEC/STEP", "--out=" + relative(image)]
            invocation = command(args, REPO, env)
            plots.append({"file": relative(image, output), "workload_id": wid, "command": invocation,
                          "sha256": sha256(image)})
    save_json(folder / "metadata.json", {"plot_seed": seed, "histogram_shared_ymax": ymax,
                                         "histogram_bins": bins, "plots": plots})
    return plots


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=SIMPLE,
                        help="D directory containing the manifest/workloads/results; owned outputs must not exist")
    parser.add_argument("--seeds", nargs="+", type=int, default=[42, 43, 44],
                        help="Distinct unsigned seeds; default: 42 43 44. Use 42 alone for a pilot.")
    args = parser.parse_args()
    output = args.output_root.resolve()
    require(output.is_relative_to(SIMPLE), "Keep generated output within analysis/simple.")
    require(len(set(args.seeds)) == len(args.seeds) and all(0 <= seed < 2**64 for seed in args.seeds),
            "Seeds must be distinct integers from 0 through 2**64-1.")
    owned = ["workloads/generated", "workloads/preparation.json", "experiments.csv",
             "results/tables/workloads.csv", "results/figures/workloads"]
    conflicts = [str(output / path) for path in owned if (output / path).exists()]
    require(not conflicts, "Refusing to overwrite prepared artifacts: " + ", ".join(conflicts)
            + ". Choose a new --output-root.")
    build = SIMPLE / ".build/workload-preparation"
    build.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    tools, build_report = build_tools(build, env)
    output.mkdir(parents=True, exist_ok=True)
    summaries = [prepare_one(tools["workload-generator"], output, build, demand, horizon, seed, env)
                 for demand in DEMANDS for horizon in HORIZONS for seed in args.seeds]
    manifest = make_manifest(summaries)
    require(len(manifest) == 20 * len(args.seeds), "Incorrect experiment count")
    write_csv(output / "experiments.csv", MANIFEST_FIELDS, manifest)
    write_csv(output / "results/tables/workloads.csv", list(summaries[0]), summaries)
    plots = create_plots(tools["workload-visualizer"], output, summaries, env, args.seeds[0])
    require(all(sha256(REPO / path) == digest for path, digest in build_report["source_sha256"].items()),
            "Objective A source changed during preparation")
    save_json(output / "workloads/preparation.json", {
        "schema_version": 1, "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(), "python": sys.version,
        "source_commit": command(["git", "rev-parse", "HEAD"], REPO, env)["stdout"].strip(),
        "shared_source_changes": command(["git", "status", "--porcelain", "--", "implementation/obj-a"],
                                         REPO, env)["stdout"],
        "script_sha256": sha256(Path(__file__)),
        "helper_script_sha256": sha256(SIMPLE / "scripts/validate_carwash.py"),
        "build": build_report, "seeds": args.seeds,
        "horizons": list(HORIZONS), "demands": DEMANDS, "workload_count": len(summaries),
        "experiment_count": len(manifest), "plot_count": len(plots),
        "manifest_sha256": sha256(output / "experiments.csv"),
        "summary_sha256": sha256(output / "results/tables/workloads.csv"),
        "verification": {"all_workloads_repeated_identically": True, "all_ids_arrivals_preserved": True,
                         "paired_resources_share_exact_workloads": True},
        "limitations": ["Synthetic inputs only; no experimental B simulations or C analysis were run.",
                        "Seed reproducibility was checked within this build/environment; C++ distribution implementations can differ across toolchains.",
                        "A histogram groups arrival timestamps; it does not fit or validate a Poisson distribution."]})
    print(f"Prepared {len(summaries)} workloads, {len(manifest)} planned runs, and {len(plots)} input plots.")
    print(f"Manifest: {output / 'experiments.csv'}")
    print("No experimental simulations were run.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, SyntaxError) as exc:
        print(f"Workload preparation failed: {exc}", file=sys.stderr)
        sys.exit(1)
