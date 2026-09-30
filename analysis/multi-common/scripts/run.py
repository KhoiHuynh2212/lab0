"""Validate F's small exit-directed networks using unchanged E/B/C; package or replay evidence."""

import argparse
from copy import deepcopy
from datetime import datetime, timezone
import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from time import perf_counter

from support import (COMMON, REPO, compatible_events, require, orient_network, write_case,
    read_csv, write_csv, save_json, sha, verify_case, incremental_metrics, check_c_visits,
    check_c_system, check_c_use, pack_evidence, unpack_evidence, parse_events, EVENT_HEADER)

DEFAULT_REF = 'be3dc3a'
C_TOOLS = ['filter-by-queue', 'filter-by-job', 'compute-use', 'derive-event-metrics', 'derive-sys-metrics']


def fixtures():
    def make(nodes, edges, exits, jobs, expected, losses=None, settings=None, horizon=5, limit=None):
        distance, outgoing = orient_network(nodes, edges, exits)
        config = {q: {'servers': 1, 'capacity': 4, 'rate': 1} for q in nodes}
        for q, values in (settings or {}).items():
            config[q].update(values)
        return {'nodes': nodes, 'edges': edges, 'exits': exits, 'distance': distance, 'outgoing': outgoing,
                'settings': config, 'jobs': [{'id': j, 'arrival': t, 'entry': q, 'work': distance[q] + 1}
                                            for j, t, q in jobs], 'horizon': horizon, 'time_limit': limit,
                'expected_completed': expected, 'expected_dropped': losses or {}}
    single = make(['q0'], [], ['q0'], [('j0', 0, 'q0'), ('j1', 0, 'q0'), ('j2', 1, 'q0')],
                  {'j0': 2, 'j1': 4, 'j2': 6}, settings={'q0': {'rate': 2}}, horizon=6)
    path = make(['q0', 'q1', 'q2'], [('q0', 'q1'), ('q1', 'q2')], ['q2'],
                [('j0', 0, 'q0'), ('j1', 0, 'q0')], {'j0': 3, 'j1': 4}, horizon=3)
    reverse = deepcopy(path)
    reverse['nodes'].reverse()
    branch = make(['q0', 'q1', 'q2', 'q3'], [('q0', 'q1'), ('q0', 'q2'), ('q1', 'q3'), ('q2', 'q3')],
                  ['q3'], [(j, 0, 'q0') for j in ['j0', 'j1', 'j2', 'j3']],
                  {'j0': 3, 'j1': 3, 'j2': 4, 'j3': 4}, settings={'q0': {'servers': 2}, 'q3': {'servers': 2}})
    overflow = make(['q0', 'q1'], [('q0', 'q1')], ['q1'],
                    [(j, 0, 'q0') for j in ['j0', 'j1', 'j2']], {'j0': 4}, {'j1': 1, 'j2': 2},
                    {'q0': {'servers': 2}, 'q1': {'capacity': 0, 'rate': 3}})
    multi = make(['q0', 'q1', 'q2', 'q3'], [('q0', 'q2'), ('q1', 'q3'), ('q0', 'q1')], ['q2', 'q3'],
                 [('j0', 0, 'q0'), ('j1', 0, 'q1')], {'j0': 2, 'j1': 2}, horizon=2)
    truncated = deepcopy(path)
    truncated.update(time_limit=2, expected_completed={})
    truncated['jobs'].append({'id': 'j9', 'arrival': 8, 'entry': 'q0', 'work': 3})
    windows = make(['q0'], [], ['q0'], [('j0', 0, 'q0'), ('j1', 0, 'q0')], {'j0': 1}, {'j1': 0},
                   {'q0': {'capacity': 0}}, horizon=1002, limit=1002)
    return {'single': single, 'path': path, 'reversed-path': reverse, 'branch': branch,
            'downstream-loss': overflow, 'multiple-boundaries': multi, 'time-limit': truncated,
            'window-history': windows}


class Runner:
    def __init__(self, output, revision):
        self.output = output
        self.workspace = output / 'working'
        self.evidence = output / 'evidence'
        self.workspace.mkdir(parents=True)
        self.evidence.mkdir()
        self.env = os.environ.copy()
        temp = self.workspace / 'tmp'
        temp.mkdir()
        for key in ['TEMP', 'TMP', 'TMPDIR']:
            self.env[key] = str(temp)
        self.env['PYTHONDONTWRITEBYTECODE'] = '1'
        self.env['PYTHONIOENCODING'] = 'utf-8'
        self.report = {'status': 'running', 'started_at_utc': datetime.now(timezone.utc).isoformat(),
                       'python': sys.version, 'commands': [], 'cases': {}, 'limitations': [
                           'Synthetic hand-calculated networks; no Texas dataset or final scale experiments yet.',
                           'C activity is rounded 0/1, saturation is rounded, error windows are counts.',
                           'C does not include elapsed in-progress service in unfinished-visit service time.',
                           'Native functional tests only; no Valgrind, sanitizers or coverage claimed.'],
                       'f_script_sha256': {p.relative_to(COMMON).as_posix(): sha(p) for p in
                                          list(Path(__file__).parent.glob('*.py')) + list((COMMON / 'tests').glob('*.py'))}}
        self.report['source_commit'] = self.git('rev-parse', f'{revision}^{{commit}}').decode().strip()
        self.report['host_head'] = self.git('rev-parse', 'HEAD').decode().strip()
        self.binaries = {}
        self.checkpoint_commands = True
        self.save()

    def git(self, *args):
        # The exception is command-scoped to the user's explicitly selected repository.
        return subprocess.check_output(['git', '-c', f'safe.directory={REPO.as_posix()}', *args], cwd=REPO)

    def save(self):
        save_json(self.evidence / 'execution.json', self.report)

    def command(self, args, cwd=None, success=True, timeout=180):
        args = [str(a) for a in args]
        started = perf_counter()
        result = subprocess.run(args, cwd=cwd or self.workspace, env=self.env, capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=timeout)
        self.report['commands'].append({'argv': args, 'cwd': str(cwd or self.workspace),
            'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
            'elapsed_seconds': round(perf_counter() - started, 6)})
        if self.checkpoint_commands or result.returncode != 0:
            self.save()
        if success:
            require(result.returncode == 0, f'Command failed: {args}\n{result.stderr}\n{result.stdout}')
        return result

    def export(self, additional=()):
        names = self.git('ls-tree', '-r', '--name-only', self.report['source_commit'], '--',
                         'implementation/obj-b/simple-queue-sim', 'implementation/obj-c', 'implementation/obj-e', *additional).decode().splitlines()
        self.source = self.workspace / 'source'
        self.report['source_sha256'], self.report['working_shared_sha256'] = {}, {}
        for name in names:
            if Path(name).suffix not in {'.cpp', '.hpp', '.py'} or '.test-build' in Path(name).parts:
                continue
            dest = self.source / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(self.git('show', f'{self.report["source_commit"]}:{name}'))
            self.report['source_sha256'][name] = sha(dest)
            if (REPO / name).is_file():
                self.report['working_shared_sha256'][name] = sha(REPO / name)
        self.save()

    def compiler_identity(self):
        compiler = shutil.which(os.environ.get('CXX', 'g++'))
        require(compiler, 'Put a C++20 compiler on PATH or set CXX')
        compiler = Path(compiler).resolve()
        identity = {'path': str(compiler), 'sha256': sha(compiler),
                    'version': self.command([compiler, '--version']).stdout,
                    'target': self.command([compiler, '-dumpmachine']).stdout.strip(),
                    'environment': {k: self.env.get(k) for k in
                        ('PATH', 'CPATH', 'CPLUS_INCLUDE_PATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH',
                         'COMPILER_PATH', 'GCC_EXEC_PREFIX', 'SOURCE_DATE_EPOCH')}}
        # The driver delegates actual compilation/linking; fingerprint those tools too.
        identity['tools'] = {}
        for name in ('cc1plus', 'as', 'ld'):
            tool = self.command([compiler, '-print-prog-name=' + name]).stdout.strip()
            resolved = Path(tool) if Path(tool).is_file() else Path(shutil.which(tool) or tool)
            require(resolved.is_file(), f'Cannot fingerprint compiler tool: {tool}')
            identity['tools'][name] = {'path': str(resolved.resolve()), 'sha256': sha(resolved)}
        return compiler, identity

    def compile_program(self, name, sources, includes, flags=None):
        """Reuse only a matching build recipe whose cached binary still hashes correctly."""
        if not hasattr(self, '_compiler'):
            self._compiler, self._compiler_identity = self.compiler_identity()
        flags = list(flags if flags is not None else ['-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-O2'])
        relative = lambda p: Path(p).relative_to(self.source).as_posix()
        recipe = {'schema': 1, 'compiler': self._compiler_identity, 'flags': flags,
                  'sources': [relative(p) for p in sources], 'includes': [relative(p) for p in includes],
                  'exported_sources': {relative(p): sha(p) for p in sorted(self.source.rglob('*')) if p.is_file()}}
        key = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
        cache = COMMON / '.build' / 'build-cache'
        cache.mkdir(parents=True, exist_ok=True)
        suffix = '.exe' if os.name == 'nt' else ''
        cached, metadata = cache / (key + suffix), cache / (key + '.json')
        target = self.workspace / (name + suffix)
        hit = False
        try:
            record = json.loads(metadata.read_text(encoding='utf-8'))
            hit = record['recipe'] == recipe and sha(cached) == record['binary_sha256']
        except (OSError, ValueError, KeyError, TypeError):
            pass
        if hit:
            shutil.copyfile(cached, target)
            require(sha(target) == record['binary_sha256'], 'Cached binary changed during copy')
        else:
            self.command([self._compiler, *flags, *['-I' + str(p) for p in includes],
                          *sources, '-o', target])
            record = {'recipe': recipe, 'binary_sha256': sha(target)}
            # Publish the binary before its record. An interrupted publication is a cache miss.
            with tempfile.TemporaryDirectory(dir=cache) as temp:
                staged = Path(temp)
                shutil.copyfile(target, staged / 'binary')
                save_json(staged / 'metadata.json', record)
                os.replace(staged / 'binary', cached)
                os.replace(staged / 'metadata.json', metadata)
        if os.name != 'nt':
            target.chmod(target.stat().st_mode | 0o111)
        self.report.setdefault('build_cache', []).append({'program': name, 'key': key,
            'hit': hit, 'binary_sha256': record['binary_sha256'], 'recipe': recipe})
        self.save()
        return target

    def build(self, only_e=False):
        b = self.source / 'implementation/obj-b/simple-queue-sim'
        e = self.source / 'implementation/obj-e/multi-queue-sim'
        compile_program = self.compile_program

        e_common = [e / 'src/multi_queue_sim.cpp', b / 'src/queue_sim.cpp']
        self.binaries['E'] = compile_program('multi-queue-sim', [e / 'src/main.cpp'] + e_common,
                                             [e / 'include', b / 'include'])
        if only_e:
            return
        e_test = compile_program('test-E', [e / 'src/test_multi_queue_sim.cpp'] + e_common,
                                 [e / 'include', b / 'include'])
        self.command([e_test])
        self.binaries['B'] = compile_program('simple-queue-sim', [b / 'src/main.cpp', b / 'src/queue_sim.cpp'], [b / 'include'])
        for name in C_TOOLS:
            folder = self.source / 'implementation/obj-c' / name
            stem = name.replace('-', '_')
            common = folder / f'src/{stem}.cpp'
            self.binaries[name] = compile_program(name, [folder / 'src/main.cpp', common], [folder / 'include'])
            test = compile_program('test-' + name, [folder / f'tests/test_{stem}.cpp', common], [folder / 'include'])
            self.command([test])
            print(f'Built and tested unchanged C tool: {name}', flush=True)
        for name, program in self.binaries.items():
            self.command([program, '--help'])
            require(self.command([program], success=False).returncode != 0, f'{name} accepted missing arguments')
        pretty = self.source / 'implementation/obj-e/pretty-print-events'
        self.command([sys.executable, pretty / 'test_pretty_print_events.py'], cwd=pretty)

    def simulate(self, folder, model):
        args = [self.binaries['E'], 'config.csv', '--metrics', 'qmet.csv', '--events', 'job.log']
        if model.get('time_limit') is not None:
            args += ['--time-limit', str(model['time_limit'])]
        self.command(args, cwd=folder, timeout=30)

    def analyze(self, folder, model):
        native = folder / 'native'
        native.mkdir()
        adapted = folder / 'analysis-input.log'
        inserted = compatible_events(folder / 'job.log', adapted, model)
        ledger = verify_case(folder, adapted, model)
        save_json(folder / 'verification.json', ledger)
        save_json(folder / 'adapter.json', {'raw_sha256': sha(folder / 'job.log'), 'inserted_arrivals': inserted,
                                           'basis': 'same-tick transfer; independently checked against raw queue snapshots'})
        self.command([self.binaries['derive-event-metrics'], folder / 'job.log', native / 'raw-visits.csv'])
        self.command([self.binaries['derive-event-metrics'], adapted, native / 'visits.csv'])
        visits = check_c_visits(native / 'visits.csv', ledger)
        self.command([self.binaries['derive-sys-metrics'], adapted, native / 'system.csv'])
        check_c_system(native / 'system.csv', ledger, model)
        use_rows = 0
        for q in model['nodes']:
            filtered = native / f'{q}-filtered.csv'
            increments = native / f'{q}-increments.csv'
            use = native / f'{q}-use.csv'
            self.command([self.binaries['filter-by-queue'], folder / 'qmet.csv', filtered, f'--queues={q}'])
            require(read_csv(filtered) == [r for r in read_csv(folder / 'qmet.csv') if r['queue-sys-id'] == q],
                    'C queue filter changed data')
            incremental_metrics(filtered, increments, q)
            self.command([self.binaries['compute-use'], increments, use])
            use_rows += check_c_use(increments, use)
        selected = model['jobs'][min(1, len(model['jobs']) - 1)]['id']
        self.command([self.binaries['filter-by-job'], adapted, native / 'selected.log', f'--jobs={selected}'])
        require(parse_events(native / 'selected.log') == [e for e in parse_events(adapted) if e[1] == selected],
                'C job selection lost events or leaked an unselected job')
        pretty = self.source / 'implementation/obj-e/pretty-print-events/pretty-print-events.py'
        self.command([sys.executable, pretty, adapted, '--jobs', selected, '--color', 'never', '--legend',
                      '--tsv', native / 'trace.tsv'])
        return {'completed': len(ledger['completed']), 'dropped': len(ledger['dropped']),
                'unfinished': len(ledger['unfinished']), 'unarrived': len(ledger['unarrived']),
                'completed_before_horizon': ledger['completed_before_horizon'],
                'raw_C_visits': len(read_csv(native / 'raw-visits.csv')), 'checked_C_visits': visits,
                'inserted_arrivals': len(inserted), 'checked_USE_rows': use_rows,
                'waiting_car_steps': ledger['waiting_car_steps'], 'service_car_steps': ledger['service_car_steps']}

    def validate(self):
        cases = fixtures()
        for name, model in cases.items():
            folder = self.evidence / name
            write_case(folder, model)
            self.simulate(folder, model)
            self.report['cases'][name] = self.analyze(folder, model)
            print(f'Validated {name}: {self.report["cases"][name]}', flush=True)
        # E and B must agree exactly for the single-queue fixture.
        single = self.evidence / 'single'
        other = self.workspace / 'b-comparison'
        write_case(other, cases['single'])
        self.command([self.binaries['B'], 'config.csv', '--metrics', 'qmet.csv', '--events', 'job.log'], cwd=other)
        require(all(sha(single / f) == sha(other / f) for f in ['completed.wl', 'qmet.csv', 'job.log']), 'E/B one-queue disagreement')
        self.report['single_queue_matches_B'] = True
        # Check native graph conversion before F sets source/sink/resource fields.
        graph = self.evidence / 'path/edges.txt'
        graph.write_text('0 1\n1 2\n', encoding='utf-8')
        generator = self.source / 'implementation/obj-e/create-config-from-network/create-config-from-network.py'
        target = self.evidence / 'path/generated-config.csv'
        self.command([sys.executable, generator, graph, target, '--server-func', 'Sub-1', '--rate', '1'])
        generated = read_csv(target)
        require({r['queue_sys_id']: r['edges_out'] for r in generated} == {'q0': 'q1', 'q1': 'q2', 'q2': ''},
                'Graph converter changed oriented edges')
        self.report['oriented_graph_converter_checked'] = True
        for name in ['branch', 'path']:
            events = parse_events(self.evidence / name / 'job.log')
            transfers = [(j, target) for _, j, q, action, target in events if q == 'q0' and action == 'DEPART-VIA']
            expected = [('j0', 'q1'), ('j1', 'q2'), ('j2', 'q1'), ('j3', 'q2')] if name == 'branch' else [('j0', 'q1'), ('j1', 'q1')]
            require(transfers == expected, 'Unexpected routing order')
        require(self.report['cases']['path']['completed_before_horizon'] == 0, 'Exact-cutoff convention failed')
        require(self.report['cases']['time-limit']['unfinished'] == 2 and
                self.report['cases']['time-limit']['unarrived'] == 1, 'Truncated accounting failed')
        self.empty_transit_probe()

    def empty_transit_probe(self):
        """Quarantined diagnostic outside the road model; never repair its lost job in analysis."""
        folder = self.evidence / 'diagnostic-empty-transit'
        folder.mkdir()
        write_csv(folder / 'in.wl', ['Job Id', 'arrival time', 'Tasks'],
                  [{'Job Id': 'probe', 'arrival time': 0, 'Tasks': "['0: W 1']"}])
        from support import CONFIG_FIELDS
        # q2 sends a completed/empty job to q1; q1 forwards it to already-processed q0.
        rows = [['q0', 2, 1, 'Sub-1', 1, '', 'completed.wl', 'q1', ''],
                ['q1', 2, 1, 'Sub-1', 1, '', '', 'q2', 'q0'],
                ['q2', 2, 1, 'Sub-1', 1, 'in.wl', '', '', 'q1']]
        write_csv(folder / 'config.csv', CONFIG_FIELDS, [dict(zip(CONFIG_FIELDS, r)) for r in rows])
        self.command([self.binaries['E'], 'config.csv', '--time-limit', '4', '--metrics', 'qmet.csv', '--events', 'job.log'], cwd=folder)
        events = parse_events(folder / 'job.log')
        lost = len(read_csv(folder / 'completed.wl')) == 0 and not any(e[3] == 'ERROR' for e in events)
        require(lost and any(e[2:] == ('q1', 'DEPART-VIA', 'q0') for e in events),
                'Empty-transit behavior changed; review diagnostic and model constraints')
        self.report['empty_transit_diagnostic'] = {'status': 'confirmed_loss_outside_supported_model',
            'offered': 1, 'completed': 0, 'logged_errors': 0,
            'explanation': 'Empty job forwarded during admission toward an already-processed queue is not delivered.',
            'road_model_prevention': 'Every transfer carries positive remaining work; completion occurs at a configured exit.'}

    def finish(self):
        require(all(sha(self.source / p) == h for p, h in self.report['source_sha256'].items()), 'Copied team source changed')
        require(all(sha(REPO / p) == h for p, h in self.report['working_shared_sha256'].items()), 'Working team source changed')
        self.report.update(status='passed_with_documented_model_constraints',
                           completed_at_utc=datetime.now(timezone.utc).isoformat(), team_sources_unchanged=True)
        self.save()


def new_output(path):
    path = path.resolve()
    require(path.is_relative_to((COMMON / '.build').resolve()) and path != (COMMON / '.build').resolve(),
            'Output must be a new directory below analysis/multi-common/.build')
    require(not path.exists(), 'Refusing existing output directory')
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    validation = sub.add_parser('validate', help='Build unchanged tools and check small hand-calculated networks')
    validation.add_argument('--ref', default=DEFAULT_REF)
    validation.add_argument('--output-root', type=Path, required=True)
    validation.add_argument('--archive', type=Path)
    replay = sub.add_parser('replay', help='Verify archive contents and rerun the road-model fixtures')
    replay.add_argument('archive', type=Path)
    replay.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    output = new_output(args.output_root)
    if args.action == 'validate' and args.archive:
        archive = args.archive.resolve()
        require(archive.is_relative_to(COMMON) and archive.suffix == '.zip' and not archive.exists(),
                'Archive must be a new ZIP inside multi-common')
        archive.parent.mkdir(parents=True, exist_ok=True)
    else:
        archive = None
    if args.action == 'replay':
        unpack_evidence(args.archive, output / 'reference')
        old = json.loads((output / 'reference/execution.json').read_text())
        require(old['status'] == 'passed_with_documented_model_constraints', 'Archive is not a passed validation')
        revision = old['source_commit']
    else:
        revision = args.ref
    runner = Runner(output, revision)
    try:
        runner.export()
        runner.build(only_e=args.action == 'replay')
        if args.action == 'validate':
            runner.validate()
        else:
            for name in old['cases']:
                original = output / 'reference' / name
                model = json.loads((original / 'model.json').read_text())
                folder = runner.evidence / name
                folder.mkdir()
                for f in original.iterdir():
                    if f.name == 'config.csv' or (f.suffix == '.wl' and f.name != 'completed.wl'):
                        shutil.copyfile(f, folder / f.name)
                runner.simulate(folder, model)
                require(all(sha(folder / f) == sha(original / f) for f in ['job.log', 'qmet.csv', 'completed.wl']),
                        f'Replay differs: {name}')
                compatible_events(folder / 'job.log', folder / 'analysis-input.log', model)
                ledger = verify_case(folder, folder / 'analysis-input.log', model)
                runner.report['cases'][name] = {'replayed_raw_bytes_match': True, 'visits_checked': len(ledger['visits'])}
                print(f'Replay matched: {name}', flush=True)
        runner.finish()
        if archive:
            record = pack_evidence(runner.evidence, archive)
            unpack_evidence(archive, output / 'archive-check')
            save_json(output / 'archive.json', record)
            print(f'Checked archive: {archive} ({record["bytes"]} bytes)', flush=True)
    except Exception as exc:
        runner.report.update(status='failed', error=str(exc))
        runner.save()
        raise
    print(f'{args.action}: passed; evidence at {runner.evidence}')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        print(f'F validation failed: {exc}', file=sys.stderr)
        sys.exit(1)
