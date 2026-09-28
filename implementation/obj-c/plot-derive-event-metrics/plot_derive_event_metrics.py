#!/usr/bin/env python3
"""Obj-C plot-derive-event-metrics: histogram or scatter plot of emetric.csv columns."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def normalize_header(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.strip().lower())


def load_columns(input_path: Path, wanted: list[str]) -> dict[str, list[float]]:
    """Read the wanted columns as floats, skipping rows with an empty cell."""
    if not input_path.is_file():
        raise FileNotFoundError(f"cannot read input file: {input_path}")

    with input_path.open(newline="") as input_file:
        reader = csv.reader(input_file)
        try:
            header = next(reader)
        except StopIteration as error:
            raise ValueError("input file is empty") from error

        normalized_to_index = {normalize_header(name): index for index, name in enumerate(header)}
        indexes = []
        for name in wanted:
            key = normalize_header(name)
            if key not in normalized_to_index:
                raise ValueError(f"input CSV is missing a {name} column")
            indexes.append(normalized_to_index[key])

        columns: dict[str, list[float]] = {name: [] for name in wanted}
        for row_number, row in enumerate(reader, start=2):
            if not row or all(field.strip() == "" for field in row):
                continue
            if max(indexes) >= len(row):
                raise ValueError(f"row {row_number} has too few columns")

            values = [row[index].strip() for index in indexes]
            if any(value == "" for value in values):
                continue

            try:
                numbers = [float(value) for value in values]
            except ValueError as error:
                raise ValueError(f"row {row_number} has a non-numeric value") from error

            for name, number in zip(wanted, numbers):
                columns[name].append(number)

    if not columns[wanted[0]]:
        raise ValueError("input file has no usable data rows")

    return columns


def parse_limits(text: str | None) -> tuple[float, float] | None:
    if text is None:
        return None
    parts = text.split(",")
    if len(parts) != 2:
        raise ValueError("limits must look like low,high")
    return float(parts[0]), float(parts[1])


def plot_histogram(input_path: Path, column: str, bins: int, arguments) -> None:
    data = load_columns(input_path, [column])[column]

    figure, axes = plt.subplots(figsize=(10, 5))
    axes.hist(data, bins=bins, color="#468cdc", edgecolor="black")
    axes.set_xlabel(column)
    axes.set_ylabel("frequency")
    finish_plot(figure, axes, arguments, f"Histogram of {column}")


def plot_scatter(input_path: Path, x_column: str, y_column: str, arguments) -> None:
    data = load_columns(input_path, [x_column, y_column])

    figure, axes = plt.subplots(figsize=(10, 5))
    axes.scatter(data[x_column], data[y_column], color="#1e5ab4", s=20)
    axes.set_xlabel(x_column)
    axes.set_ylabel(y_column)
    finish_plot(figure, axes, arguments, f"{y_column} vs {x_column}")


def finish_plot(figure, axes, arguments, default_title: str) -> None:
    axes.set_title(arguments.title or default_title)
    axes.grid(True, alpha=0.3)

    x_limits = parse_limits(arguments.xlim)
    if x_limits:
        axes.set_xlim(x_limits)
    y_limits = parse_limits(arguments.ylim)
    if y_limits:
        axes.set_ylim(y_limits)

    output_path = Path(arguments.output_png)
    figure.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="plot-derive-event-metrics",
        description="Plot a histogram or scatter plot from an emetric.csv file.",
    )
    parser.add_argument("input_csv", help="derived event metrics CSV")
    parser.add_argument("output_png", help="PNG file to write")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--hist", metavar="COLUMN", help='column to histogram, e.g. "arrival-time"')
    mode.add_argument("--scat", metavar="X,Y", help='two columns to scatter, e.g. "arrival-time,response-time"')
    parser.add_argument("--bins", type=int, default=10, help="histogram bin count (default 10)")
    parser.add_argument("--title", help="plot title")
    parser.add_argument("--xlim", metavar="LOW,HIGH", help="x-axis limits")
    parser.add_argument("--ylim", metavar="LOW,HIGH", help="y-axis limits")
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)

    try:
        input_path = Path(arguments.input_csv)
        if arguments.hist:
            if arguments.bins <= 0:
                raise ValueError("bins must be a positive number")
            plot_histogram(input_path, arguments.hist.strip(), arguments.bins, arguments)
        else:
            parts = [part.strip() for part in arguments.scat.split(",")]
            if len(parts) != 2 or not all(parts):
                raise ValueError("--scat needs exactly two column names: X,Y")
            plot_scatter(input_path, parts[0], parts[1], arguments)
    except Exception as error:
        print(error, file=sys.stderr)
        return 1

    print(f"Wrote {arguments.output_png}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
