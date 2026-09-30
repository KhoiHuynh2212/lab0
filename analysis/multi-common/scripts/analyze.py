"""Execute a scale study: simulate all manifest runs, validate, analyze with all C tools, and report."""

import argparse, base64, csv, json, sys
from collections import Counter, defaultdict, deque
from datetime import datetime, timezone
from pathlib import Path
from time import perf_counter

from run import Runner, new_output, C_TOOLS, DEFAULT_REF
from support import (COMMON, REPO, require, sha, read_csv, write_csv, save_json,
    compatible_events, verify_case, check_c_visits, check_c_system, check_c_use,
    incremental_metrics, pack_evidence, unpack_evidence, write_case)

SCALES = {'SMALL': 'multi-SMALL', 'MEDIUM': 'multi-MEDIUM', 'LARGE': 'multi-LARGE'}
SELECTED_RUNS_FOR_PLOTS = {'stressed-42-+0', 'low-42-+0', 'overloaded-42-+0'}


def load_inputs(archive, dest):
    """Extract inputs.zip and return topology, server allocations, and workload loader."""
    unpack_evidence(archive, dest)
    topology = json.loads((dest / 'topology.json').read_text())
    allocations = json.loads((dest / 'server-allocations.json').read_text())
    return topology, allocations


def build_model(topology, manifest_row, allocations, vehicles_dir):
    """Construct a model dict from prepared inputs for one experiment run."""
    change = str(manifest_row['server_change_percent'])
    servers = allocations[change]
    settings = {q: dict(servers=servers[q], capacity=20, rate=1) for q in topology['nodes']}
    vehicles = read_csv(vehicles_dir / manifest_row['workload'])
    jobs = [{'id': r['id'], 'arrival': int(r['arrival']), 'entry': r['entry'],
             'work': int(r['work'])} for r in vehicles]
    return dict(nodes=topology['nodes'], edges=topology['edges'], exits=topology['exits'],
                distance=topology['distance'], outgoing=topology['outgoing'],
                settings=settings, jobs=jobs, horizon=int(manifest_row['horizon']), time_limit=None)


def write_run(folder, model):
    write_case(folder, model)


def response_summary(completed, jobs):
    """Exact end-to-end latency of completed vehicles; dropped jobs are excluded."""
    arrivals = {j['id']: j['arrival'] for j in jobs}
    departures = Counter(completed.values())
    return dict(mean_response=(sum(t-arrivals[j] for j, t in completed.items()) / len(completed)
                               if completed else None),
                peak_throughput=max(departures.values(), default=0), departure_steps=len(departures))


def terminal_use(path):
    """One metrics pass; retain only the last 1,000 samples per queue for final C-style USE."""
    history = defaultdict(lambda: deque(maxlen=1000))
    previous, next_time = Counter(), Counter()
    with path.open(newline='', encoding='utf-8') as stream:
        for row in csv.DictReader(stream):
            q = row['queue-sys-id']
            cumulative = int(row['num-of-errors'])
            require(int(row['time-step']) == next_time[q], 'Noncontiguous queue metric times')
            require(cumulative >= previous[q], 'Decreasing error counter')
            history[q].append((int(int(row['num-jobs-in-service']) > 0),
                               int(row['num-jobs-in-queue']), cumulative-previous[q]))
            previous[q], next_time[q] = cumulative, next_time[q]+1
    require(history and len(set(next_time.values())) == 1, 'Missing queue metric times')
    result = {}
    for q, samples in history.items():
        values = list(samples)
        result[q] = {}
        for window in (10, 100, 1000):
            tail = values[-window:]
            busy, waiting, errors = map(sum, zip(*tail))
            n = len(tail)
            for label, value in zip(('Utilization', 'Saturation', 'Errors'),
                                    ((busy+n//2)//n, (waiting+n//2)//n, errors)):
                result[q][f'{label}_{window}'] = value
    return result


def package_study(runner):
    """Finalize before packaging; only the external record contains the archive's own hash."""
    runner.finish()
    archive = pack_evidence(runner.evidence, runner.output / 'evidence.zip')
    save_json(runner.output / 'execution.json', dict(runner.report, evidence_archive=archive))
    return archive


def run_experiment(runner, name, folder, model, selected_queues, do_plots):
    """Simulate, validate, run all C tools, optionally plot. Return metrics dict."""
    timings = {}
    started = perf_counter()
    # Simulate
    runner.command([runner.binaries['E'], 'config.csv', '--metrics', 'qmet.csv', '--events', 'job.log'],
                   cwd=folder, timeout=900)
    timings['simulation_seconds'] = perf_counter() - started
    started = perf_counter()
    # Analysis copy (insert downstream arrivals)
    adapted = folder / 'analysis-input.log'
    inserted = compatible_events(folder / 'job.log', adapted, model, count_only=True)
    # Full accounting validation
    ledger = verify_case(folder, adapted, model)
    timings['validation_seconds'] = perf_counter() - started
    started = perf_counter()
    save_json(folder / 'adapter.json', {'inserted_arrivals': inserted})
    native = folder / 'native'
    native.mkdir()

    # C++ tools: derive-event-metrics, derive-sys-metrics
    runner.command([runner.binaries['derive-event-metrics'], adapted, native / 'visits.csv'], timeout=600)
    check_c_visits(native / 'visits.csv', ledger)
    runner.command([runner.binaries['derive-sys-metrics'], adapted, native / 'system.csv'], timeout=600)
    check_c_system(native / 'system.csv', ledger, model)

    # Summarize every queue once; demonstrate and cross-check native C on plotted queues.
    queue_metrics = terminal_use(folder / 'qmet.csv')
    for q in (selected_queues if do_plots else []):
        filtered = native / f'{q}-filtered.csv'
        increments = native / f'{q}-increments.csv'
        use = native / f'{q}-use.csv'
        runner.command([runner.binaries['filter-by-queue'], folder / 'qmet.csv', filtered, f'--queues={q}'])
        incremental_metrics(filtered, increments, q)
        runner.command([runner.binaries['compute-use'], increments, use])
        check_c_use(increments, use)
        # Grab final-step USE values
        use_rows = read_csv(use)
        if use_rows:
            last = use_rows[-1]
            native_values = {f'{m}_{w}': int(last[f'{m}({w})'])
                                for m in ['Utilization', 'Saturation', 'Errors'] for w in [10, 100, 1000]}
            require(native_values == queue_metrics[q], 'Streaming USE differs from native C')

    # C++ tool: filter-by-job (select one vehicle for tracing)
    selected_job = model['jobs'][min(1, len(model['jobs']) - 1)]['id']
    runner.command([runner.binaries['filter-by-job'], adapted, native / 'selected.log', f'--jobs={selected_job}'])

    # E pretty-print
    pretty = runner.source / 'implementation/obj-e/pretty-print-events/pretty-print-events.py'
    runner.command([sys.executable, pretty, adapted, '--jobs', selected_job, '--color', 'never',
                    '--legend', '--tsv', native / 'trace.tsv'])

    timings['analysis_seconds'] = perf_counter() - started
    started = perf_counter()
    # Python C plotting tools (only for selected runs)
    if do_plots:
        plots = native / 'plots'
        plots.mkdir()
        c_scripts = runner.source / 'implementation/obj-c'
        # plot-sys-metrics
        runner.command([sys.executable, c_scripts / 'plot-sys-metrics/plot_sys_metrics.py',
                        native / 'system.csv', plots / 'system-metrics.png', '--title', f'System Metrics: {name}'])
        # create-event-trace (on selected job's filtered log)
        runner.command([sys.executable, c_scripts / 'create-event-trace/create_event_trace.py',
                        native / 'selected.log', plots / 'event-trace.png', '--title', f'Event Trace: {selected_job}'])
        # plot-derive-event-metrics (histogram of response times)
        runner.command([sys.executable, c_scripts / 'plot-derive-event-metrics/plot_derive_event_metrics.py',
                        native / 'visits.csv', plots / 'response-hist.png', '--hist', 'response-time', '--bins', '20'], timeout=300)
        # plot-derive-event-metrics (scatter arrival vs turnaround)
        runner.command([sys.executable, c_scripts / 'plot-derive-event-metrics/plot_derive_event_metrics.py',
                        native / 'visits.csv', plots / 'arrival-vs-turnaround.png',
                        '--scat', 'arrival-time,turn-around-time'], timeout=300)
        # plot-use for selected queues
        for q in selected_queues:
            use_csv = native / f'{q}-use.csv'
            if use_csv.exists():
                runner.command([sys.executable, c_scripts / 'plot-use/plot_use.py',
                                use_csv, str(plots / f'{q}-use')])

    timings['plotting_seconds'] = perf_counter() - started
    # Collect per-run summary
    visits = Counter(v['queue'] for v in ledger['visits'])
    sys_summary = response_summary(ledger['completed'], model['jobs'])
    return dict(
        completed=len(ledger['completed']), dropped=len(ledger['dropped']),
        loss_rate=round(len(ledger['dropped']) / ledger['offered'], 4) if ledger['offered'] else 0,
        timesteps=ledger['metric_timesteps'], total_visits=sum(visits.values()),
        active_nodes=len(visits), inserted_arrivals=inserted,
        waiting_car_steps=ledger['waiting_car_steps'], service_car_steps=ledger['service_car_steps'],
        **sys_summary, **timings, queue_metrics=queue_metrics, selected_job=selected_job)


def select_queues(topology):
    """Pick 3-4 representative queues: exit, highest-flow interior, farthest peripheral."""
    dist = topology['distance']
    exits = set(topology['exits'])
    exit_q = topology['exits'][0]
    interior = [q for q in topology['nodes'] if q not in exits]
    farthest = max(interior, key=lambda q: dist[q])
    flow = topology.get('no_loss_visit_fraction', {})
    bottleneck = max(interior, key=lambda q: flow.get(q, 0)) if flow else interior[0]
    selected = list(dict.fromkeys([exit_q, bottleneck, farthest]))  # deduplicate preserving order
    return selected


def embed_png(path):
    """Return base64 data URI for a PNG, or empty string if missing."""
    if not path.exists():
        return ''
    return f'data:image/png;base64,{base64.b64encode(path.read_bytes()).decode()}'


def generate_report(scale, topology, manifest, results, selected_queues, evidence_dir, partial=False):
    """Build self-contained HTML report with embedded plots and tables."""
    dist = topology['distance']
    html = [f'''<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Objective F: {scale} Scale Study</title>
<style>
body {{ font-family: sans-serif; max-width: 1200px; margin: 0 auto; padding: 20px; }}
table {{ border-collapse: collapse; margin: 10px 0; }} th, td {{ border: 1px solid #ccc; padding: 4px 8px; text-align: right; }}
th {{ background: #f0f0f0; }} .left {{ text-align: left; }}
img {{ max-width: 100%; height: auto; margin: 5px 0; }}
h2 {{ border-bottom: 2px solid #333; padding-bottom: 4px; }}
.grid {{ display: flex; flex-wrap: wrap; gap: 10px; }}
.grid img {{ max-width: 48%; }}
</style></head><body>
<h1>Objective F: {scale} Texas Road Network Study</h1>
<p>Exit-directed synthetic traffic on a {len(topology["nodes"])}-node sample of SNAP roadNet-TX.
{len(topology["exits"])} exit(s), {len(topology["ingress"])} ingress nodes, max distance {max(dist.values())}.</p>''']

    if partial:
        html.append('<p><strong>Feasibility pilot / partial study.</strong> Only the selected manifest runs '
                    'are included. The full experiment matrix and scale conclusions remain pending.</p>')
    # Topology summary
    html.append(f'''<h2>Network Topology</h2>
<table><tr><th class="left">Queue</th><th>Distance</th><th>Servers (baseline)</th><th>Role</th><th>Outgoing</th></tr>''')
    for q in topology['nodes']:
        role = 'EXIT' if q in topology['exits'] else ('selected' if q in selected_queues else '')
        html.append(f'<tr><td class="left">{q}</td><td>{dist[q]}</td><td>4</td>'
                     f'<td class="left">{role}</td><td class="left">{", ".join(topology["outgoing"][q]) or "—"}</td></tr>')
    html.append('</table>')

    # Run summary table
    html.append('<h2>Run Summary</h2><table>')
    html.append('<tr><th class="left">Run</th><th>Demand</th><th>Seed</th><th>Servers</th>'
                '<th>Jobs</th><th>Completed</th><th>Dropped</th><th>Loss%</th><th>Steps</th><th>Mean latency</th></tr>')
    for row in manifest:
        r = results[row['run']]
        latency = '—' if r['mean_response'] is None else f'{r["mean_response"]:.3f}'
        html.append(f'<tr><td class="left">{row["run"]}</td><td>{row["demand"]}</td><td>{row["seed"]}</td>'
                     f'<td>{row["total_servers"]}</td><td>{row["actual_jobs"]}</td>'
                     f'<td>{r["completed"]}</td><td>{r["dropped"]}</td>'
                     f'<td>{r["loss_rate"]*100:.1f}</td><td>{r["timesteps"]}</td>'
                     f'<td>{latency}</td></tr>')
    html.append('</table>')

    # Resource sensitivity
    stressed_runs = [row for row in manifest if row['demand'] == 'stressed' and row['seed'] == '42']
    if len(stressed_runs) > 1:
        html.append('<h2>Resource Sensitivity (stressed, seed 42)</h2><table>')
        html.append('<tr><th>Server Δ</th><th>Total</th><th>Completed</th><th>Dropped</th><th>Loss%</th><th>Steps</th></tr>')
        for row in stressed_runs:
            r = results[row['run']]
            html.append(f'<tr><td>{row["server_change_percent"]}%</td><td>{row["total_servers"]}</td>'
                         f'<td>{r["completed"]}</td><td>{r["dropped"]}</td>'
                         f'<td>{r["loss_rate"]*100:.1f}</td><td>{r["timesteps"]}</td></tr>')
        html.append('</table>')

    # Plots section
    html.append('<h2>C Tool Demonstrations</h2><p>All 9 Objective C tools demonstrated:</p><ol>'
                '<li>filter-by-queue (C++): selected queue demonstrations; all-queue summaries use one streaming pass</li>'
                '<li>filter-by-job (C++): single-vehicle event selection</li>'
                '<li>compute-use (C++): USE window metrics</li>'
                '<li>derive-event-metrics (C++): per-visit timings</li>'
                '<li>derive-sys-metrics (C++): system response/throughput</li>'
                '<li>plot-use (Python): USE plots below</li>'
                '<li>plot-sys-metrics (Python): system metrics plot below</li>'
                '<li>plot-derive-event-metrics (Python): visit histograms/scatter below</li>'
                '<li>create-event-trace (Python): event timeline below</li></ol>')

    # Embed plots from selected runs
    for run_name in sorted(SELECTED_RUNS_FOR_PLOTS):
        plots_dir = evidence_dir / run_name / 'native' / 'plots'
        if not plots_dir.exists():
            continue
        html.append(f'<h3>Plots: {run_name}</h3>')
        # System metrics
        img = embed_png(plots_dir / 'system-metrics.png')
        if img:
            html.append(f'<img src="{img}" alt="System metrics: {run_name}">')
        # Event trace
        img = embed_png(plots_dir / 'event-trace.png')
        if img:
            html.append(f'<img src="{img}" alt="Event trace: {run_name}">')
        # Visit histograms
        html.append('<div class="grid">')
        for name in ['response-hist.png', 'arrival-vs-turnaround.png']:
            img = embed_png(plots_dir / name)
            if img:
                html.append(f'<img src="{img}" alt="{name}">')
        html.append('</div>')
        # USE plots for selected queues
        for q in selected_queues:
            use_dir = plots_dir / f'{q}-use'
            if use_dir.exists():
                html.append(f'<h4>USE: {q} (distance {dist[q]})</h4><div class="grid">')
                for png in sorted(use_dir.glob('*.png')):
                    html.append(f'<img src="{embed_png(png)}" alt="{png.stem}">')
                html.append('</div>')

    # Pretty-print excerpt
    baseline = evidence_dir / 'stressed-42-+0' / 'native' / 'trace.tsv'
    if baseline.exists():
        excerpt = '\n'.join(baseline.read_text(encoding='utf-8').splitlines()[:25])
        html.append(f'<h2>Pretty-Print Trace Excerpt (E tool)</h2><pre>{excerpt}</pre>')

    html.append(f'<hr><p>Generated {datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")}. '
                f'Synthetic model; see README for limitations.</p></body></html>')
    return '\n'.join(html)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scale', required=True, choices=SCALES)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--ref', default=DEFAULT_REF)
    parser.add_argument('--run', action='append', help='Select an existing manifest run (repeatable); leaves manifest unchanged')
    args = parser.parse_args()
    scale_dir = COMMON.parent / SCALES[args.scale]
    manifest = read_csv(scale_dir / 'experiments.csv')
    if args.run:
        require(set(args.run) <= {r['run'] for r in manifest}, 'Unknown manifest run')
        manifest = [r for r in manifest if r['run'] in args.run]
    output = new_output(args.output_root)

    # Build tools
    runner = Runner(output, args.ref)
    runner.checkpoint_commands = False
    evidence = runner.evidence
    runner.report['limitations'] = [f'{args.scale} scale study using prepared Texas road inputs.',
        'C activity is rounded 0/1; error windows are counts, not rates.',
        'Synthetic exit-directed traffic; not calibrated Texas data.']
    runner.report['study_scope'] = {'scale': args.scale, 'partial': bool(args.run),
        'selected_runs': [r['run'] for r in manifest],
        'inputs_sha256': sha(scale_dir / 'inputs.zip'),
        'manifest_sha256': sha(scale_dir / 'experiments.csv')}
    if args.run:
        runner.report['limitations'].append('Selected-run pilot only; full study remains pending.')
    runner.export()
    runner.build()
    print(f'Built E, B, and {len(C_TOOLS)} C++ tools.', flush=True)

    # Extract inputs
    inputs = output / 'inputs'
    topology, allocations = load_inputs(scale_dir / 'inputs.zip', inputs)
    selected_queues = select_queues(topology)
    print(f'Selected queues for plots: {selected_queues}', flush=True)

    # Run all experiments
    results = {}
    run_summaries, queue_summaries = [], []
    for row in manifest:
        name = row['run']
        print(f'Running {name}...', end=' ', flush=True)
        t0 = perf_counter()
        require(sha(inputs / row['workload']) == row['workload_sha256'], 'Prepared workload hash mismatch')
        model = build_model(topology, row, allocations, inputs)
        require(len(model['jobs']) == int(row['actual_jobs']), 'Prepared workload count mismatch')
        folder = evidence / name
        write_run(folder, model)
        do_plots = name in SELECTED_RUNS_FOR_PLOTS
        metrics = run_experiment(runner, name, folder, model, selected_queues, do_plots)
        elapsed = perf_counter() - t0
        metrics['elapsed_seconds'] = round(elapsed, 2)
        results[name] = metrics
        runner.report['cases'][name] = {k: v for k, v in metrics.items() if k != 'queue_metrics'}
        runner.save()
        print(f'{metrics["completed"]} completed, {metrics["dropped"]} dropped, {elapsed:.1f}s', flush=True)

        # Append to summary tables
        run_summaries.append(dict(run=name, demand=row['demand'], seed=row['seed'],
            server_change_percent=row['server_change_percent'], total_servers=row['total_servers'],
            actual_jobs=row['actual_jobs'], completed=metrics['completed'], dropped=metrics['dropped'],
            loss_rate=metrics['loss_rate'], timesteps=metrics['timesteps'], visits=metrics['total_visits'],
            active_nodes=metrics['active_nodes'], mean_response=metrics.get('mean_response', ''),
            peak_throughput=metrics.get('peak_throughput', ''),
            waiting_car_steps=metrics['waiting_car_steps'], service_car_steps=metrics['service_car_steps'],
            elapsed_seconds=metrics['elapsed_seconds'], **{k: metrics[k] for k in
                ('simulation_seconds', 'validation_seconds', 'analysis_seconds', 'plotting_seconds')}))

        dist = topology['distance']
        for q in topology['nodes']:
            qm = metrics['queue_metrics'].get(q, {})
            queue_summaries.append(dict(run=name, queue=q, distance=dist[q],
                is_exit=int(q in topology['exits']),
                **{k: qm.get(k, '') for k in ['Utilization_1000', 'Saturation_1000', 'Errors_1000']}))

    # Write consolidated CSVs
    write_csv(output / 'run-summary.csv', list(run_summaries[0]), run_summaries)
    write_csv(output / 'queue-summary.csv', list(queue_summaries[0]), queue_summaries)

    # Generate HTML report
    report_html = generate_report(args.scale, topology, manifest, results, selected_queues, evidence, bool(args.run))
    (output / 'report.html').write_text(report_html, encoding='utf-8')

    # Write execution record
    runner.report['cases'] = {name: {k: v for k, v in m.items() if k != 'queue_metrics'}
                              for name, m in results.items()}
    runner.report['selected_queues'] = selected_queues
    archive = package_study(runner)
    print(f'Evidence archive: {archive["bytes"]} bytes, {archive["members"]} members', flush=True)

    label = 'selected-run pilot' if args.run else 'study'
    print(f'\n{args.scale} {label} complete: {len(results)} runs, evidence at {output}', flush=True)
    print(f'Copy to scale directory:\n  run-summary.csv, queue-summary.csv, report.html, '
          f'evidence.zip, execution.json -> {scale_dir}/', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError) as exc:
        print(f'Study failed: {exc}', file=sys.stderr)
        sys.exit(1)
