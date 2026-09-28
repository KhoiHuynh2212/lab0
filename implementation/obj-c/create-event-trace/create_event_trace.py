#!/usr/bin/env python3
"""Obj-C create-event-trace: plot every job's queue/service/error events over time."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

QUEUE_MARKER = "s"
SERVICE_MARKER = "o"
ERROR_MARKER = "x"


def parse_event_log(input_path: Path) -> dict[str, list[tuple[int, str, str]]]:
    if not input_path.is_file():
        raise FileNotFoundError(f"cannot read input file: {input_path}")

    events: dict[str, list[tuple[int, str, str]]] = {}

    with input_path.open() as input_file:
        for line in input_file:
            parts = line.split()
            if len(parts) < 5 or parts[2] != "IN" or not parts[0].isdigit():
                continue

            time = int(parts[0])
            job_id = parts[1]
            queue_id = parts[3]
            event = parts[4]
            events.setdefault(job_id, []).append((time, queue_id, event))

    if not events:
        raise ValueError("input file has no event lines")

    return events


def build_trace(
    job_events: list[tuple[int, str, str]],
    last_time: int,
) -> list[tuple[int, str, str]]:
    """One (time, queue_id, marker) point for every step the job is in the system."""
    points: list[tuple[int, str, str]] = []
    state = None
    state_start = 0
    queue_id = ""

    def flush(end_time: int) -> None:
        marker = QUEUE_MARKER if state == "queue" else SERVICE_MARKER
        for step in range(state_start, end_time):
            points.append((step, queue_id, marker))

    for time, event_queue, event in job_events:
        if event == "ARRIVE-VIA":
            state = "queue"
            state_start = time
            queue_id = event_queue
        elif event == "ENTERS-SERVER":
            if state is not None:
                flush(time)
            state = "service"
            state_start = time
        elif event in ("EXITS-SERVER", "DEPART-VIA"):
            if state is not None:
                flush(time)
                state = "queue" if event == "EXITS-SERVER" else None
                state_start = time
        elif event == "ERROR":
            if state is not None:
                flush(time)
            points.append((time, event_queue, ERROR_MARKER))
            state = None

    if state is not None:
        flush(last_time + 1)

    return points


def job_sort_key(job_id: str) -> tuple[int, str]:
    number = job_id[1:] if job_id.startswith("j") else job_id
    return (int(number), job_id) if number.isdigit() else (sys.maxsize, job_id)


def plot_trace(
    events: dict[str, list[tuple[int, str, str]]],
    output_path: Path,
    title: str,
) -> None:
    last_time = max(time for job in events.values() for time, _, _ in job)
    job_ids = sorted(events, key=job_sort_key)
    queue_ids = sorted({queue for job in events.values() for _, queue, _ in job})
    color_cycle = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    queue_colors = {
        queue: color_cycle[index % len(color_cycle)]
        for index, queue in enumerate(queue_ids)
    }

    figure, axes = plt.subplots(figsize=(10, max(3, 0.4 * len(job_ids) + 1.5)))

    for row, job_id in enumerate(job_ids):
        points = build_trace(events[job_id], last_time)
        if points:
            axes.plot(
                [time for time, _, _ in points],
                [row] * len(points),
                color="lightgray",
                linewidth=1,
                zorder=1,
            )
        for time, queue_id, marker in points:
            axes.scatter(
                time,
                row,
                marker=marker,
                color=queue_colors[queue_id],
                s=60,
                zorder=2,
            )

    axes.set_yticks(range(len(job_ids)))
    axes.set_yticklabels(job_ids)
    axes.set_xlabel("time")
    axes.set_ylabel("job id")
    axes.set_title(title)
    axes.grid(True, axis="x", alpha=0.3)

    handles = [
        plt.Line2D([], [], color=queue_colors[queue], marker="o", linestyle="", label=queue)
        for queue in queue_ids
    ]
    handles += [
        plt.Line2D([], [], color="black", marker=QUEUE_MARKER, linestyle="", label="in queue"),
        plt.Line2D([], [], color="black", marker=SERVICE_MARKER, linestyle="", label="in service"),
        plt.Line2D([], [], color="black", marker=ERROR_MARKER, linestyle="", label="error"),
    ]
    axes.legend(handles=handles, loc="upper left", bbox_to_anchor=(1.01, 1.0))

    figure.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="create-event-trace",
        description="Plot each job's queue (box), service (circle) and error (x) events over time.",
    )
    parser.add_argument("input_log", help="event log from the queue simulator")
    parser.add_argument("output_png", help="PNG file to write")
    parser.add_argument("--title", default="Event Trace", help="plot title")
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)

    try:
        events = parse_event_log(Path(arguments.input_log))
        plot_trace(events, Path(arguments.output_png), arguments.title)
    except Exception as error:
        print(error, file=sys.stderr)
        return 1

    print(f"Wrote {arguments.output_png}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
