#!/usr/bin/env python3
"""Build unchanged A/B into D, run existing tests, and verify two car-wash cases."""

import argparse
import ast
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys


SIMPLE = Path(__file__).resolve().parents[1]
REPO = SIMPLE.parents[1]
TASKS = [f"{i}: W {work}" for i, work in enumerate([5, 8, 10, 6, 5])]
COMPONENTS = [
    ("workload-generator", "obj-a/workload-generator", "workload_generator"),
    ("workload-visualizer", "obj-a/workload-visualizer", "workload_visualizer"),
    ("simple-queue-sim", "obj-b/simple-queue-sim", "queue_sim"),
]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def relative(path, start=REPO):
    return Path(os.path.relpath(path, start)).as_posix()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def command(args, cwd, env, expected_success=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env,
                            capture_output=True, text=True, errors="replace")
    record = {"argv": [str(a) for a in args], "cwd": relative(cwd),
              "exit_code": result.returncode, "stdout": result.stdout,
              "stderr": result.stderr}
    if expected_success and result.returncode:
        raise RuntimeError(f"Command failed: {args}\n{result.stdout}\n{result.stderr}")
    return record


def build_and_check(build, env):
    cxx = shutil.which(os.environ.get("CXX", "g++"))
    require(cxx is not None, "C++ compiler not found; put g++ on PATH or set CXX.")
    suffix = ".exe" if os.name == "nt" else ""
    report = {"compiler": command([cxx, "--version"], REPO, env), "components": {},
              "limitations": ["Native functional tests only; no Valgrind, ASan/UBSan, or coverage claim.",
                              "Team shell test runners were not invoked because they write into shared component directories."]}
    executables = {}
    for name, folder, stem in COMPONENTS:
        component = REPO / "implementation" / folder
        executable = build / (name + suffix)
        test_executable = build / ("test-" + name + suffix)
        flags = [cxx, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-O2",
                 "-I" + relative(component / "include")]
        records = []
        print(f"Building and checking {name} (unchanged sources)", flush=True)
        for main, output in [(component / "src/main.cpp", executable),
                             (component / f"tests/test_{stem}.cpp", test_executable)]:
            records.append(command(flags + [relative(main), relative(component / f"src/{stem}.cpp"),
                                             "-o", relative(output)], REPO, env))
        records.append(command([test_executable], build, env))
        print(records[-1]["stdout"].strip(), flush=True)
        records.append(command([executable, "--help"], build, env))
        missing_args = command([executable], build, env, expected_success=False)
        require(missing_args["exit_code"] != 0, f"{name} accepted missing arguments")
        records.append(missing_args)
        report["components"][name] = records
        executables[name] = executable
    return executables, report


def verify_case(run, workload, expected_arrivals, expected_rejections):
    source = read_csv(workload)
    require([int(row["arrival time"]) for row in source] == expected_arrivals,
            "Unexpected fixture arrivals")
    require([row["Job Id"] for row in source] == [f"j{i}" for i in range(len(source))],
            "Fixture IDs must be unique and ordered")
    require(all(ast.literal_eval(row["Tasks"]) == TASKS for row in source),
            "Fixture tasks differ from car-wash specification")
    completed = read_csv(run / "completed.csv")
    count = len(source) - expected_rejections
    expected_ids = [f"j{i}" for i in range(count)]
    require([row["Job Id"] for row in completed] == expected_ids,
            "Wrong completed cars or FIFO order")
    require([int(row["arrival time"]) for row in completed] == [34 * (i + 1) for i in range(count)],
            "Completion times differ from hand calculation")
    require(all(ast.literal_eval(row["Tasks"]) == [] for row in completed),
            "Completed cars still have tasks")
    events = [line.split() for line in (run / "job.log").read_text().splitlines()]
    require(all(len(e) >= 5 and e[2:4] == ["IN", "q0"] for e in events),
            "Unexpected event format or queue")
    arrivals = [e[1] for e in events if e[4] == "ARRIVE-VIA"]
    require(arrivals == [row["Job Id"] for row in source], "Missing/duplicate arrival events")
    errors = [e for e in events if e[4] == "ERROR"]
    require(len(errors) == expected_rejections and all(e[5:] == ["QUEUE-FULL"] for e in errors),
            "Unexpected rejection count or error type")
    require([e[1] for e in errors] == [f"j{i}" for i in range(count, len(source))],
            "Unexpected rejected cars")
    starts = [e for e in events if e[4] == "ENTERS-SERVER"]
    stops = [e for e in events if e[4] == "EXITS-SERVER"]
    departures = [e for e in events if e[4] == "DEPART-VIA"]
    require([e[1] for e in starts] == expected_ids and [e[1] for e in stops] == expected_ids
            and [e[1] for e in departures] == expected_ids, "Missing or duplicate service/departure events")
    for i, (start, stop, departure) in enumerate(zip(starts, stops, departures)):
        require(int(start[0]) == 34 * i and int(stop[0]) == 34 * (i + 1)
                and stop[0] == departure[0], "Incorrect service duration or departure")
    metrics = read_csv(run / "qmet.csv")
    end = 34 * count
    require([int(row["time-step"]) for row in metrics] == list(range(end + 1)),
            "Missing or unexpected measurement timesteps")
    require(all(row["queue-sys-id"] == "q0" and 0 <= int(row["num-jobs-in-queue"]) <= 20
                and 0 <= int(row["num-jobs-in-service"]) <= 1 for row in metrics),
            "Queue/server capacity violated")
    cumulative_errors = [int(row["num-of-errors"]) for row in metrics]
    require(cumulative_errors == sorted(cumulative_errors) and cumulative_errors[-1] == expected_rejections,
            "Incorrect cumulative error measurements")
    require(int(metrics[-1]["num-jobs-in-queue"]) == 0
            and int(metrics[-1]["num-jobs-in-service"]) == 0, "Simulation did not drain")
    if expected_rejections:
        require(int(metrics[0]["num-jobs-in-queue"]) == 20
                and int(metrics[0]["num-jobs-in-service"]) == 1, "Boundary capacity not exercised")
    return {"status": "passed", "offered": len(source), "completed": count,
            "rejected": len(errors), "last_departure": end, "service_steps_per_car": 34,
            "completion_times": [int(row["arrival time"]) for row in completed],
            "checks": ["arrival accounting", "FIFO timing", "service duration", "empty completed tasks",
                       "queue/server bounds", "cumulative errors", "all admitted cars drained"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=SIMPLE / "runs",
                        help="New directory for evidence; refuses to overwrite existing output")
    args = parser.parse_args()
    output = args.output_root.resolve()
    require(output.is_relative_to(SIMPLE), "Keep output within analysis/simple.")
    require(not output.exists(), f"Output already exists: {output}. Choose a new --output-root.")
    build = SIMPLE / ".build"
    build.mkdir(exist_ok=True)
    temp = build / "test-temp"
    temp.mkdir(exist_ok=True)
    env = os.environ.copy()
    # Existing tests create/delete named fixtures in the OS temp directory.
    # Keep those fixtures inside this isolated D build directory as well.
    env.update({"TMP": str(temp), "TEMP": str(temp), "TMPDIR": str(temp)})
    revision = command(["git", "rev-parse", "HEAD"], REPO, env)["stdout"].strip()
    source_paths = [p for _, folder, _ in COMPONENTS
                    for p in (REPO / "implementation" / folder).rglob("*")
                    if p.suffix in {".cpp", ".hpp"}]
    hashes = {relative(p): sha256(p) for p in sorted(source_paths)}
    executables, report = build_and_check(build, env)
    output.mkdir(parents=True)
    report.update({"recorded_at_utc": datetime.now(timezone.utc).isoformat(),
                   "platform": platform.platform(), "python": sys.version,
                   "source_commit": revision, "source_sha256": hashes,
                   "source_changes": command(["git", "status", "--porcelain", "--", "implementation/obj-a",
                                               "implementation/obj-b"], REPO, env)["stdout"]})
    save_json(output / "build-checks.json", report)
    template_path = SIMPLE / "configs/carwash-template.csv"
    template = read_csv(template_path)
    require(len(template) == 1, "Template must contain one queue")
    for name, arrivals, rejections in [("smoke", [0, 4, 9, 10, 15, 22], 0),
                                       ("overflow", [0] * 22, 1)]:
        run = output / name
        run.mkdir()
        workload = SIMPLE / "workloads" / f"{name}.csv"
        config = dict(template[0])
        config.update({"source": relative(workload, run), "sink": "completed.csv"})
        with (run / "config.csv").open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(config), lineterminator="\n")
            writer.writeheader()
            writer.writerow(config)
        invocation = [executables["simple-queue-sim"], "config.csv", "--metrics", "qmet.csv", "--events", "job.log"]
        result = command(invocation, run, env, expected_success=False)
        (run / "stdout.txt").write_text(result["stdout"], encoding="utf-8")
        (run / "stderr.txt").write_text(result["stderr"], encoding="utf-8")
        metadata = {"source_commit": revision, "input_kind": "hand-constructed validation fixture",
                    "seed": None, "workload": relative(workload), "workload_sha256": sha256(workload),
                    "config_sha256": sha256(run / "config.csv"), "command": result,
                    "simulator_sha256": sha256(executables["simple-queue-sim"]),
                    "build_report": "../build-checks.json", "time_unit_seconds": 10}
        save_json(run / "metadata.json", metadata)
        require(result["exit_code"] == 0, f"{name} failed; see saved stderr.txt")
        verification = verify_case(run, workload, arrivals, rejections)
        save_json(run / "verification.json", verification)
        print(f"{name}: {verification}", flush=True)
    require(hashes == {relative(p): sha256(p) for p in sorted(source_paths)},
            "Shared source files changed during validation")
    print(f"Car-wash validation passed. Evidence: {output}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError) as exc:
        print(f"Validation failed: {exc}", file=sys.stderr)
        sys.exit(1)
