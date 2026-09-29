#!/usr/bin/env python3
"""
pretty-print-events.py  (Objective E)

Lay out an event log (job.log, written by simple-queue-sim / multi-queue-sim)
as a table that is easy to read:

  * one column per queue-system,
  * rows in time order (events of one time-step that happen in different
    queue systems share a row; a queue with several events in the same
    time-step gets extra rows; the time is printed only on the first),
  * colour marks the event type (ARRIVE, ENTERS-SERVER, TASK-START, ...).

The layout follows the "debugging pretty" idea from
https://blog.josejg.com/debugging-pretty/ (one column per component, time down
the side) using only the Python standard library.

Event log line format:
    <time> <job-id> IN <qsid> <EVENT> [args...]
e.g.
    10 j0 IN q0 TASK-START 1 "W 10"
    20 j0 IN q3 DEPART-VIA out0.wl

Usage:
    ./pretty-print-events.py job.log
    ./pretty-print-events.py job.log --jobs j0,j1 --queues q0,q1,q47
    ./pretty-print-events.py job.log --from 0 --to 50 --legend
    ./pretty-print-events.py job.log --tsv out.tsv --color never
"""

import argparse
import re
import sys

# --------------------------------------------------------------------------
# Event model
# --------------------------------------------------------------------------

LINE_PATTERN = re.compile(r"^(\d+)\s+(\S+)\s+IN\s+(\S+)\s+(\S+)(?:\s+(.*))?$")

# Event types mapped to an ANSI colour (SGR code) and a short description.
# TASK-END is accepted as a synonym for TASK-STOP (the lab handout uses both).
EVENT_STYLES = {
    "ARRIVE-VIA": ("32", "job enters a queue system"),
    "ENTERS-SERVER": ("36", "job starts on a server"),
    "TASK-START": ("34", "a task starts"),
    "TASK-STOP": ("35", "a task stops"),
    "TASK-END": ("35", "a task stops"),
    "EXITS-SERVER": ("33", "job leaves a server"),
    "DEPART-VIA": ("92", "job leaves the queue system"),
    "ERROR": ("1;31", "job dropped (QUEUE-FULL / NO-OUT-EDGE)"),
}
UNKNOWN_STYLE = "37"
RESET = "\033[0m"


class Event:
    """One parsed log line."""

    __slots__ = ("time", "job", "queue", "kind", "detail")

    def __init__(self, time, job, queue, kind, detail):
        self.time = time
        self.job = job
        self.queue = queue
        self.kind = kind
        self.detail = detail

    def text(self):
        """Cell text: '<job> <EVENT> <detail>'."""
        parts = [self.job, self.kind]
        if self.detail:
            parts.append(self.detail)
        return " ".join(parts)


def parse_event_line(line):
    """Parse one log line. Returns an Event, or None if it is not an event."""
    match = LINE_PATTERN.match(line.strip())
    if not match:
        return None
    time, job, queue, kind, detail = match.groups()
    return Event(int(time), job, queue, kind, (detail or "").strip())


def load_events(path):
    """Read a log file. Returns (events, number_of_unparseable_lines)."""
    events = []
    skipped = 0
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.strip():
                continue
            event = parse_event_line(line)
            if event is None:
                skipped += 1
            else:
                events.append(event)
    return events, skipped


# --------------------------------------------------------------------------
# Selecting and ordering
# --------------------------------------------------------------------------

def natural_key(text):
    """Sort key so that q2 comes before q10."""
    return [int(part) if part.isdigit() else part
            for part in re.split(r"(\d+)", text)]


def split_list(text):
    """'a,b, c' -> ['a', 'b', 'c'] (empty items dropped)."""
    if not text:
        return []
    return [item.strip() for item in text.split(",") if item.strip()]


def select_events(events, queues=None, jobs=None, time_from=None,
                  time_to=None, kinds=None):
    """Keep only events matching every given filter (None = no filter).
    The original log order is preserved."""
    queue_set = set(queues) if queues else None
    job_set = set(jobs) if jobs else None
    kind_set = set(kinds) if kinds else None
    selected = []
    for event in events:
        if queue_set is not None and event.queue not in queue_set:
            continue
        if job_set is not None and event.job not in job_set:
            continue
        if kind_set is not None and event.kind not in kind_set:
            continue
        if time_from is not None and event.time < time_from:
            continue
        if time_to is not None and event.time > time_to:
            continue
        selected.append(event)
    return selected


def column_order(events, requested_queues=None):
    """Queue ids for the columns. If --queues was given, keep that order;
    otherwise use every queue seen, in natural order."""
    if requested_queues:
        return list(dict.fromkeys(requested_queues))
    return sorted({event.queue for event in events}, key=natural_key)


# --------------------------------------------------------------------------
# Layout
# --------------------------------------------------------------------------

def build_rows(events, queue_ids, pack=True):
    """Return rows as (time_label, cells) pairs; cells has one entry per queue:
    None or an (text, kind) tuple.

    time_label is the time on the first row of a time-step and '' on the
    following rows of the same time-step.

    pack=True (default): events of one time-step share rows where they can:
    events in different queue systems go in the same row, unless they belong
    to the same job (one job's events always read top to bottom, in log
    order) or the same queue (a queue's events also stay in log order).
    Extra rows are added only when needed.
    pack=False: one event per row."""
    column_of = {queue: index for index, queue in enumerate(queue_ids)}
    rows = []
    group = []          # events of the current time-step
    current_time = None

    def flush():
        if not group:
            return
        if pack:
            depth = [0] * len(queue_ids)     # next free row in each column
            last_row_of_job = {}             # keeps one job's events in order
            group_rows = []
            for event in group:
                column = column_of[event.queue]
                row_index = max(depth[column],
                                last_row_of_job.get(event.job, -1) + 1)
                while len(group_rows) <= row_index:
                    group_rows.append([None] * len(queue_ids))
                group_rows[row_index][column] = (event.text(), event.kind)
                depth[column] = row_index + 1
                last_row_of_job[event.job] = row_index
        else:
            group_rows = []
            for event in group:
                cells = [None] * len(queue_ids)
                cells[column_of[event.queue]] = (event.text(), event.kind)
                group_rows.append(cells)
        for index, cells in enumerate(group_rows):
            rows.append((str(current_time) if index == 0 else "", cells))
        group.clear()

    for event in events:
        if event.queue not in column_of:
            continue
        if event.time != current_time:
            flush()
            current_time = event.time
        group.append(event)
    flush()
    return rows


def fit(text, width):
    """Cut text to width characters, marking the cut with '~'."""
    if len(text) <= width:
        return text
    return text[:width - 1] + "~"


def column_widths(rows, queue_ids, max_width):
    """Width of each queue column: as wide as its longest cell (and its
    header), but never more than max_width."""
    widths = [len(queue) for queue in queue_ids]
    for _label, cells in rows:
        for index, cell in enumerate(cells):
            if cell is not None:
                widths[index] = max(widths[index], len(cell[0]))
    return [min(width, max_width) for width in widths]


def paint(text, kind, color):
    if not color:
        return text
    style = EVENT_STYLES.get(kind, (UNKNOWN_STYLE, ""))[0]
    return f"\033[{style}m{text}{RESET}"


def format_table(rows, queue_ids, max_width=30, color=False):
    """Return the table as a list of text lines (with ANSI colour if asked)."""
    widths = column_widths(rows, queue_ids, max_width)
    time_width = max([len("time")] + [len(label) for label, _ in rows])

    def join(time_text, cell_texts):
        return "  ".join([time_text.rjust(time_width)] + cell_texts).rstrip()

    lines = [join("time", [queue.ljust(width)
                           for queue, width in zip(queue_ids, widths)])]
    lines.append("-" * len(lines[0]))
    for label, cells in rows:
        cell_texts = []
        for cell, width in zip(cells, widths):
            if cell is None:
                cell_texts.append(" " * width)
            else:
                shown = fit(cell[0], width)
                # Pad first so the invisible colour codes do not shift columns.
                cell_texts.append(paint(shown.ljust(width), cell[1], color))
        lines.append(join(label, cell_texts))
    return lines


def format_tsv(rows, queue_ids):
    """Tab-separated version of the same layout (no colour, no truncation)."""
    lines = ["\t".join(["time"] + list(queue_ids))]
    for label, cells in rows:
        fields = [label] + [cell[0] if cell is not None else ""
                            for cell in cells]
        lines.append("\t".join(fields))
    return lines


def format_legend(color):
    """One line per event type, painted in its own colour."""
    lines = ["legend:"]
    for kind, (_style, description) in EVENT_STYLES.items():
        if kind == "TASK-END":
            continue  # same look as TASK-STOP
        lines.append("  " + paint(kind.ljust(14), kind, color) + description)
    return lines


# --------------------------------------------------------------------------
# Command line
# --------------------------------------------------------------------------

def nonnegative_int(text):
    value = int(text)
    if value < 0:
        raise argparse.ArgumentTypeError("must be >= 0")
    return value


def positive_int(text):
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return value


def build_parser():
    p = argparse.ArgumentParser(
        description="Pretty-print a queue-simulator event log: rows are "
                    "time-steps, columns are queue systems, colour is the "
                    "event type.")
    p.add_argument("event_log", help="event log written by the simulator (job.log)")
    p.add_argument("--queues", default="",
                   help="comma-separated queue ids to show (default: all)")
    p.add_argument("--jobs", default="",
                   help="comma-separated job ids to show (default: all)")
    p.add_argument("--events", default="",
                   help="comma-separated event types to show, e.g. "
                        "ARRIVE-VIA,ERROR (default: all)")
    p.add_argument("--from", dest="time_from", type=nonnegative_int, default=None,
                   help="first time-step to show")
    p.add_argument("--to", dest="time_to", type=nonnegative_int, default=None,
                   help="last time-step to show")
    p.add_argument("--col-width", type=positive_int, default=30,
                   help="maximum width of a queue column (default: 30)")
    p.add_argument("--color", choices=["auto", "always", "never"], default="auto",
                   help="colour the event types (auto = only on a terminal)")
    p.add_argument("--sequential", action="store_true",
                   help="one event per row (default: events of a time-step "
                        "that are in different queues share a row)")
    p.add_argument("--legend", action="store_true",
                   help="print the colour legend after the table")
    p.add_argument("--tsv", metavar="FILE", default=None,
                   help="also write the table as tab-separated values to FILE")
    return p


def use_color(mode, stream):
    if mode == "always":
        return True
    if mode == "never":
        return False
    return stream.isatty()


def main(argv=None, stdout=None):
    stdout = stdout if stdout is not None else sys.stdout
    args = build_parser().parse_args(argv)

    try:
        events, skipped = load_events(args.event_log)
    except OSError as error:
        print(f"pretty-print-events: {error}", file=sys.stderr)
        return 1

    if (args.time_from is not None and args.time_to is not None
            and args.time_from > args.time_to):
        print("pretty-print-events: --from must not be greater than --to",
              file=sys.stderr)
        return 1

    requested_queues = split_list(args.queues)
    selected = select_events(events,
                             queues=requested_queues,
                             jobs=split_list(args.jobs),
                             time_from=args.time_from,
                             time_to=args.time_to,
                             kinds=split_list(args.events))
    queue_ids = column_order(selected, requested_queues)
    rows = build_rows(selected, queue_ids, pack=not args.sequential)

    if skipped:
        print(f"pretty-print-events: skipped {skipped} line(s) that are not "
              f"events", file=sys.stderr)

    if not rows:
        print("pretty-print-events: no events to show", file=sys.stderr)
        return 0

    color = use_color(args.color, stdout)
    for line in format_table(rows, queue_ids, args.col_width, color):
        print(line, file=stdout)
    if args.legend:
        print(file=stdout)
        for line in format_legend(color):
            print(line, file=stdout)

    if args.tsv:
        try:
            with open(args.tsv, "w", encoding="utf-8", newline="") as handle:
                handle.write("\n".join(format_tsv(rows, queue_ids)) + "\n")
        except OSError as error:
            print(f"pretty-print-events: {error}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())