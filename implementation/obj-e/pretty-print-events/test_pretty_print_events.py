#!/usr/bin/env python3
"""Unit tests for pretty-print-events.py (standard library only).

Run:   python3 test_pretty_print_events.py
"""

import contextlib
import importlib.util
import io
import os
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "pretty_print_events", os.path.join(HERE, "pretty-print-events.py"))
ppe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ppe)

SAMPLE_LOG = """\
0 j0 IN q0 ARRIVE-VIA in0.wl
0 j0 IN q0 ENTERS-SERVER 0
0 j0 IN q0 TASK-START 0 "W 10"
0 j1 IN q0 ARRIVE-VIA in0.wl
0 j1 IN q0 ERROR QUEUE-FULL
10 j0 IN q0 TASK-STOP 0 "W 10"
10 j0 IN q0 EXITS-SERVER 0
10 j0 IN q0 DEPART-VIA q1
10 j0 IN q1 DEPART-VIA out0.wl
12 j2 IN q10 ARRIVE-VIA in1.wl
12 j3 IN q2 ARRIVE-VIA in2.wl
"""


class TempFiles(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = self._tmp.name

    def write(self, name, text):
        path = os.path.join(self.dir, name)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(text)
        return path


class ParsingTests(TempFiles):
    def test_parse_event_with_and_without_detail(self):
        event = ppe.parse_event_line('10 j0 IN q0 TASK-START 1 "W 10"')
        self.assertEqual((event.time, event.job, event.queue, event.kind),
                         (10, "j0", "q0", "TASK-START"))
        self.assertEqual(event.detail, '1 "W 10"')
        self.assertEqual(event.text(), 'j0 TASK-START 1 "W 10"')

        bare = ppe.parse_event_line("3 j9 IN q4 ERROR")
        self.assertEqual(bare.detail, "")
        self.assertEqual(bare.text(), "j9 ERROR")

    def test_parse_rejects_non_events(self):
        for line in ["", "hello world", "x j0 IN q0 ARRIVE-VIA a",
                     "5 j0 AT q0 ARRIVE-VIA a"]:
            self.assertIsNone(ppe.parse_event_line(line))

    def test_load_events_counts_skipped_lines(self):
        path = self.write("log", SAMPLE_LOG + "\ngarbage line\n")
        events, skipped = ppe.load_events(path)
        self.assertEqual(len(events), 11)
        self.assertEqual(skipped, 1)

    def test_load_events_missing_file(self):
        with self.assertRaises(OSError):
            ppe.load_events(os.path.join(self.dir, "nope.log"))

    def test_natural_key_and_split_list(self):
        self.assertEqual(sorted(["q10", "q2", "q1"], key=ppe.natural_key),
                         ["q1", "q2", "q10"])
        self.assertEqual(ppe.split_list("a, b,,c "), ["a", "b", "c"])
        self.assertEqual(ppe.split_list(""), [])


class SelectionTests(TempFiles):
    def setUp(self):
        super().setUp()
        self.events, _ = ppe.load_events(self.write("log", SAMPLE_LOG))

    def test_no_filters_keeps_everything(self):
        self.assertEqual(len(ppe.select_events(self.events)), 11)

    def test_filters(self):
        by_queue = ppe.select_events(self.events, queues=["q1"])
        self.assertEqual([e.queue for e in by_queue], ["q1"])
        by_job = ppe.select_events(self.events, jobs=["j1"])
        self.assertEqual(len(by_job), 2)
        by_kind = ppe.select_events(self.events, kinds=["ERROR"])
        self.assertEqual(len(by_kind), 1)
        window = ppe.select_events(self.events, time_from=10, time_to=10)
        self.assertEqual({e.time for e in window}, {10})
        after = ppe.select_events(self.events, time_from=11)
        self.assertEqual({e.time for e in after}, {12})
        before = ppe.select_events(self.events, time_to=0)
        self.assertEqual({e.time for e in before}, {0})

    def test_column_order(self):
        self.assertEqual(ppe.column_order(self.events),
                         ["q0", "q1", "q2", "q10"])           # natural order
        self.assertEqual(ppe.column_order(self.events, ["q10", "q0", "q10"]),
                         ["q10", "q0"])                       # requested order


class LayoutTests(TempFiles):
    def setUp(self):
        super().setUp()
        self.events, _ = ppe.load_events(self.write("log", SAMPLE_LOG))
        self.queues = ["q0", "q1"]

    def rows(self, **kwargs):
        chosen = ppe.select_events(self.events, queues=self.queues)
        return ppe.build_rows(chosen, self.queues, **kwargs)

    def test_time_label_only_on_first_row_of_a_time_step(self):
        labels = [label for label, _ in self.rows()]
        self.assertEqual(labels.count("0"), 1)
        self.assertEqual(labels.count("10"), 1)
        self.assertEqual(labels[0], "0")

    def test_events_land_in_their_queue_column(self):
        for _label, cells in self.rows():
            self.assertEqual(sum(cell is not None for cell in cells), 1
                             if cells[0] is None or cells[1] is None else 2)
        last = self.rows()[-1][1]
        self.assertEqual(last[1][0], "j0 DEPART-VIA out0.wl")   # q1 column
        self.assertIsNone(last[0])

    def test_packed_rows_share_a_row_across_jobs_and_queues(self):
        # j1's error (q0) is a different job than j0's server events, but they
        # are in the same queue, so they must stay in log order (no sharing).
        # j0 DEPART-VIA q1 (q0) then j0 DEPART-VIA out0.wl (q1): same job,
        # so the second must be on a LATER row.
        rows = self.rows()
        row_of = {}
        for index, (_label, cells) in enumerate(rows):
            for cell in cells:
                if cell:
                    row_of[cell[0]] = index
        self.assertLess(row_of["j0 DEPART-VIA q1"],
                        row_of["j0 DEPART-VIA out0.wl"])

    def test_packing_puts_different_jobs_in_different_queues_together(self):
        events = [
            ppe.parse_event_line("5 a IN q0 ARRIVE-VIA x"),
            ppe.parse_event_line("5 b IN q1 ARRIVE-VIA y"),
        ]
        packed = ppe.build_rows(events, ["q0", "q1"], pack=True)
        self.assertEqual(len(packed), 1)
        self.assertIsNotNone(packed[0][1][0])
        self.assertIsNotNone(packed[0][1][1])
        sequential = ppe.build_rows(events, ["q0", "q1"], pack=False)
        self.assertEqual(len(sequential), 2)
        self.assertEqual([label for label, _ in sequential], ["5", ""])

    def test_same_queue_events_keep_log_order(self):
        events = [
            ppe.parse_event_line("5 a IN q0 ARRIVE-VIA x"),
            ppe.parse_event_line("5 b IN q0 ARRIVE-VIA x"),
        ]
        rows = ppe.build_rows(events, ["q0"], pack=True)
        self.assertEqual([cells[0][0] for _l, cells in rows],
                         ["a ARRIVE-VIA x", "b ARRIVE-VIA x"])

    def test_events_of_unlisted_queues_are_ignored(self):
        events = [ppe.parse_event_line("5 a IN q7 ARRIVE-VIA x")]
        self.assertEqual(ppe.build_rows(events, ["q0"]), [])

    def test_fit_and_widths(self):
        self.assertEqual(ppe.fit("abcdef", 6), "abcdef")
        self.assertEqual(ppe.fit("abcdefg", 6), "abcde~")
        rows = self.rows()
        widths = ppe.column_widths(rows, self.queues, max_width=100)
        self.assertGreaterEqual(widths[0], len("j0 ENTERS-SERVER 0"))
        capped = ppe.column_widths(rows, self.queues, max_width=5)
        self.assertEqual(capped, [5, 5])

    def test_format_table_plain(self):
        lines = ppe.format_table(self.rows(), self.queues, 30, color=False)
        self.assertTrue(lines[0].startswith("time"))
        self.assertIn("q0", lines[0])
        self.assertEqual(set(lines[1]), {"-"})
        self.assertFalse(any("\033" in line for line in lines))
        self.assertTrue(any("j0 ENTERS-SERVER 0" in line for line in lines))

    def test_format_table_truncates_long_cells(self):
        lines = ppe.format_table(self.rows(), self.queues, 8, color=False)
        self.assertTrue(any("~" in line for line in lines))

    def test_format_table_colour_uses_ansi_per_event_type(self):
        lines = ppe.format_table(self.rows(), self.queues, 30, color=True)
        text = "\n".join(lines)
        self.assertIn("\033[32m", text)      # ARRIVE-VIA green
        self.assertIn("\033[1;31m", text)    # ERROR bold red
        self.assertIn(ppe.RESET, text)

    def test_unknown_event_type_gets_default_colour(self):
        self.assertEqual(ppe.paint("x", "WEIRD", True),
                         f"\033[{ppe.UNKNOWN_STYLE}mx{ppe.RESET}")
        self.assertEqual(ppe.paint("x", "WEIRD", False), "x")

    def test_tsv(self):
        lines = ppe.format_tsv(self.rows(), self.queues)
        self.assertEqual(lines[0], "time\tq0\tq1")
        self.assertEqual(lines[1].split("\t")[0], "0")
        self.assertTrue(all(line.count("\t") == 2 for line in lines))

    def test_legend_lists_every_event_type_once(self):
        plain = "\n".join(ppe.format_legend(False))
        self.assertIn("ARRIVE-VIA", plain)
        self.assertIn("ERROR", plain)
        self.assertEqual(plain.count("TASK-STOP"), 1)
        self.assertNotIn("TASK-END", plain)
        self.assertIn("\033[", "\n".join(ppe.format_legend(True)))


class CommandLineTests(TempFiles):
    def run_main(self, *argv, tty=False):
        out = io.StringIO()
        out.isatty = lambda: tty
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = ppe.main(list(argv), stdout=out)
        return code, out.getvalue(), err.getvalue()

    def setUp(self):
        super().setUp()
        self.log = self.write("job.log", SAMPLE_LOG)

    def test_default_run(self):
        code, out, err = self.run_main(self.log, "--color", "never")
        self.assertEqual(code, 0)
        self.assertIn("q10", out.splitlines()[0])
        self.assertEqual(err, "")

    def test_default_uses_stdout_argument_of_sys_stdout(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(ppe.main([self.log, "--color", "never"]), 0)
        self.assertIn("time", out.getvalue())

    def test_color_modes(self):
        _c, always, _e = self.run_main(self.log, "--color", "always")
        self.assertIn("\033[", always)
        _c, never, _e = self.run_main(self.log, "--color", "never", tty=True)
        self.assertNotIn("\033[", never)
        _c, auto_tty, _e = self.run_main(self.log, tty=True)
        self.assertIn("\033[", auto_tty)
        _c, auto_pipe, _e = self.run_main(self.log, tty=False)
        self.assertNotIn("\033[", auto_pipe)

    def test_filters_and_sequential(self):
        code, out, _ = self.run_main(self.log, "--jobs", "j0", "--queues",
                                     "q0,q1", "--from", "10", "--to", "10",
                                     "--sequential", "--color", "never")
        self.assertEqual(code, 0)
        body = out.splitlines()[2:]
        self.assertEqual(len(body), 4)
        code, out, _ = self.run_main(self.log, "--events", "ERROR",
                                     "--color", "never")
        self.assertIn("QUEUE-FULL", out)
        self.assertNotIn("ARRIVE-VIA", out)

    def test_legend(self):
        _c, out, _e = self.run_main(self.log, "--legend", "--color", "never")
        self.assertIn("legend:", out)

    def test_tsv_file_is_written(self):
        target = os.path.join(self.dir, "out.tsv")
        code, _o, _e = self.run_main(self.log, "--tsv", target, "--color",
                                     "never")
        self.assertEqual(code, 0)
        with open(target, encoding="utf-8") as handle:
            first = handle.readline().rstrip("\n")
        self.assertEqual(first, "time\tq0\tq1\tq2\tq10")

    def test_tsv_write_failure(self):
        code, _o, err = self.run_main(self.log, "--tsv", self.dir)  # a folder
        self.assertEqual(code, 1)
        self.assertIn("pretty-print-events:", err)

    def test_missing_input_file(self):
        code, _o, err = self.run_main(os.path.join(self.dir, "nope.log"))
        self.assertEqual(code, 1)
        self.assertIn("pretty-print-events:", err)

    def test_from_greater_than_to(self):
        code, _o, err = self.run_main(self.log, "--from", "5", "--to", "2")
        self.assertEqual(code, 1)
        self.assertIn("--from", err)

    def test_no_matching_events(self):
        code, out, err = self.run_main(self.log, "--jobs", "nobody")
        self.assertEqual(code, 0)
        self.assertEqual(out, "")
        self.assertIn("no events", err)

    def test_skipped_lines_are_reported(self):
        path = self.write("dirty.log", SAMPLE_LOG + "not an event\n")
        code, _o, err = self.run_main(path, "--color", "never")
        self.assertEqual(code, 0)
        self.assertIn("skipped 1 line", err)

    def test_col_width_is_applied(self):
        code, out, _err = self.run_main(self.log, "--col-width", "6",
                                        "--color", "never")
        self.assertEqual(code, 0)
        self.assertIn("~", out)

    def test_bad_numbers_are_rejected_by_argparse(self):
        for args in (["--from", "-1"], ["--col-width", "0"]):
            with self.assertRaises(SystemExit) as raised:
                with contextlib.redirect_stderr(io.StringIO()):
                    ppe.main([self.log] + args)
            self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()