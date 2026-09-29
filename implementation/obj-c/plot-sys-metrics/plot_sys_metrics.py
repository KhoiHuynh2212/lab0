#!/usr/bin/env python3
"""Obj-C plot-sys-metrics: response time and throughput vs time-step from sysmetric.csv."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

REQUIRED_COLUMNS = [
    "time-step",
    "avg-response-time",
    "median-response-time",
    "10-response-time",
    "90-response-time",
    "throughput",
]

LATENCY_STYLES = {
    "90-response-time": ("P90", "#f0a030", "--"),
    "avg-response-time": ("Avg", "#1e5ab4", "-"),
    "median-response-time": ("Median", "#20b8c8", "-"),
    "10-response-time": ("P10", "#f0a030", "--"),
}
THROUGHPUT_COLOR = "#3c7a3c"


def normalize_header(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.strip().lower())


def load_sys_metrics(input_path: Path) -> dict[str, list[float | None]]:
    """Read the required columns. Empty response cells become None."""
    if not input_path.is_file():
        raise FileNotFoundError(f"cannot read input file: {input_path}")

    with input_path.open(newline="") as input_file:
        reader = csv.reader(input_file)
        try:
            header = next(reader)
        except StopIteration as error:
            raise ValueError("input file is empty") from error

        normalized_to_index = {normalize_header(name): index for index, name in enumerate(header)}
        indexes = {}
        for column in REQUIRED_COLUMNS:
            key = normalize_header(column)
            if key not in normalized_to_index:
                raise ValueError(f"input CSV is missing a {column} column")
            indexes[column] = normalized_to_index[key]

        series: dict[str, list[float | None]] = {column: [] for column in REQUIRED_COLUMNS}
        for row_number, row in enumerate(reader, start=2):
            if not row or all(field.strip() == "" for field in row):
                continue
            if max(indexes.values()) >= len(row):
                raise ValueError(f"row {row_number} has too few columns")

            for column in REQUIRED_COLUMNS:
                text = row[indexes[column]].strip()
                if text == "":
                    if column in ("time-step", "throughput"):
                        raise ValueError(f"row {row_number} has an empty {column} value")
                    series[column].append(None)
                    continue
                try:
                    series[column].append(float(text))
                except ValueError as error:
                    raise ValueError(f"row {row_number} has a non-numeric {column} value") from error

    if not series["time-step"]:
        raise ValueError("input file has no data rows")

    return series


def plot_sys_metrics(input_path: Path, output_path: Path, title: str) -> None:
    series = load_sys_metrics(input_path)
    time_steps = series["time-step"]

    figure, (latency_axes, throughput_axes) = plt.subplots(
        2, 1, figsize=(10, 7), sharex=True
    )

    for column, (label, color, line_style) in LATENCY_STYLES.items():
        points = [(t, v) for t, v in zip(time_steps, series[column]) if v is not None]
        latency_axes.plot(
            [t for t, _ in points],
            [v for _, v in points],
            color=color,
            linestyle=line_style,
            linewidth=2,
            marker=".",
            label=label,
        )
    latency_axes.set_ylabel("response time")
    latency_axes.set_title(f"{title}: response time (latency) vs time-step")
    latency_axes.grid(True, alpha=0.3)
    latency_axes.legend(loc="upper left")

    throughput_axes.bar(time_steps, series["throughput"], color=THROUGHPUT_COLOR, width=0.8)
    throughput_axes.set_ylabel("jobs exiting")
    throughput_axes.set_xlabel("time-step")
    throughput_axes.set_title("throughput vs time-step")
    throughput_axes.grid(True, alpha=0.3)

    figure.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="plot-sys-metrics",
        description="Plot response time and throughput vs time-step from a sysmetric.csv file.",
    )
    parser.add_argument("input_csv", help="derived system metrics CSV from derive-sys-metrics")
    parser.add_argument("output_png", help="PNG file to write")
    parser.add_argument("--title", default="System metrics", help="plot title")
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)

    try:
        plot_sys_metrics(Path(arguments.input_csv), Path(arguments.output_png), arguments.title)
    except Exception as error:
        print(error, file=sys.stderr)
        return 1

    print(f"Wrote {arguments.output_png}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
