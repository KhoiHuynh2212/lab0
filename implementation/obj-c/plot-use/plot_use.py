#!/usr/bin/env python3
"""Obj-C plot-use: plot USE metrics from quse.csv into three PNG graphs."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
DEFAULT_RESULTS_DIRECTORY = SCRIPT_DIRECTORY / "results"

REQUIRED_COLUMNS = [
    "time-step",
    "Utilization(10)",
    "Saturation(10)",
    "Errors(10)",
    "Utilization(100)",
    "Saturation(100)",
    "Errors(100)",
    "Utilization(1000)",
    "Saturation(1000)",
    "Errors(1000)",
]

WINDOW_SIZES = (10, 100, 1000)

# Shorter windows use darker shades within each metric color family.
UTILIZATION_COLORS = {
    10: "#1e5ab4",
    100: "#468cdc",
    1000: "#96bef0",
}
SATURATION_COLORS = {
    10: "#b48c14",
    100: "#dcb428",
    1000: "#f0dc78",
}
ERROR_COLORS = {
    10: "#b42828",
    100: "#dc5050",
    1000: "#f08c8c",
}


def normalize_header(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.strip().lower())


def resolve_columns(columns: list[str]) -> dict[str, str]:
    normalized_to_actual = {normalize_header(column): column for column in columns}
    resolved: dict[str, str] = {}

    for required in REQUIRED_COLUMNS:
        normalized = normalize_header(required)
        if normalized not in normalized_to_actual:
            raise ValueError(f"input CSV is missing a {required} column")
        resolved[required] = normalized_to_actual[normalized]

    return resolved


def parse_non_negative(value: str, field_name: str, row_number: int) -> float:
    text = value.strip()
    if text == "":
        raise ValueError(f"row {row_number} has an empty {field_name} value")
    try:
        number = float(text)
    except ValueError as error:
        raise ValueError(
            f"row {row_number} has a non-numeric {field_name} value"
        ) from error
    if number < 0:
        raise ValueError(f"row {row_number} has an invalid {field_name} value")
    return number


def load_use_metrics(input_path: Path) -> dict[str, list[float]]:
    if not input_path.is_file():
        raise FileNotFoundError(f"cannot read input file: {input_path}")

    with input_path.open(newline="") as input_file:
        reader = csv.reader(input_file)
        try:
            header = next(reader)
        except StopIteration as error:
            raise ValueError("input file is empty") from error

        resolved = resolve_columns(header)
        series = {column: [] for column in REQUIRED_COLUMNS}

        for row_number, row in enumerate(reader, start=2):
            if not row or all(field.strip() == "" for field in row):
                continue

            for column in REQUIRED_COLUMNS:
                column_index = header.index(resolved[column])
                if column_index >= len(row):
                    raise ValueError(f"row {row_number} has too few columns")
                series[column].append(
                    parse_non_negative(row[column_index], column, row_number)
                )

    if not series["time-step"]:
        raise ValueError("input file has no data rows")

    return series


def plot_metric(
    series: dict[str, list[float]],
    metric_prefix: str,
    title: str,
    y_label: str,
    colors: dict[int, str],
    output_path: Path,
    fill_under: bool = False,
) -> None:
    figure, axes = plt.subplots(figsize=(10, 5))
    time_steps = series["time-step"]

    for window_size in WINDOW_SIZES:
        column_name = f"{metric_prefix}({window_size})"
        values = series[column_name]
        color = colors[window_size]
        label = f"W{window_size}"

        if fill_under:
            axes.fill_between(time_steps, values, color=color, alpha=0.20)

        axes.plot(time_steps, values, color=color, linewidth=2.0, label=label)

    axes.set_title(title)
    axes.set_xlabel("Time Step")
    axes.set_ylabel(y_label)
    axes.grid(True, alpha=0.3)
    axes.legend(title="Window")
    figure.tight_layout()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def plot_use_metrics(input_path: Path, results_directory: Path) -> list[Path]:
    series = load_use_metrics(input_path)
    results_directory.mkdir(parents=True, exist_ok=True)

    outputs = [
        (
            "Utilization",
            "Utilization vs Time Step",
            "Utilization",
            UTILIZATION_COLORS,
            results_directory / "utilization.png",
            False,
        ),
        (
            "Saturation",
            "Saturation vs Time Step",
            "Saturation",
            SATURATION_COLORS,
            results_directory / "saturation.png",
            True,
        ),
        (
            "Errors",
            "Error Rate vs Time Step",
            "Error Rate",
            ERROR_COLORS,
            results_directory / "error-rate.png",
            False,
        ),
    ]

    written: list[Path] = []
    for metric_prefix, title, y_label, colors, output_path, fill_under in outputs:
        plot_metric(
            series,
            metric_prefix,
            title,
            y_label,
            colors,
            output_path,
            fill_under=fill_under,
        )
        written.append(output_path)

    return written


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="plot-use",
        description=(
            "Create three USE-method PNG plots (Utilization, Saturation, "
            "Error Rate) from a quse.csv file."
        ),
    )
    parser.add_argument("input_csv", help="USE-metrics CSV from compute-use")
    parser.add_argument(
        "results_dir",
        nargs="?",
        default=str(DEFAULT_RESULTS_DIRECTORY),
        help=(
            "Directory for the three PNG outputs "
            f"(default: {DEFAULT_RESULTS_DIRECTORY})"
        ),
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)

    try:
        written = plot_use_metrics(
            Path(arguments.input_csv),
            Path(arguments.results_dir),
        )
    except Exception as error:  # noqa: BLE001 - CLI should print any failure
        print(error, file=sys.stderr)
        return 1

    for path in written:
        print(f"Wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
