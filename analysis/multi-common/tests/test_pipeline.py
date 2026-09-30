"""Model and corruption tests against packaged native evidence; no simulator build needed."""

import hashlib
import gzip
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile

COMMON = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(COMMON / 'scripts'))
from run import Runner, fixtures, new_output
from prepare import read_network, bfs, sample, allocate_servers, CHANGES
from analyze import response_summary, terminal_use, package_study
from types import SimpleNamespace
from support import (orient_network, validate_model, compatible_events, verify_case, read_csv,
                     write_csv, check_c_visits, check_c_system, incremental_metrics, unpack_evidence, check_c_use)


class ModelTests(unittest.TestCase):
    def test_orientation_preserves_real_edges_and_reduces_distance(self):
        distance, edges = orient_network(['q0', 'q1', 'q2', 'q3'],
            [('q0', 'q1'), ('q0', 'q2'), ('q1', 'q2'), ('q1', 'q3'), ('q2', 'q3')], ['q3'])
        self.assertEqual(distance, {'q3': 0, 'q1': 1, 'q2': 1, 'q0': 2})
        self.assertEqual(edges, {'q0': ['q1', 'q2'], 'q1': ['q3'], 'q2': ['q3'], 'q3': []})

    def test_missing_exit(self):
        with self.assertRaisesRegex(ValueError, 'exit'):
            orient_network(['q0'], [], [])

    def test_unreachable_component(self):
        with self.assertRaisesRegex(ValueError, 'cannot reach'):
            orient_network(['q0', 'q1'], [], ['q1'])

    def test_wrong_work(self):
        model = fixtures()['path']
        model['jobs'][0]['work'] = 1
        with self.assertRaisesRegex(ValueError, 'Work'):
            validate_model(model)

    def test_backward_edge(self):
        model = fixtures()['path']
        model['outgoing']['q1'].append('q0')
        with self.assertRaisesRegex(ValueError, 'Direction'):
            validate_model(model)

    def test_duplicate_source_job(self):
        model = fixtures()['multiple-boundaries']
        model['jobs'][1]['id'] = 'j0'
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            validate_model(model)

    def test_unsafe_queue_filename(self):
        model = fixtures()['single']
        model['nodes'] = ['../q0']
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            validate_model(model)

    def test_job_prefix_required_by_c_filter(self):
        model = fixtures()['single']
        model['jobs'][0]['id'] = 'a'
        with self.assertRaisesRegex(ValueError, 'prefix'):
            validate_model(model)


class BuildCacheTests(unittest.TestCase):
    def test_reuse_and_invalidate_source_header_flags_compiler_and_binary(self):
        with tempfile.TemporaryDirectory(dir=COMMON / '.build') as temp:
            root = Path(temp)
            runner = Runner.__new__(Runner)
            runner.source, runner.workspace = root / 'source', root / 'working'
            runner.source.mkdir()
            runner.workspace.mkdir()
            source, header = runner.source / 'main.cpp', runner.source / 'api.hpp'
            source.write_text('original source')
            header.write_text('original header')
            runner._compiler, runner._compiler_identity = 'fake-compiler', {'version': '1'}
            runner.report = {}
            runner.save = lambda: None
            compilations = []
            def compile_fake(args):
                compilations.append(args)
                Path(args[-1]).write_bytes(('binary ' + str(len(compilations))).encode())
            runner.command = compile_fake
            with patch('run.COMMON', root):
                def build(flags=None):
                    return runner.compile_program('test', [source], [runner.source], flags)
                build()
                build()
                self.assertEqual(len(compilations), 1)
                self.assertTrue(runner.report['build_cache'][-1]['hit'])
                source.write_text('changed source')
                build()
                header.write_text('changed header')
                build()
                build(['-O0'])
                runner._compiler_identity['version'] = '2'
                build(['-O0'])
                key = runner.report['build_cache'][-1]['key']
                binary = root / '.build/build-cache' / (key + ('.exe' if os.name == 'nt' else ''))
                binary.write_bytes(b'corrupt')
                build(['-O0'])
                self.assertEqual(len(compilations), 6)
                build(['-O0'])
                self.assertEqual(len(compilations), 6)
                (binary.parent / (key + '.json')).write_text('{broken')
                build(['-O0'])
                self.assertEqual(len(compilations), 7)


class PreparationTests(unittest.TestCase):
    def test_normalization_counts_reverse_duplicates_and_loops(self):
        with tempfile.TemporaryDirectory(dir=COMMON / '.build') as temp:
            path = Path(temp) / 'graph.gz'
            with gzip.open(path, 'wt') as output:
                output.write('# graph\n0 1\n1 0\n0 1\n1 2\n2 2\n')
            graph, counts = read_network(path)
            self.assertEqual(graph, {0: [1], 1: [0, 2], 2: [1]})
            self.assertEqual(counts, dict(nodes=3, undirected_edges=2, input_records=5,
                                         removed_self_loops=1, duplicate_or_reverse_records=2))

    def test_nested_samples_and_boundary_evidence(self):
        graph = {n: sorted({(n-1) % 80, (n+1) % 80}) for n in range(80)}
        order = bfs(graph, 0, 40)
        small, medium = sample(graph, order[:20]), sample(graph, order)
        self.assertEqual(list(small['original_ids'].values()), list(medium['original_ids'].values())[:20])
        for topology in (small, medium):
            selected = set(topology['original_ids'].values())
            self.assertEqual(set(topology['exits']) & set(topology['ingress']), set())
            for q, outside in topology['boundary'].items():
                self.assertEqual(outside, [n for n in graph[topology['original_ids'][q]] if n not in selected])
            self.assertTrue(set(topology['exits']) <= set(topology['boundary']))
            self.assertAlmostEqual(sum(topology['no_loss_visit_fraction'][q] for q in topology['exits']), 1)

    def test_dead_end_is_not_automatically_an_exit(self):
        graph = {n: [v for v in (n-1, n+1) if 0 <= v < 25] for n in range(25)}
        topology = sample(graph, bfs(graph, 0, 20))
        self.assertEqual(topology['exits'], ['q19'])
        self.assertNotIn('q0', topology['boundary'])

    def test_exact_resource_totals_at_all_scales(self):
        for n in (20, 200, 2000):
            nodes = [f'q{i}' for i in range(n)]
            for change in CHANGES:
                allocation = allocate_servers(nodes, change)
                self.assertEqual(sum(allocation.values())*100, 4*n*(100+change))
                self.assertGreaterEqual(min(allocation.values()), 1)
            self.assertEqual(list(allocate_servers(nodes, -30).values())[:n//5], [2]*(n//5))

    def test_component_too_small_fails(self):
        with self.assertRaisesRegex(ValueError, 'too small'):
            bfs({0: [1], 1: [0]}, 0, 3)


class StudyTests(unittest.TestCase):
    def test_mean_weights_each_completed_vehicle_and_excludes_drops(self):
        jobs = [{'id': 'j0', 'arrival': 0}, {'id': 'j1', 'arrival': 9},
                {'id': 'j2', 'arrival': 10}, {'id': 'j3', 'arrival': 0}]
        result = response_summary({'j0': 10, 'j1': 10, 'j2': 11}, jobs)
        self.assertEqual(result, dict(mean_response=4, peak_throughput=2, departure_steps=2))
        self.assertIsNone(response_summary({}, jobs)['mean_response'])

    def test_terminal_use_matches_every_saved_small_native_queue(self):
        with tempfile.TemporaryDirectory(dir=COMMON / '.build') as temp, \
                ZipFile(COMMON.parent / 'multi-SMALL/evidence.zip') as archive:
            path = Path(temp) / 'qmet.csv'
            for row in read_csv(COMMON.parent / 'multi-SMALL/experiments.csv'):
                name = row['run']
                path.write_bytes(archive.read(f'{name}/qmet.csv'))
                actual = terminal_use(path)
                for q, values in actual.items():
                    path.write_bytes(archive.read(f'{name}/native/{q}-use.csv'))
                    last = read_csv(path)[-1]
                    expected = {f'{label}_{w}': int(last[f'{label}({w})'])
                                for label in ('Utilization', 'Saturation', 'Errors') for w in (10, 100, 1000)}
                    self.assertEqual(values, expected, (name, q))

    def test_archive_contains_final_cases_and_external_hash_matches(self):
        with tempfile.TemporaryDirectory(dir=COMMON / '.build') as temp:
            output = Path(temp)
            evidence = output / 'evidence'
            evidence.mkdir()
            report = {'status': 'running', 'cases': {'example': {'completed': 3}}}
            def finish():
                report['status'] = 'passed_with_documented_model_constraints'
                (evidence / 'execution.json').write_text(json.dumps(report))
            runner = SimpleNamespace(output=output, evidence=evidence, report=report, finish=finish)
            package_study(runner)
            external = json.loads((output / 'execution.json').read_text())
            with ZipFile(output / 'evidence.zip') as archive:
                internal = json.loads(archive.read('execution.json'))
            self.assertEqual(internal, {k: v for k, v in external.items() if k != 'evidence_archive'})
            self.assertEqual(external['evidence_archive']['sha256'],
                             hashlib.sha256((output / 'evidence.zip').read_bytes()).hexdigest())
            self.assertNotEqual(internal['status'], 'running')

    def test_streaming_use_rejects_decreasing_counter(self):
        with tempfile.TemporaryDirectory(dir=COMMON / '.build') as temp:
            path = Path(temp) / 'qmet.csv'
            path.write_text('time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,num-of-errors\n'
                            '0,q0,0,1,1\n1,q0,0,0,0\n')
            with self.assertRaisesRegex(ValueError, 'Decreasing'):
                terminal_use(path)


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        (COMMON / '.build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=COMMON / '.build')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        archive = Path(os.environ.get('F_VALIDATION_ARCHIVE', str(COMMON / 'validation.zip')))
        self.reference = self.root / 'reference'
        unpack_evidence(archive, self.reference)

    def case(self, name='path'):
        folder = self.reference / name
        return folder, json.loads((folder / 'model.json').read_text())

    def test_all_archived_road_cases_reconcile(self):
        for name in fixtures():
            folder, model = self.case(name)
            ledger = verify_case(folder, folder / 'analysis-input.log', model)
            self.assertEqual(json.loads(json.dumps(ledger)), json.loads((folder / 'verification.json').read_text()))
            check_c_visits(folder / 'native/visits.csv', ledger)
            check_c_system(folder / 'native/system.csv', ledger, model)

    def test_streamed_adapter_preserves_archived_bytes_and_insertions(self):
        for name in fixtures():
            folder, model = self.case(name)
            target = folder / 'streamed.log'
            inserted = compatible_events(folder / 'job.log', target, model)
            self.assertEqual(target.read_bytes(), (folder / 'analysis-input.log').read_bytes())
            self.assertEqual(inserted, json.loads((folder / 'adapter.json').read_text())['inserted_arrivals'])
            counted = folder / 'counted.log'
            self.assertEqual(compatible_events(folder / 'job.log', counted, model, count_only=True), len(inserted))
            self.assertEqual(counted.read_bytes(), target.read_bytes())

    def test_main_accounting_does_not_use_whole_file_event_or_metric_readers(self):
        folder, model = self.case()
        original_read = read_csv
        def bounded_read(path):
            self.assertNotEqual(Path(path).name, 'qmet.csv')
            return original_read(path)
        with patch('support.read_csv', side_effect=bounded_read), \
                patch('support.parse_events', side_effect=AssertionError('Whole-file event read')):
            compatible_events(folder / 'job.log', folder / 'streamed.log', model, count_only=True)
            verify_case(folder, folder / 'streamed.log', model)

    def test_streamed_snapshot_rejects_missing_duplicate_and_regressing_rows(self):
        folder, model = self.case()
        path = folder / 'qmet.csv'
        rows = read_csv(path)
        for corrupt in (rows[1:], [rows[0], *rows], [*rows, rows[0]]):
            with self.subTest(rows=len(corrupt)):
                write_csv(path, list(rows[0]), corrupt)
                with self.assertRaisesRegex(ValueError, 'snapshot|duplicate|Noncontiguous'):
                    verify_case(folder, folder / 'analysis-input.log', model)

    def test_streamed_events_reject_time_regression_and_past_observation(self):
        folder, model = self.case()
        path = folder / 'analysis-input.log'
        original = path.read_text()
        for extra, message in [('0 j0 IN q0 ERROR QUEUE-FULL\n', 'time order'),
                               ('999 j0 IN q0 ERROR QUEUE-FULL\n', 'outside observation')]:
            with self.subTest(message=message):
                path.write_text(original + extra)
                with self.assertRaisesRegex(ValueError, message):
                    verify_case(folder, path, model)

    def test_duplicate_c_visit_cannot_mask_omission(self):
        folder, model = self.case()
        ledger = verify_case(folder, folder / 'analysis-input.log', model)
        path = folder / 'native/visits.csv'
        rows = read_csv(path)
        rows[-1] = rows[0].copy()
        write_csv(path, list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'omitted or duplicated'):
            check_c_visits(path, ledger)

    def test_downstream_visits_missing_without_adapter(self):
        folder, model = self.case()
        ledger = verify_case(folder, folder / 'analysis-input.log', model)
        self.assertEqual(len(ledger['visits']), 6)
        self.assertEqual(len(read_csv(folder / 'native/raw-visits.csv')), 2)
        with self.assertRaisesRegex(ValueError, 'omitted'):
            check_c_visits(folder / 'native/raw-visits.csv', ledger)

    def test_future_native_arrivals_are_not_duplicated(self):
        folder, model = self.case()
        target = folder / 'readapted.log'
        self.assertEqual(compatible_events(folder / 'analysis-input.log', target, model), [])
        verify_case(folder, target, model)

    def test_source_log_cannot_be_overwritten(self):
        folder, model = self.case()
        original = (folder / 'job.log').read_bytes()
        with self.assertRaisesRegex(ValueError, 'overwrite'):
            compatible_events(folder / 'job.log', folder / 'job.log', model)
        self.assertEqual((folder / 'job.log').read_bytes(), original)

    def test_plausible_but_wrong_queue_count(self):
        folder, model = self.case()
        rows = read_csv(folder / 'qmet.csv')
        rows[0]['num-jobs-in-queue'] = '0'
        write_csv(folder / 'qmet.csv', list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'occupancy'):
            verify_case(folder, folder / 'analysis-input.log', model)

    def test_missing_task_stop(self):
        folder, model = self.case()
        p = folder / 'analysis-input.log'
        p.write_text(p.read_text().replace('1 j0 IN q0 TASK-STOP 0 "W 3"\n', ''), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Incomplete service'):
            verify_case(folder, p, model)

    def test_config_drift_detected_even_at_low_load(self):
        folder, model = self.case()
        rows = read_csv(folder / 'config.csv')
        rows[0]['max_queue_size'] = '100'
        write_csv(folder / 'config.csv', list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'Configuration settings'):
            verify_case(folder, folder / 'analysis-input.log', model)

    def test_adapter_cannot_hide_lost_delivery(self):
        folder, model = self.case()
        p = folder / 'job.log'
        # Lose all downstream evidence for a; inferred arrivals must fail raw-state checks.
        p.write_text('\n'.join(line for line in p.read_text().splitlines()
                               if not (' j0 IN ' in line and ' IN q0 ' not in line)) + '\n', encoding='utf-8')
        target = folder / 'corrupt-adapted.log'
        compatible_events(p, target, model)
        with self.assertRaisesRegex(ValueError, 'occupancy'):
            verify_case(folder, target, model)

    def test_internal_drop_residence_is_counted(self):
        folder, model = self.case('downstream-loss')
        ledger = verify_case(folder, folder / 'analysis-input.log', model)
        self.assertEqual(ledger['dropped'], {'j1': 1, 'j2': 2})
        self.assertEqual(ledger['waiting_car_steps'] + ledger['service_car_steps'], 7)
        # The completed car contributes four; the two later-lost cars contribute one and two.
        self.assertEqual(ledger['completed'], {'j0': 4})

    def test_cutoff_and_unarrived_are_separate(self):
        folder, model = self.case('time-limit')
        ledger = verify_case(folder, folder / 'analysis-input.log', model)
        self.assertEqual(ledger['unfinished'], ['j0', 'j1'])
        self.assertEqual(ledger['unarrived'], ['j9'])
        self.assertEqual(ledger['service_car_steps'], 3)
        folder, model = self.case('path')
        self.assertEqual(verify_case(folder, folder / 'analysis-input.log', model)['completed_before_horizon'], 0)

    def test_duplicate_sink_job(self):
        folder, model = self.case()
        rows = read_csv(folder / 'completed.wl')
        rows[1] = rows[0].copy()
        write_csv(folder / 'completed.wl', list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'Sink count'):
            verify_case(folder, folder / 'analysis-input.log', model)

    def test_internal_transfers_not_system_completions(self):
        folder, model = self.case()
        ledger = verify_case(folder, folder / 'analysis-input.log', model)
        rows = read_csv(folder / 'native/system.csv')
        rows[0]['throughput'] = '3'
        write_csv(folder / 'native/system.csv', list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'system values'):
            check_c_system(folder / 'native/system.csv', ledger, model)

    def test_decreasing_error_counter(self):
        folder, _ = self.case('downstream-loss')
        p = folder / 'native/q1-filtered.csv'
        rows = read_csv(p)
        rows[-1]['num-of-errors'] = '0'
        write_csv(p, list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'Decreasing'):
            incremental_metrics(p, folder / 'bad-increments.csv', 'q1')

    def test_full_windows_expire_historical_error(self):
        folder, _ = self.case('window-history')
        rows = read_csv(folder / 'native/q0-use.csv')
        for w in (10, 100, 1000):
            self.assertEqual(rows[w - 1][f'Errors({w})'], '1')
            self.assertEqual(rows[w][f'Errors({w})'], '0')
        rows[1000]['Errors(1000)'] = '1'
        write_csv(folder / 'native/q0-use.csv', list(rows[0]), rows)
        with self.assertRaisesRegex(ValueError, 'window contract'):
            check_c_use(folder / 'native/q0-increments.csv', folder / 'native/q0-use.csv')

    def test_existing_output_is_protected(self):
        sentinel = self.root / 'sentinel'
        sentinel.write_text('keep')
        with self.assertRaisesRegex(ValueError, 'existing'):
            new_output(self.root)
        self.assertEqual(sentinel.read_text(), 'keep')

    def test_archive_path_traversal(self):
        p = self.root / 'unsafe.zip'
        with ZipFile(p, 'w') as archive:
            archive.writestr('../escape.txt', 'bad')
            archive.writestr('checksums.json', '{}')
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            unpack_evidence(p, self.root / 'unsafe-output')
        self.assertFalse((self.root / 'escape.txt').exists())

    def test_archive_corruption(self):
        p = self.root / 'bad.zip'
        with ZipFile(p, 'w') as archive:
            archive.writestr('data.txt', 'changed')
            archive.writestr('checksums.json', json.dumps({'data.txt': hashlib.sha256(b'original').hexdigest()}))
        with self.assertRaisesRegex(ValueError, 'checksum'):
            unpack_evidence(p, self.root / 'bad-output')
        self.assertFalse((self.root / 'bad-output').exists())


if __name__ == '__main__':
    unittest.main()
