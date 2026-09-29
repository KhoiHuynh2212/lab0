#!/usr/bin/env python3
"""Analyze saved D experiments with unchanged, versioned C tools and explicit adapters."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import uuid

from analysis_helpers import (adapt_events, adapt_metrics, check_derived, check_use,
                              paired_comparisons, summarize)
from prepare_workloads import write_csv
from run_experiments import build_simulator, load_manifest, verify_run
from validate_carwash import REPO, SIMPLE, TASKS, command, read_csv, relative, require, save_json, sha256


C_REF = "c65b1516fc98a5fc30115c4827a851fecadbc0d4"
CPP_TOOLS = ["filter-by-queue", "filter-by-job", "compute-use", "derive-event-metrics", "derive-sys-metrics"]
PY_TOOLS = ["plot-use", "plot-sys-metrics", "plot-derive-event-metrics", "create-event-trace"]
LIMITATIONS = [
    "C utilization is round(mean(any server busy)): a 0/1 activity indicator, not a utilization percentage or normalized bay capacity.",
    "C saturation is a rolling average rounded to whole cars.",
    "With D's incremental-error input, C Errors(W) is a window count. The unchanged plot-use title 'Error Rate' does not divide by duration.",
    "System statistics occur only at departures and round means/medians to whole steps. Whole-run timing summaries use completed per-job rows instead.",
    "Native functional validation only; no Valgrind, sanitizer, or coverage claim.",
    "The handout defines percentage utilization but shows an ambiguous example table; this study reports C's actual semantics and does not resolve that ambiguity.",
]


class Analysis:
    def __init__(self, output, c_ref):
        self.output = output
        self.workspace = SIMPLE / ".build" / ("analysis-" + uuid.uuid4().hex[:10])
        self.workspace.mkdir(parents=True)
        self.env = os.environ.copy()
        for name in ["TEMP", "TMP", "TMPDIR"]:
            self.env[name] = str(self.workspace / "tmp")
        (self.workspace / "tmp").mkdir()
        self.env["MPLCONFIGDIR"] = str(self.workspace / "matplotlib-cache")
        self.report = {"status": "running", "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
                       "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
                       "platform": platform.platform(), "python": sys.version, "scratch": relative(self.workspace),
                       "limitations": LIMITATIONS, "commands": [], "runs": {}, "validation": {},
                       "figures": [], "tool_coverage": [], "script_sha256": {relative(p): sha256(p) for p in
                           [Path(__file__), SIMPLE / "scripts/analysis_helpers.py", SIMPLE / "scripts/run_experiments.py",
                            SIMPLE / "scripts/prepare_workloads.py", SIMPLE / "scripts/validate_carwash.py"]}}
        self.tools = {}
        self.report["c_commit"] = subprocess.check_output(["git", "rev-parse", f"{c_ref}^{{commit}}"], cwd=REPO, text=True).strip()

    def save(self):
        save_json(self.output / "execution.json", self.report)

    def run(self, args, cwd=None, success=True):
        record = command(args, cwd or self.workspace, self.env, expected_success=False)
        self.report["commands"].append(record)
        if success:
            require(record["exit_code"] == 0, f"Analysis command failed: {record}")
        return record

    def tool(self, name, *args):
        if name not in self.report["tool_coverage"]:
            self.report["tool_coverage"].append(name)
        return self.run(self.tools[name] + list(args))

    def build(self):
        revision = self.report["c_commit"]
        names = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", revision, "--", "implementation/obj-c"], cwd=REPO, text=True).splitlines()
        source_root = self.workspace / "source"
        hashes = {}
        for name in names:
            path = Path(name)
            if path.suffix not in {".cpp", ".hpp", ".py"} or ".test-build" in path.parts:
                continue
            dest = source_root / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(subprocess.check_output(["git", "show", f"{revision}:{name}"], cwd=REPO))
            hashes[name] = sha256(dest)
        self.report["c_source_sha256"] = hashes
        cxx = shutil.which(os.environ.get("CXX", "g++"))
        require(cxx is not None, "Put g++ on PATH or set CXX")
        self.report["compiler"] = self.run([cxx, "--version"])
        suffix = ".exe" if os.name == "nt" else ""
        for name in CPP_TOOLS:
            folder = source_root / "implementation/obj-c" / name
            stem = name.replace("-", "_")
            binary = self.workspace / (name + suffix)
            flags = [cxx, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-O2", "-I" + str(folder / "include")]
            for src, dest in [(folder / "src/main.cpp", binary),
                              (folder / f"tests/test_{stem}.cpp", self.workspace / ("test-" + name + suffix))]:
                self.run(flags + [src, folder / f"src/{stem}.cpp", "-o", dest])
            self.run([self.workspace / ("test-" + name + suffix)])
            self.run([binary, "--help"])
            require(self.run([binary], success=False)["exit_code"] != 0, "Missing arguments accepted")
            self.tools[name] = [binary]
            print(f"Built and checked unchanged C tool: {name}", flush=True)
        for name in PY_TOOLS:
            path = source_root / "implementation/obj-c" / name / (name.replace("-", "_") + ".py")
            self.tools[name] = [sys.executable, path]
            self.run(self.tools[name] + ["--help"])
            require(self.run(self.tools[name], success=False)["exit_code"] != 0, "Missing plot arguments accepted")
        self.save()

    def process(self, name, folder, workload, row, summary):
        dest = self.workspace / "derived" / name
        dest.mkdir(parents=True)
        before_hashes = {f: sha256(folder / f) for f in ["config.csv", "completed.csv", "qmet.csv", "job.log"]}
        config = read_csv(folder / "config.csv")[0]
        record = {"raw_folder": relative(folder), "input_sha256": before_hashes, "workload_sha256": sha256(workload)}
        self.tool("filter-by-queue", folder / "qmet.csv", dest / "q0.csv", "--queues=q0")
        require(read_csv(dest / "q0.csv") == read_csv(folder / "qmet.csv"), "Filter changed single-queue rows")
        record["metrics_adapter"] = adapt_metrics(dest / "q0.csv", dest / "qmet-increments.csv", int(row["arrival_horizon"]))
        record["log_adapter"] = adapt_events(folder / "job.log", dest / "system-input.log", config["source"], config["sink"])
        self.tool("compute-use", dest / "qmet-increments.csv", dest / "quse.csv")
        self.tool("derive-event-metrics", folder / "job.log", dest / "emetric.csv")
        self.tool("derive-sys-metrics", dest / "system-input.log", dest / "sysmetric.csv")
        events, system = check_derived(dest / "emetric.csv", dest / "sysmetric.csv", folder, workload, summary)
        samples, count = check_use(dest / "qmet-increments.csv", dest / "quse.csv", name, int(row["arrival_horizon"]))
        require(all(sha256(folder / f) == h for f, h in before_hashes.items()), "Original evidence changed")
        record.update({"status": "passed_under_documented_C_contract", "event_rows": len(events), "system_rows": len(system),
                       "use_rows": count, "raw_evidence_unchanged": True,
                       "checks": ["exact q0 filter", "all original job IDs/arrivals", "completed/rejected population",
                                  "waiting/service/turnaround versus events", "sink departures", "per-exit C statistics",
                                  "occupancy versus summed customer times", "all 10/100/1000-step C windows"],
                       "intermediate_sha256": {p.name: sha256(p) for p in dest.iterdir() if p.is_file()}})
        return dest, record, events, system, samples

    def validate(self):
        for name, horizon in [("smoke", 205), ("overflow", 715), ("two-bay", 68)]:
            if name == "two-bay":
                folder = self.workspace / "two-bay-input"
                folder.mkdir()
                workload = folder / "workload.csv"
                write_csv(workload, ["Job Id", "arrival time", "Tasks"],
                          [{"Job Id": f"j{i}", "arrival time": t, "Tasks": repr(TASKS)} for i, t in enumerate([0, 0, 1, 2])])
                config = read_csv(SIMPLE / "configs/carwash-template.csv")[0]
                config.update({"# of servers": "2", "source": "workload.csv", "sink": "completed.csv"})
                write_csv(folder / "config.csv", list(config), [config])
                binary, build = build_simulator(folder, self.env)
                self.report["validation_simulator_build"] = build
                self.run([binary, "config.csv", "--metrics", "qmet.csv", "--events", "job.log"], cwd=folder)
            else:
                folder, workload = SIMPLE / "runs" / name, SIMPLE / "workloads" / f"{name}.csv"
            row = {"run_id": name, "family": "validation", "workload_id": name, "demand": "fixed", "seed": "",
                   "arrival_horizon": str(horizon), "queue_size": "20", "num_servers": "2" if name == "two-bay" else "1"}
            summary, verification = verify_run(folder, row, read_csv(workload))
            dest, record, events, system, _ = self.process(name, folder, workload, row, summary)
            record["raw_verification"] = verification
            if name == "two-bay":
                require([int(r["time-step"]) for r in system] == [34, 68]
                        and [int(r["throughput"]) for r in system] == [2, 2], "Tied-departure validation failed")
                require([int(system[1][k]) for k in ["Avg-response-time", "median-response-time", "10-response-time", "90-response-time"]]
                        == [67, 67, 66, 67], "Rounded tied statistics differ")
                require(summary["completed_before_cutoff"] == 2 and summary["unfinished_at_cutoff"] == 2, "Half-open cutoff failed")
            selected = "j0,j5" if name == "smoke" else "j0,j21" if name == "overflow" else "j2,j3"
            config = read_csv(folder / "config.csv")[0]
            record["filter_adapter"] = adapt_events(folder / "job.log", dest / "filter-input.log", config["source"], config["sink"], aliases=False)
            self.tool("filter-by-job", dest / "filter-input.log", dest / "selected.log", "--jobs=" + selected)
            chosen = set(selected.split(","))
            expected = [line for line in (folder / "job.log").read_text().splitlines() if line.split()[1] in chosen]
            require((dest / "selected.log").read_text().splitlines() == ["time job IN queue event detail"] + expected, "Job filter changed events")
            record["selected_job_ids"] = sorted(chosen)
            record["intermediate_sha256"].update({p.name: sha256(p) for p in [dest / "filter-input.log", dest / "selected.log"]})
            example = self.output / "examples" / name
            example.mkdir(parents=True)
            for filename in ["emetric.csv", "sysmetric.csv", "quse.csv", "selected.log"]:
                shutil.copyfile(dest / filename, example / filename)
            self.report["validation"][name] = record
            print(f"Validated C against {name}: {len(events)} jobs, {len(system)} departure timestamps", flush=True)
        self.save()

    def plots(self, selected):
        figures = self.output / "figures"
        figures.mkdir()

        def add(name, tool, args, caption):
            self.tool(tool, *args)
            self.report["figures"].append({"file": name, "tool": tool, "caption": caption})

        for demand in ["near", "over"]:
            run_id = f"baseline-{demand}-h28800-seed42"
            if run_id not in selected:
                continue
            dest = figures / f"{demand}-use"
            self.tool("plot-use", self.workspace / "derived" / run_id / "quse.csv", dest)
            for metric in ["utilization", "saturation", "error-rate"]:
                caption = {"utilization": "C rounded any-bay-busy indicator (0/1), NOT a percentage or bay-capacity utilization.",
                           "saturation": "C rolling average waiting cars, rounded to whole cars.",
                           "error-rate": "Despite C's built-in Error Rate title, values are new-error COUNTS per 10/100/1000-step window, not rates."}[metric]
                self.report["figures"].append({"file": f"{demand}-use/{metric}.png", "tool": "plot-use",
                                               "caption": f"{run_id}. {caption} Includes draining and any explicitly appended idle tail."})
        for run_id in ["baseline-light-h28800-seed42", "baseline-near-h28800-seed42", "baseline-over-h28800-seed42", "baseline-over-h2880-seed42"]:
            if run_id in selected:
                name = run_id + "-system.png"
                add(name, "plot-sys-metrics", [self.workspace / "derived" / run_id / "sysmetric.csv", figures / name,
                                              "--title=" + run_id],
                    "Latency is arrival-to-completion in 10-second steps; throughput bars count exits at each timestamp. Only departure timestamps have latency. Connecting lines are visual guides, not observations at missing timestamps. Includes draining.")
        for run_id in ["baseline-over-h28800-seed42", "queue40-over-h28800-seed42", "bays2-over-h28800-seed42"]:
            if run_id in selected:
                native = read_csv(self.workspace / "derived" / run_id / "emetric.csv")
                # Completed visits have a first-service timestamp; rejected visits have a blank response.
                completed = [r for r in native if r["response-time"] != ""]
                path = self.workspace / "derived" / run_id / "completed-emetric.csv"
                write_csv(path, list(completed[0]), completed)
                name = run_id + "-waiting.png"
                add(name, "plot-derive-event-metrics", [path, figures / name, "--hist=response-time", "--bins=20", "--title=" + run_id + " - completed-car wait (steps)"],
                    "Completed cars only, followed through draining. response-time means waiting before first service; multiply steps by 1/6 for minutes. Native histogram bin widths and axes are chosen per case; compare numeric summaries rather than bar heights across plots.")
                if run_id.startswith("baseline"):
                    name = run_id + "-arrival-wait.png"
                    add(name, "plot-derive-event-metrics", [path, figures / name, "--scat=arrival-time,response-time", "--title=Overload - arrival and completed-car wait (steps)"],
                        "Completed-car arrival time versus waiting time, both in steps. Rejected cars are reported separately.")
        for case, filename in [("smoke", "selected.log"), ("overflow", "selected.log")]:
            name = case + "-trace.png"
            add(name, "create-event-trace", [self.workspace / "derived" / case / filename, figures / name, "--title=" + case + " - selected car events (steps)"],
                "Selected cars only: smoke j0/j5 shows immediate service versus waiting; overflow j0/j21 shows acceptance versus rejection. Squares indicate waiting, circles service, x rejection.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=SIMPLE / "results/analysis")
    parser.add_argument("--c-ref", default=C_REF, help="Git revision containing C; default is the validated snapshot")
    parser.add_argument("--run-ids", nargs="+", help="Optional pilot subset; include matching baselines for variants")
    args = parser.parse_args()
    output = args.output_root.resolve()
    require(output.is_relative_to(SIMPLE) and output != SIMPLE and not output.exists(), "Choose a new output directory inside D; refusing to overwrite evidence")
    manifest_path = SIMPLE / "experiments.csv"
    rows, _ = load_manifest(manifest_path)
    by_id = {r["run_id"]: r for r in rows}
    if args.run_ids:
        require(len(args.run_ids) == len(set(args.run_ids)) and set(args.run_ids) <= set(by_id), "Unknown or duplicate run IDs")
    selected = [r for r in rows if not args.run_ids or r["run_id"] in args.run_ids]
    ids = {r["run_id"] for r in selected}
    require(all(r["baseline_run_id"] in ids for r in selected), "Include each comparison's baseline in the selected runs")
    raw_report_path = SIMPLE / "runs/experiments/execution.json"
    raw_report = json.loads(raw_report_path.read_text())
    require(raw_report["status"] == "passed", "Raw experiments are not verified")
    summary_path = SIMPLE / "results/tables/run-summary.csv"
    require(sha256(manifest_path) == raw_report["manifest_sha256"] and sha256(summary_path) == raw_report["summary_sha256"], "Saved manifest or summary changed")
    raw_summary = {r["run_id"]: r for r in read_csv(summary_path)}
    for row in selected:
        folder = SIMPLE / "runs/experiments" / row["run_id"]
        metadata = raw_report["runs"][row["run_id"]]["metadata"]
        require(metadata["run"] == row and sha256(folder / "config.csv") == metadata["config_sha256"], "Raw configuration/manifest mismatch")
        require(all(sha256(folder / filename) == metadata["output_sha256"][filename] for filename in ["completed.csv", "qmet.csv", "job.log"]), "Raw output changed")
    # Import check happens before creating the destination, so missing dependencies leave no partial output.
    import matplotlib
    output.mkdir(parents=True)
    analysis = Analysis(output, args.c_ref)
    analysis.report.update({"manifest_sha256": sha256(manifest_path), "raw_execution_sha256": sha256(raw_report_path),
                            "raw_summary_sha256": sha256(summary_path), "matplotlib_version": matplotlib.__version__,
                            "selected_run_ids": [r["run_id"] for r in selected]})
    analysis.save()
    try:
        analysis.build()
        analysis.validate()
        jobs, systems, samples, summaries = [], [], [], []
        for i, row in enumerate(selected, 1):
            name = row["run_id"]
            workload = SIMPLE / row["workload_file"]
            _, record, events, system, use_samples = analysis.process(name, SIMPLE / "runs/experiments" / name,
                                                                    workload, row, raw_summary[name])
            summary = summarize(row, raw_summary[name], events, system)
            summaries.append(summary)
            jobs.extend({"run_id": name, **r} for r in events)
            systems.extend({"run_id": name, **r} for r in system)
            samples.extend(use_samples)
            analysis.report["runs"][name] = record
            analysis.save()
            print(f"[{i}/{len(selected)}] {name}: verified; mean completed-car wait {summary['mean_wait_minutes']} minutes", flush=True)
        tables = output / "tables"
        for filename, data in [("job-metrics.csv", jobs), ("system-metrics.csv", systems), ("use-samples.csv", samples),
                               ("performance-summary.csv", summaries), ("paired-comparisons.csv", paired_comparisons(selected, summaries))]:
            if data:
                write_csv(tables / filename, list(data[0]), data)
        analysis.plots(ids)
        require(set(analysis.report["tool_coverage"]) == set(CPP_TOOLS + PY_TOOLS), "Incomplete tool demonstration")
        require(all(sha256(analysis.workspace / "source" / p) == h for p, h in analysis.report["c_source_sha256"].items()), "C snapshot changed")
        require(sha256(raw_report_path) == analysis.report["raw_execution_sha256"] and sha256(summary_path) == analysis.report["raw_summary_sha256"], "Raw run records changed")
        from PIL import Image
        for path in (output / "figures").rglob("*.png"):
            with Image.open(path) as picture:
                picture.verify()
        analysis.report.update({"status": "passed_with_documented_measurement_limitations", "completed_at_utc": datetime.now(timezone.utc).isoformat(),
                                "c_source_unchanged": True, "completed_experiments": len(summaries),
                                "artifacts_sha256": {relative(p, output): sha256(p) for p in output.rglob("*") if p.is_file() and p.name != "execution.json"}})
        analysis.save()
    except Exception as exc:
        analysis.report.update({"status": "failed", "error": str(exc)})
        analysis.save()
        raise
    print(f"Analyzed {len(selected)} saved runs using unchanged C. Results: {output}")
    print("C utilization remains a rounded activity indicator; see documented measurement limitations.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, ImportError) as exc:
        print(f"Analysis failed: {exc}", file=sys.stderr)
        sys.exit(1)
