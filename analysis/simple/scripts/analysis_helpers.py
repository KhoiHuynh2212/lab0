"""Adapt D input copies and check the existing C contracts without modifying C."""

from collections import defaultdict, deque
import csv
import math
from statistics import mean, median

from prepare_workloads import write_csv
from validate_carwash import read_csv, require


def adapt_metrics(source, target, horizon):
    """Difference cumulative errors; append known idle time if B drained before H."""
    require(source.resolve() != target.resolve() and not target.exists(), "Refusing to overwrite adapter input/output")
    rows = read_csv(source)
    require(bool(rows), "Empty queue measurements")
    previous = 0
    for t, row in enumerate(rows):
        require(int(row["time-step"]) == t and row["queue-sys-id"] == "q0", "Expected contiguous q0 measurements from zero")
        total = int(row["num-of-errors"])
        require(total >= previous, "Cumulative errors decreased")
        row["num-of-errors"] = str(total - previous)
        previous = total
    require(int(rows[-1]["num-jobs-in-queue"]) == int(rows[-1]["num-jobs-in-service"]) == 0,
            "Cannot infer idle tail for an unfinished run")
    original_count = len(rows)
    for t in range(original_count, horizon):
        rows.append({**rows[-1], "time-step": str(t), "num-of-errors": "0"})
    write_csv(target, list(rows[0]), rows)
    return {"original_rows": original_count, "idle_rows_appended": len(rows) - original_count,
            "new_errors_total": previous, "error_convention": "Per-step increments, differenced before any slicing."}


def adapt_events(source, target, source_label, sink_label, aliases=True):
    """Add C's header; optionally alias this single queue's known boundaries."""
    require(source.resolve() != target.resolve() and not target.exists(), "Refusing to overwrite adapter input/output")
    count = 0
    with source.open(encoding="utf-8") as src, target.open("w", encoding="utf-8", newline="\n") as dst:
        dst.write("time job IN queue event detail\n")
        for line in src:
            fields = line.rstrip("\r\n").split(maxsplit=5)
            require(len(fields) == 6 and fields[2:4] == ["IN", "q0"], "Unexpected D event")
            if fields[4] in {"ARRIVE-VIA", "DEPART-VIA"}:
                expected = source_label if fields[4] == "ARRIVE-VIA" else sink_label
                require(fields[5] == expected, "Unexpected boundary label; refusing to reinterpret an edge")
                alias = ("source.wl" if fields[4] == "ARRIVE-VIA" else "sink.wl") if aliases else fields[5]
                dst.write(" ".join(fields[:5] + [alias]) + "\n")
            else:
                dst.write(line.rstrip("\r\n") + "\n")
            count += 1
    return {"events": count, "header_added": True,
            "aliases": {"source.wl": source_label, "sink.wl": sink_label} if aliases else {}}


def event_facts(path):
    """Independent check of C's derived values against original event timestamps."""
    facts = {}
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            t, job, _, queue, action, detail = line.rstrip().split(maxsplit=5)
            t = int(t)
            require(queue == "q0", "Unexpected queue")
            if action == "ARRIVE-VIA":
                require(job not in facts, "Duplicate arrival")
                facts[job] = {"arrival": t}
            elif action in {"ENTERS-SERVER", "EXITS-SERVER", "DEPART-VIA", "ERROR"}:
                field = {"ENTERS-SERVER": "start", "EXITS-SERVER": "exit", "DEPART-VIA": "depart", "ERROR": "reject"}[action]
                require(job in facts and field not in facts[job], "Missing arrival or duplicate lifecycle event")
                if action == "ERROR":
                    require(detail == "QUEUE-FULL", "Unexpected error")
                facts[job][field] = t
    return facts


def nearest_rank(values, pct):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * pct / 100) - 1)]


def rounded_average(total, n):
    return (total + n // 2) // n


def check_derived(events_path, system_path, raw_folder, workload, expected_summary):
    facts = event_facts(raw_folder / "job.log")
    arrivals = {r["Job Id"]: int(r["arrival time"]) for r in read_csv(workload)}
    require({j: f["arrival"] for j, f in facts.items()} == arrivals, "Raw events differ from source arrivals")
    sink = {r["Job Id"]: int(r["arrival time"]) for r in read_csv(raw_folder / "completed.csv")}
    require({j: f["depart"] for j, f in facts.items() if "depart" in f} == sink, "Sink/event mismatch")
    events = read_csv(events_path)
    require(len(events) == len(facts) and {r["job-id"] for r in events} == set(facts), "C lost or duplicated jobs")
    accepted, buckets = [], defaultdict(list)
    for row in events:
        job = row["job-id"]
        f = facts[job]
        require(row["queue-sys-id"] == "q0" and int(row["arrival-time"]) == f["arrival"], "Wrong C arrival")
        if job in sink:
            require("reject" not in f and f["exit"] == f["depart"] and f["exit"] - f["start"] == 34, "Invalid admitted lifecycle")
            wait, total = f["start"] - f["arrival"], f["depart"] - f["arrival"]
            require(int(row["response-time"]) == int(row["time-in-queue"]) == wait
                    and int(row["time-in-service"]) == 34 and int(row["turn-around-time"]) == total,
                    "C timing mismatch")
            accepted.append(row)
            buckets[f["depart"]].append(total)
            row["outcome"] = "completed"
        else:
            require(f.get("reject") == f["arrival"] and "start" not in f, "Unexpected rejection lifecycle")
            require(row["response-time"] == "" and all(row[k] == "0" for k in ["time-in-queue", "time-in-service", "turn-around-time"]),
                    "Changed C rejected-job convention")
            row["outcome"] = "rejected"
    system = read_csv(system_path)
    require(len(system) == len(buckets) and [int(r["time-step"]) for r in system] == sorted(buckets), "C lost or duplicated departure timestamps")
    for row in system:
        values = sorted(buckets[int(row["time-step"])])
        n = len(values)
        med = values[n // 2] if n % 2 else rounded_average(values[n // 2 - 1] + values[n // 2], 2)
        expected = [rounded_average(sum(values), n), med, nearest_rank(values, 10), nearest_rank(values, 90), n]
        columns = ["Avg-response-time", "median-response-time", "10-response-time", "90-response-time", "throughput"]
        require([int(row[c]) for c in columns] == expected, "C system statistics mismatch")
    require(len(events) == int(expected_summary["offered"]) and len(accepted) == int(expected_summary["completed"])
            and len(events) - len(accepted) == int(expected_summary["rejected"]), "C counts differ from saved run summary")
    # Occupancy over [0,last_departure) must equal admitted customers' total times.
    with (raw_folder / "qmet.csv").open(newline="", encoding="utf-8") as stream:
        metrics = list(csv.DictReader(stream))
    require(sum(int(r["num-jobs-in-queue"]) for r in metrics) == sum(int(r["time-in-queue"]) for r in accepted), "Waiting occupancy/time mismatch")
    require(sum(int(r["num-jobs-in-service"]) for r in metrics) == 34 * len(accepted), "Service occupancy/time mismatch")
    return events, system


def check_use(input_path, output_path, run_id, horizon):
    """Check C's actual rounded semantics, without relabeling them as percentages."""
    windows = {w: deque() for w in (10, 100, 1000)}
    sums = {w: [0, 0, 0] for w in windows}
    samples, count = [], 0
    with input_path.open(newline="", encoding="utf-8") as src, output_path.open(newline="", encoding="utf-8") as out:
        source, result = csv.DictReader(src), csv.DictReader(out)
        for t, row in enumerate(source):
            actual = next(result, None)
            require(actual is not None and int(actual["time-step"]) == t, "Missing C USE row")
            sample = (int(int(row["num-jobs-in-service"]) > 0), int(row["num-jobs-in-queue"]), int(row["num-of-errors"]))
            for w, queue in windows.items():
                queue.append(sample)
                for i, value in enumerate(sample):
                    sums[w][i] += value
                if len(queue) > w:
                    old = queue.popleft()
                    for i, value in enumerate(old):
                        sums[w][i] -= value
                expected = [rounded_average(sums[w][0], len(queue)), rounded_average(sums[w][1], len(queue)), sums[w][2]]
                require([int(actual[f"{name}({w})"]) for name in ["Utilization", "Saturation", "Errors"]] == expected,
                        "C USE contract changed; review units/adapters before continuing")
            if t in {0, 9, 99, 999, horizon - 1}:
                samples.append({"run_id": run_id, **actual})
            count += 1
        require(next(result, None) is None, "Extra C USE row")
    require(count >= horizon, "Missing idle tail for arrival-window observation")
    return samples, count


def summarize(row, original, events, system):
    accepted = [r for r in events if r["outcome"] == "completed"]
    horizon = int(row["arrival_horizon"])
    before = sum(int(r["throughput"]) for r in system if int(r["time-step"]) < horizon)
    require(before == int(original["completed_before_cutoff"]), "C cutoff throughput mismatch")
    result = {k: row[k] for k in ["run_id", "family", "demand", "seed", "arrival_horizon", "queue_size", "num_servers"]}
    result.update({k: original[k] for k in ["offered", "completed", "rejected", "rejection_fraction", "completed_before_cutoff", "unfinished_at_cutoff", "last_departure", "maximum_waiting"]})
    result["arrival_window_throughput_cars_per_hour"] = f"{before * 360 / horizon:.6f}"
    last = int(original["last_departure"])
    result["drained_throughput_cars_per_hour"] = f"{len(accepted) * 360 / last:.6f}" if last else ""
    for label, column in [("wait", "response-time"), ("total", "turn-around-time")]:
        values = [int(r[column]) / 6 for r in accepted]  # 10-second steps to minutes
        for stat, value in [("mean", mean(values)), ("median", median(values)), ("p10", nearest_rank(values, 10)),
                            ("p90", nearest_rank(values, 90)), ("max", max(values))]:
            result[f"{stat}_{label}_minutes"] = f"{value:.6f}"
    return result


def paired_comparisons(rows, summaries):
    by_id = {r["run_id"]: r for r in summaries}
    comparisons = []
    for row in rows:
        if row["family"] == "baseline" or row["run_id"] not in by_id:
            continue
        require(row["baseline_run_id"] in by_id, "Selected comparison lacks its baseline")
        value, baseline = by_id[row["run_id"]], by_id[row["baseline_run_id"]]
        item = {k: row[k] for k in ["run_id", "baseline_run_id", "demand", "seed", "changed_resource", "resource_change_percent"]}
        for key in ["mean_wait_minutes", "p90_wait_minutes", "mean_total_minutes", "arrival_window_throughput_cars_per_hour"]:
            a, b = float(value[key]), float(baseline[key])
            item[f"delta_{key}"] = f"{a-b:.6f}"
            item[f"percent_change_{key}"] = f"{100*(a-b)/b:.6f}" if b else ""
        item["delta_rejection_percentage_points"] = f"{100*(int(value['rejected'])/int(value['offered'])-int(baseline['rejected'])/int(baseline['offered'])):.6f}"
        comparisons.append(item)
    return comparisons
