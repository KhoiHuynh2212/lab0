"""Check input adaptations, customer populations and time/summary boundaries."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SIMPLE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SIMPLE / "scripts"))
from analysis_helpers import adapt_events, adapt_metrics, check_derived, check_use, paired_comparisons, summarize
from prepare_workloads import write_csv
from validate_carwash import read_csv


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        build = (SIMPLE / ".build").resolve()
        build.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="analysis-test-", dir=build)
        self.folder = Path(self.temp.name).resolve()
        self.assertTrue(self.folder.is_relative_to(build))
        self.addCleanup(self.temp.cleanup)

    def metrics(self, errors=(1, 1, 2), busy=(1, 1, 0)):
        path = self.folder / "raw.csv"
        rows = [{"time-step": str(t), "queue-sys-id": "q0", "num-jobs-in-queue": "0",
                 "num-jobs-in-service": str(b), "num-of-errors": str(e)} for t, (e, b) in enumerate(zip(errors, busy))]
        write_csv(path, list(rows[0]), rows)
        return path

    def test_error_differences_and_idle_tail_preserve_input(self):
        source = self.metrics()
        before = source.read_bytes()
        target = self.folder / "adapted.csv"
        result = adapt_metrics(source, target, 5)
        self.assertEqual([r["num-of-errors"] for r in read_csv(target)], ["1", "0", "1", "0", "0"])
        self.assertEqual(result["idle_rows_appended"], 2)
        self.assertEqual(result["new_errors_total"], 2)
        self.assertEqual(source.read_bytes(), before)

    def test_decreasing_cumulative_counter_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "decreased"):
            adapt_metrics(self.metrics(errors=(1, 0, 0)), self.folder / "out.csv", 5)

    def test_unfinished_system_does_not_get_an_invented_idle_tail(self):
        with self.assertRaisesRegex(RuntimeError, "unfinished"):
            adapt_metrics(self.metrics(busy=(1, 1, 1)), self.folder / "out.csv", 5)

    def test_adapter_cannot_overwrite_its_source(self):
        source = self.metrics()
        before = source.read_bytes()
        with self.assertRaisesRegex(RuntimeError, "overwrite"):
            adapt_metrics(source, source, 5)
        self.assertEqual(source.read_bytes(), before)

    def test_event_adapter_preserves_all_events_and_only_maps_known_boundaries(self):
        source = self.folder / "events.log"
        text = '0 j0 IN q0 ARRIVE-VIA source.csv\n0 j0 IN q0 TASK-START 0 "W 5"\n34 j0 IN q0 DEPART-VIA completed.csv\n'
        source.write_text(text)
        out = self.folder / "adapted.log"
        report = adapt_events(source, out, "source.csv", "completed.csv")
        lines = out.read_text().splitlines()
        self.assertEqual(len(lines), 4)
        self.assertEqual(lines[1], "0 j0 IN q0 ARRIVE-VIA source.wl")
        self.assertEqual(lines[2], text.splitlines()[1])
        self.assertEqual(lines[3], "34 j0 IN q0 DEPART-VIA sink.wl")
        self.assertEqual(report["aliases"]["source.wl"], "source.csv")
        self.assertEqual(source.read_text(), text)

    def test_unknown_edge_is_not_relabelled_as_a_source(self):
        source = self.folder / "events.log"
        source.write_text("0 j0 IN q0 ARRIVE-VIA network-edge\n")
        with self.assertRaisesRegex(RuntimeError, "boundary"):
            adapt_events(source, self.folder / "out.log", "source.csv", "completed.csv")

    def test_filter_input_adds_header_without_changing_boundary_labels(self):
        source = self.folder / "events.log"
        original = "0 j0 IN q0 ARRIVE-VIA source.csv\n1 j1 IN q0 ARRIVE-VIA source.csv\n"
        source.write_text(original)
        target = self.folder / "filter.log"
        result = adapt_events(source, target, "source.csv", "completed.csv", aliases=False)
        self.assertEqual(target.read_text(), "time job IN queue event detail\n" + original)
        self.assertEqual(result["aliases"], {})

    def test_use_check_rejects_wrong_window_errors(self):
        source = self.metrics(errors=(1, 1, 2), busy=(0, 0, 0))
        adapted = self.folder / "adapted.csv"
        adapt_metrics(source, adapted, 3)
        values = []
        for t, total in enumerate([1, 1, 2]):
            row = {"time-step": t}
            for w in [10, 100, 1000]:
                row.update({f"Utilization({w})": 0, f"Saturation({w})": 0, f"Errors({w})": total})
            values.append(row)
        out = self.folder / "use.csv"
        write_csv(out, list(values[0]), values)
        self.assertEqual(check_use(adapted, out, "test", 3)[1], 3)
        values[-1]["Errors(10)"] = 4
        write_csv(out, list(values[0]), values)
        with self.assertRaisesRegex(RuntimeError, "contract changed"):
            check_use(adapted, out, "test", 3)

    def test_completed_population_and_half_open_cutoff(self):
        row = {"run_id": "test", "family": "baseline", "demand": "light", "seed": "42",
               "arrival_horizon": "68", "queue_size": "20", "num_servers": "1"}
        original = {"offered": 3, "completed": 2, "rejected": 1, "rejection_fraction": "0.33333333",
                    "completed_before_cutoff": 1, "unfinished_at_cutoff": 1, "last_departure": 68, "maximum_waiting": 1}
        events = [{"outcome": "completed", "response-time": "0", "turn-around-time": "34"},
                  {"outcome": "completed", "response-time": "30", "turn-around-time": "64"},
                  {"outcome": "rejected", "response-time": "", "turn-around-time": "0"}]
        result = summarize(row, original, events, [{"time-step": "34", "throughput": "1"}, {"time-step": "68", "throughput": "1"}])
        self.assertEqual(result["mean_wait_minutes"], "2.500000")
        self.assertEqual(result["mean_total_minutes"], "8.166667")
        self.assertAlmostEqual(float(result["arrival_window_throughput_cars_per_hour"]), 360 / 68, places=6)

    def test_derived_timing_is_checked_against_original_events(self):
        arrivals, waits = [0, 4, 9, 10, 15, 22], [0, 30, 59, 92, 121, 148]
        events, system = [], []
        for i, (arrival, wait) in enumerate(zip(arrivals, waits)):
            events.append({"job-id": f"j{i}", "queue-sys-id": "q0", "arrival-time": arrival,
                           "time-in-queue": wait, "time-in-service": 34, "response-time": wait, "turn-around-time": wait + 34})
            system.append({"time-step": 34 * (i + 1), "Avg-response-time": wait + 34, "median-response-time": wait + 34,
                           "10-response-time": wait + 34, "90-response-time": wait + 34, "throughput": 1})
        event_path, system_path = self.folder / "events.csv", self.folder / "system.csv"
        write_csv(event_path, list(events[0]), events)
        write_csv(system_path, list(system[0]), system)
        args = (event_path, system_path, SIMPLE / "runs/smoke", SIMPLE / "workloads/smoke.csv",
                {"offered": 6, "completed": 6, "rejected": 0})
        checked, _ = check_derived(*args)
        self.assertTrue(all(r["outcome"] == "completed" for r in checked))
        events[1]["response-time"] = 29
        write_csv(event_path, list(events[0]), events)
        with self.assertRaisesRegex(RuntimeError, "timing mismatch"):
            check_derived(*args)

    def test_system_output_cannot_silently_drop_a_departure(self):
        # The empty system output produced by an unadapted C log must never pass validation.
        arrivals, waits = [0, 4, 9, 10, 15, 22], [0, 30, 59, 92, 121, 148]
        events = [{"job-id": f"j{i}", "queue-sys-id": "q0", "arrival-time": arrival,
                   "time-in-queue": wait, "time-in-service": 34, "response-time": wait, "turn-around-time": wait + 34}
                  for i, (arrival, wait) in enumerate(zip(arrivals, waits))]
        event_path, system_path = self.folder / "events.csv", self.folder / "system.csv"
        write_csv(event_path, list(events[0]), events)
        system_path.write_text("time-step,Avg-response-time,median-response-time,10-response-time,90-response-time,throughput\n")
        with self.assertRaisesRegex(RuntimeError, "departure timestamps"):
            check_derived(event_path, system_path, SIMPLE / "runs/smoke", SIMPLE / "workloads/smoke.csv",
                          {"offered": 6, "completed": 6, "rejected": 0})

    def test_zero_baseline_percentage_change_is_undefined(self):
        rows = [{"run_id": "variant", "baseline_run_id": "base", "family": "additional-bay", "demand": "light",
                 "seed": "42", "changed_resource": "bays", "resource_change_percent": "100"}]
        fields = {"mean_wait_minutes": "0", "p90_wait_minutes": "0", "mean_total_minutes": "5.666667",
                  "arrival_window_throughput_cars_per_hour": "3", "rejected": "0", "offered": "10"}
        result = paired_comparisons(rows, [{"run_id": "base", **fields}, {"run_id": "variant", **fields}])[0]
        self.assertEqual(result["percent_change_mean_wait_minutes"], "")
        self.assertEqual(result["delta_mean_wait_minutes"], "0.000000")

    def test_existing_analysis_output_is_preserved_before_building(self):
        output = self.folder / "output"
        output.mkdir()
        sentinel = output / "keep.txt"
        sentinel.write_text("keep")
        result = subprocess.run([sys.executable, str(SIMPLE / "scripts/analyze_results.py"), "--output-root", str(output)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("refusing to overwrite", result.stderr)
        self.assertEqual(sentinel.read_text(), "keep")
        self.assertEqual(list(output.iterdir()), [sentinel])


if __name__ == "__main__":
    unittest.main()
