"""Check the accounting verifier against known fixtures and deliberately damaged copies."""

import csv
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


SIMPLE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SIMPLE / "scripts"))
from run_experiments import load_manifest, verify_run  # noqa: E402
import run_experiments as runner  # noqa: E402
from validate_carwash import read_csv, sha256  # noqa: E402


class ExperimentVerificationTests(unittest.TestCase):
    def setUp(self):
        build = (SIMPLE / ".build").resolve()
        build.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="accounting-test-", dir=build)
        self.scratch = Path(self.temporary.name).resolve()
        self.assertTrue(self.scratch.is_relative_to(build))
        self.addCleanup(self.temporary.cleanup)

    def fixture(self, name="smoke", horizon=2880):
        folder = self.scratch / name
        folder.mkdir()
        for filename in ["completed.csv", "job.log", "qmet.csv"]:
            shutil.copyfile(SIMPLE / "runs" / name / filename, folder / filename)
        row = {"run_id": name, "family": "baseline", "workload_id": name,
               "demand": "light", "seed": "42", "arrival_horizon": str(horizon),
               "queue_size": "20", "num_servers": "1"}
        return folder, row, read_csv(SIMPLE / "workloads" / f"{name}.csv")

    @staticmethod
    def write_rows(path, rows):
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)

    def test_valid_smoke_and_idle_tail(self):
        summary, _ = verify_run(*self.fixture())
        self.assertEqual((summary["offered"], summary["completed"], summary["rejected"]), (6, 6, 0))
        self.assertEqual(summary["last_departure"], 204)
        self.assertEqual(summary["unfinished_at_cutoff"], 0)

    def test_valid_overflow(self):
        summary, _ = verify_run(*self.fixture("overflow"))
        self.assertEqual((summary["offered"], summary["completed"], summary["rejected"]), (22, 21, 1))
        self.assertEqual(summary["maximum_waiting"], 20)

    def test_departure_exactly_at_cutoff_is_outside_half_open_window(self):
        summary, _ = verify_run(*self.fixture(horizon=68))
        self.assertEqual(summary["completed_before_cutoff"], 1)
        self.assertEqual(summary["unfinished_at_cutoff"], 5)
        self.assertEqual((summary["waiting_at_cutoff"], summary["busy_at_cutoff"]), (4, 1))

    def test_duplicate_sink_car_is_rejected(self):
        folder, row, source = self.fixture()
        sink = read_csv(folder / "completed.csv")
        self.write_rows(folder / "completed.csv", sink + [sink[0]])
        with self.assertRaisesRegex(RuntimeError, "Sink count mismatch"):
            verify_run(folder, row, source)

    def test_incorrect_cumulative_error_is_rejected(self):
        folder, row, source = self.fixture()
        metrics = read_csv(folder / "qmet.csv")
        metrics[-1]["num-of-errors"] = "1"
        self.write_rows(folder / "qmet.csv", metrics)
        with self.assertRaisesRegex(RuntimeError, "conservation mismatch"):
            verify_run(folder, row, source)

    def test_in_bounds_but_incorrect_queue_count_is_rejected(self):
        folder, row, source = self.fixture()
        metrics = read_csv(folder / "qmet.csv")
        metrics[4]["num-jobs-in-queue"] = "2"
        self.write_rows(folder / "qmet.csv", metrics)
        with self.assertRaisesRegex(RuntimeError, "conservation mismatch"):
            verify_run(folder, row, source)

    def test_unexpected_error_type_is_rejected(self):
        folder, row, source = self.fixture("overflow")
        log = folder / "job.log"
        log.write_text(log.read_text().replace("QUEUE-FULL", "NO-OUT-EDGE"), encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Unexpected or duplicate rejection"):
            verify_run(folder, row, source)

    def test_missing_task_event_is_rejected(self):
        folder, row, source = self.fixture()
        log = folder / "job.log"
        lines = log.read_text().splitlines()
        del lines[next(i for i, line in enumerate(lines) if "TASK-STOP" in line)]
        log.write_text("\n".join(lines) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Missing operation events"):
            verify_run(folder, row, source)

    def test_wrong_input_hash_is_rejected_before_running(self):
        rows = read_csv(SIMPLE / "experiments.csv")
        for row in rows:
            row["workload_file"] = str(SIMPLE / row["workload_file"])
        rows[0]["workload_sha256"] = "0" * 64
        path = self.scratch / "manifest.csv"
        self.write_rows(path, rows)
        with self.assertRaisesRegex(RuntimeError, "Input hash mismatch"):
            load_manifest(path)

    def test_existing_output_is_not_overwritten(self):
        folder = self.scratch / "runs/experiments"
        folder.mkdir(parents=True)
        sentinel = folder / "existing.txt"
        sentinel.write_text("keep this", encoding="utf-8")
        result = subprocess.run([sys.executable, str(SIMPLE / "scripts/run_experiments.py"),
                                 "--output-root", str(self.scratch)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Refusing to overwrite", result.stderr)
        self.assertEqual(sentinel.read_text(), "keep this")
        self.assertEqual(list(folder.iterdir()), [sentinel])

    def exercise_shared_report(self, fail_second=False):
        """Use known outputs as a stand-in for B; exercise real orchestration and verification."""
        workload = SIMPLE / "workloads/smoke.csv"
        base = read_csv(SIMPLE / "experiments.csv")[0]
        rows = [{**base, "run_id": name, "workload_file": "workloads/smoke.csv",
                 "workload_sha256": sha256(workload)} for name in ["first", "second"]]

        def fake_command(argv, cwd, env, expected_success=True):
            if argv[0] == "git":
                return {"stdout": "test-revision" if "rev-parse" in argv else ""}
            for filename in ["completed.csv", "qmet.csv", "job.log"]:
                shutil.copyfile(SIMPLE / "runs/smoke" / filename, cwd / filename)
            failed = fail_second and cwd.name == "second"
            return {"argv": [str(a) for a in argv], "cwd": str(cwd),
                    "exit_code": 1 if failed else 0, "stdout": "captured completion\n",
                    "stderr": "simulated diagnostic\n" if failed else ""}

        with patch.object(sys, "argv", ["runner", "--output-root", str(self.scratch)]), \
                patch.object(runner, "load_manifest", return_value=(rows, {workload: read_csv(workload)})), \
                patch.object(runner, "build_simulator", return_value=(Path("fake-simulator"),
                             {"executable_sha256": "test-hash", "source_sha256": {}})), \
                patch.object(runner, "command", side_effect=fake_command):
            if fail_second:
                with self.assertRaisesRegex(RuntimeError, "B failed"):
                    runner.main()
            else:
                runner.main()
        root = self.scratch / "runs/experiments"
        report = json.loads((root / "execution.json").read_text())
        self.assertEqual(report["layout_version"], 2)
        self.assertEqual(report["manifest_sha256"], sha256(SIMPLE / "experiments.csv"))
        self.assertFalse((root / "manifest.csv").exists())
        for name in ["first", "second"]:
            self.assertEqual({p.name for p in (root / name).iterdir()},
                             {"config.csv", "completed.csv", "qmet.csv", "job.log"})
            self.assertEqual(report["runs"][name]["metadata"]["command"]["stdout"], "captured completion\n")
        first = report["runs"]["first"]
        self.assertEqual(first["verification"]["summary"]["completed"], 6)
        for name, digest in first["metadata"]["output_sha256"].items():
            self.assertEqual(sha256(root / "first" / name), digest)
        return report

    def test_shared_report_preserves_successful_runs_and_outputs(self):
        report = self.exercise_shared_report()
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["completed_run_ids"], ["first", "second"])
        self.assertEqual(len(read_csv(self.scratch / "results/tables/run-summary.csv")), 2)

    def test_shared_report_preserves_diagnostics_and_prior_success_on_failure(self):
        report = self.exercise_shared_report(fail_second=True)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["completed_run_ids"], ["first"])
        self.assertEqual(report["failed_run_id"], "second")
        second = report["runs"]["second"]
        self.assertEqual(second["verification"]["status"], "failed")
        self.assertEqual(second["metadata"]["command"]["stderr"], "simulated diagnostic\n")
        self.assertEqual(len(read_csv(self.scratch / "results/tables/run-summary.csv")), 1)


if __name__ == "__main__":
    unittest.main()
