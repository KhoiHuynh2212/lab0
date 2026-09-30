"""Prepare reproducible Texas road inputs and measure small feasibility pilots."""

import argparse
from collections import Counter, deque
from datetime import datetime, timezone
import gzip
import math
import os
from pathlib import Path
import shutil
import sys
from time import perf_counter

from run import Runner, DEFAULT_REF, new_output
from support import (COMMON, require, sha, save_json, read_csv, write_csv, orient_network,
                     write_case, compatible_events, verify_case, check_c_visits,
                     pack_evidence, unpack_evidence)

SCALES = {'SMALL': 20, 'MEDIUM': 200, 'LARGE': 2000}
DEMANDS = {'low': 0.5, 'stressed': 1.0, 'overloaded': 1.5}
CHANGES = [0, 25, 50, 100, -10, -20, -30]
URL = 'https://snap.stanford.edu/data/roadNet-TX.txt.gz'
DATASET_SHA256 = '1bc0ce058546ae66101d57af7c16de37225e37778ad2e3781b4f7bf3a28cb41f'


def read_network(path):
    """Deduplicate packed undirected pairs before constructing sorted adjacency."""
    edges, nodes = set(), set()
    records = loops = 0
    with gzip.open(path, 'rt', encoding='ascii') as stream:
        for line in stream:
            if not line.strip() or line.startswith('#'):
                continue
            a, b = map(int, line.split())
            require(0 <= a < 2**32 and 0 <= b < 2**32, 'Node outside packed ID range')
            records += 1
            nodes.update((a, b))
            if a == b:
                loops += 1
            else:
                a, b = sorted((a, b))
                edges.add((a << 32) | b)
    graph = {n: [] for n in nodes}
    for pair in edges:
        a, b = pair >> 32, pair & (2**32 - 1)
        graph[a].append(b)
        graph[b].append(a)
    for neighbors in graph.values():
        neighbors.sort()
    return graph, dict(nodes=len(nodes), undirected_edges=len(edges), input_records=records,
                       removed_self_loops=loops, duplicate_or_reverse_records=records-loops-len(edges))


def bfs(graph, start, limit):
    seen, queue, order = {start}, deque([start]), []
    while queue and len(order) < limit:
        n = queue.popleft()
        order.append(n)
        for v in graph[n]:
            if v not in seen:
                seen.add(v)
                queue.append(v)
    require(len(order) == limit, 'Starting component is too small')
    return order


def sample(graph, order):
    ids = {n: f'q{i}' for i, n in enumerate(order)}
    nodes = list(ids.values())
    edges = [(ids[n], ids[v]) for n in order for v in graph[n] if v in ids and n < v]
    boundary = {ids[n]: [v for v in graph[n] if v not in ids] for n in order
                if any(v not in ids for v in graph[n])}
    require(boundary, 'Sample has no external connections')
    original = {q: n for n, q in ids.items()}
    exits = [min(boundary, key=original.get)]
    # Spread exits by graph distance, with original node ID as the deterministic tie break.
    for _ in range(min(len(boundary), max(1, len(nodes)//20)) - 1):
        distance, _ = orient_network(nodes, edges, exits)
        exits.append(min(set(boundary)-set(exits), key=lambda q: (-distance[q], original[q])))
    distance, outgoing = orient_network(nodes, edges, exits)
    ingress = [q for q in nodes if q not in exits]
    flow = {q: (1 / len(ingress) if q in ingress else 0) for q in nodes}
    for q in sorted(nodes, key=lambda q: -distance[q]):
        for v in outgoing[q]:
            flow[v] += flow[q] / len(outgoing[q])
    return dict(nodes=nodes, edges=edges, exits=exits, distance=distance, outgoing=outgoing,
                settings={q: dict(servers=4, capacity=20, rate=1) for q in nodes},
                original_ids=original, boundary=boundary, ingress=ingress,
                no_loss_visit_fraction=flow, nominal_bottleneck_arrivals_per_step=4/max(flow.values()))


def allocate_servers(nodes, change):
    """Exact network-wide percentage, reducing in recorded BFS order in rounds."""
    target = 4 * len(nodes) * (100 + change) // 100
    require(100 * target == 4 * len(nodes) * (100 + change), 'Nonintegral resource target')
    base, extra = divmod(target, len(nodes))
    require(base >= 1, 'Resource allocation would remove all servers')
    # For reductions, lower-ranked nodes lose capacity first.
    return {q: base + int(i >= len(nodes)-extra) for i, q in enumerate(nodes)}


def workload(runner, topology, folder, demand, seed, target):
    folder.mkdir(parents=True)
    nominal = topology['nominal_bottleneck_arrivals_per_step'] * DEMANDS[demand]
    horizon = max(1, math.ceil(target / nominal))
    rate = target / horizon
    original = folder / 'original-A.csv'
    runner.command([runner.binaries['A'], '--out', original, '--interval', f'0,{horizon-1}',
                    '--distribution', 'poisson', '--lambda', rate, '--tasks-per-job', 1,
                    '--task-work', 'fixed', '--fixed-task-work', 1, '--seed', seed])
    rows = read_csv(original)
    ingress = topology['ingress']
    jobs = [{'id': row['Job Id'], 'arrival': int(row['arrival time']),
             'entry': ingress[i % len(ingress)], 'work': topology['distance'][ingress[i % len(ingress)]] + 1}
            for i, row in enumerate(rows)]
    require(len({j['id'] for j in jobs}) == len(jobs), 'Duplicate A ID')
    write_csv(folder / 'vehicles.csv', ['id', 'arrival', 'entry', 'work'], jobs)
    return jobs, dict(seed=seed, demand=demand, target_jobs=target, actual_jobs=len(jobs),
                      horizon=horizon, poisson_lambda=rate, nominal_pressure=rate/topology['nominal_bottleneck_arrivals_per_step'],
                      original_A_sha256=sha(original), vehicles_sha256=sha(folder/'vehicles.csv'))


def build_tools(runner):
    runner.export(additional=('implementation/obj-a/workload-generator',))
    runner.build(only_e=True)
    for name, location, stem in [('A', 'obj-a/workload-generator', 'workload_generator'),
                                 ('visits', 'obj-c/derive-event-metrics', 'derive_event_metrics')]:
        folder = runner.source / 'implementation' / location
        runner.binaries[name] = runner.compile_program(name,
            [folder/'src/main.cpp', folder/f'src/{stem}.cpp'], [folder/'include'], ['-std=c++20', '-O2'])


def pilot(runner, topology, scale, demand, target):
    jobs, info = workload(runner, topology, runner.evidence/f'pilot-{scale}-{demand}-input', demand, 42, target)
    model = dict(topology, jobs=jobs, horizon=info['horizon'], time_limit=None)
    folder = runner.evidence/f'pilot-{scale}-{demand}'
    write_case(folder, model)
    start = perf_counter()
    runner.command([runner.binaries['E'], 'config.csv', '--metrics', 'qmet.csv', '--events', 'job.log'],
                   cwd=folder, timeout=180)
    simulation = perf_counter()-start
    start = perf_counter()
    compatible_events(folder/'job.log', folder/'analysis-input.log', model)
    ledger = verify_case(folder, folder/'analysis-input.log', model)
    accounting = perf_counter()-start
    start = perf_counter()
    runner.command([runner.binaries['visits'], folder/'analysis-input.log', folder/'visits.csv'])
    check_c_visits(folder/'visits.csv', ledger)
    analysis = perf_counter()-start
    files = ['qmet.csv', 'job.log', 'completed.wl']
    raw = sum((folder/f).stat().st_size for f in files)
    from zipfile import ZipFile, ZIP_DEFLATED
    archive = runner.output/f'pilot-{scale}-{demand}.zip'
    with ZipFile(archive, 'x', compression=ZIP_DEFLATED, compresslevel=9) as z:
        for f in files:
            z.write(folder/f, f)
    visits = Counter(v['queue'] for v in ledger['visits'])
    summary = dict(info, scale=scale, completed=len(ledger['completed']), dropped=len(ledger['dropped']),
                   fully_drained=ledger['fully_drained'], metric_rows=ledger['metric_timesteps']*len(topology['nodes']),
                   timesteps=ledger['metric_timesteps'], active_nodes=len(visits), total_nodes=len(topology['nodes']),
                   visits=sum(visits.values()), simulation_seconds=simulation, accounting_seconds=accounting,
                   native_C_seconds=analysis, raw_bytes=raw, compressed_raw_bytes=archive.stat().st_size,
                   raw_sha256={f: sha(folder/f) for f in files}, per_queue_visits=dict(visits))
    save_json(folder/'verification.json', {k:v for k,v in summary.items() if k != 'per_queue_visits'})
    print(f'{scale} {demand}: {len(jobs)} jobs, {summary["completed"]} completed, '
          f'{summary["dropped"]} dropped, {len(visits)}/{len(topology["nodes"])} active; '
          f'{simulation:.2f}s simulation, {accounting:.2f}s accounting', flush=True)
    return summary


def prepare(runner, dataset):
    require(sha(dataset) == DATASET_SHA256, 'Dataset differs from the recorded Texas download')
    graph, counts = read_network(dataset)
    require((counts['nodes'], counts['undirected_edges']) == (1379917, 1921660), 'Unexpected Texas dataset counts')
    order = bfs(graph, min(graph), 2000)
    topologies = {scale: sample(graph, order[:n]) for scale, n in SCALES.items()}
    del graph
    source = dict(url=URL, page='https://snap.stanford.edu/data/roadNet-TX.html', sha256=sha(dataset),
                  compressed_bytes=dataset.stat().st_size, observed_at_utc=datetime.now(timezone.utc).isoformat(),
                  citation='Leskovec, Lang, Dasgupta, Mahoney. Community Structure in Large Networks: Natural Cluster Sizes and the Absence of Large Well-Defined Clusters. Internet Mathematics 6(1), 29-123, 2009.',
                  parsed=counts, rules=dict(sampling='BFS from minimum original ID, ascending original neighbors; nested prefixes 20/200/2000; all induced edges retained.',
                  exits='Boundary nodes only; start with minimum original ID, then farthest from current exits; ties by original ID. Count=min(boundary count, nodes//20).',
                  ingress='All non-exit nodes in BFS order; round-robin assignment of original A arrival order. Trips originate throughout the sampled area.',
                  direction='Original edges with exit distance decreasing by exactly one; same-level edges omitted.',
                  workload='Native Poisson A arrivals, inclusive interval 0..H-1; preserve IDs and times; replace placeholder work with ingress distance+1.',
                  demand='No-loss uniform-ingress flow split equally on outgoing edges; bottleneck arrival capacity=4/max(node visit fraction). Pressure targets 0.5/1/1.5; H=ceil(target/nominal rate), lambda=target/H.',
                  resources='4 servers, 20 waiting spaces, Sub-1 duration 1. Exact total server percentages; reductions remove servers in BFS-order rounds.'),
                  source_commit=runner.report['source_commit'], scales={}, pilots=[])
    generator = runner.source/'implementation/obj-e/create-config-from-network/create-config-from-network.py'
    for scale, topology in topologies.items():
        folder = runner.output/'inputs'/scale
        folder.mkdir(parents=True)
        save_json(folder/'topology.json', topology)
        directed = folder/'directed.txt'
        directed.write_text(''.join(f'{q[1:]} {v[1:]}\n' for q in topology['nodes'] for v in topology['outgoing'][q]), encoding='ascii')
        runner.command([sys.executable, generator, directed, folder/'E-generated.csv', '--servers', 4,
                        '--max-queue-size', 20, '--server-func', 'Sub-1', '--rate', 1])
        generated = read_csv(folder/'E-generated.csv')
        require({r['queue_sys_id'] for r in generated} == set(topology['nodes']), 'E generator omitted nodes')
        require(all(r['edges_out'].split(';') == topology['outgoing'][r['queue_sys_id']] if r['edges_out']
                    else not topology['outgoing'][r['queue_sys_id']] for r in generated), 'E generator changed directions')
        source['scales'][scale] = dict(nodes=len(topology['nodes']), original_edges=len(topology['edges']),
            directed_edges=sum(map(len, topology['outgoing'].values())), boundaries=len(topology['boundary']),
            exits=len(topology['exits']), ingresses=len(topology['ingress']), maximum_distance=max(topology['distance'].values()),
            nominal_bottleneck_arrivals_per_step=topology['nominal_bottleneck_arrivals_per_step'])
        for demand in DEMANDS:
            source['pilots'].append(pilot(runner, topology, scale, demand, 2000))
        manifest, workloads = [], {}
        for demand in DEMANDS:
            for seed in [42, 43, 44]:
                key = f'{demand}-{seed}'
                jobs, info = workload(runner, topology, folder/'workloads'/key, demand, seed, 100*len(topology['nodes']))
                require(50*len(topology['nodes']) <= len(jobs) <= 500*len(topology['nodes']), 'Final workload outside assignment range')
                workloads[key] = info
                for change in (CHANGES if demand == 'stressed' else [0]):
                    servers = allocate_servers(topology['nodes'], change)
                    manifest.append(dict(run=f'{key}-{change:+d}', demand=demand, seed=seed, server_change_percent=change,
                        total_servers=sum(servers.values()), workload=f'workloads/{key}/vehicles.csv',
                        workload_sha256=info['vehicles_sha256'], actual_jobs=info['actual_jobs'],
                        horizon=info['horizon'], poisson_lambda=info['poisson_lambda']))
        save_json(folder/'workloads.json', workloads)
        save_json(folder/'server-allocations.json', {str(c): allocate_servers(topology['nodes'], c) for c in CHANGES})
        write_csv(runner.output/f'{scale}-experiments.csv', list(manifest[0]), manifest)
        record = pack_evidence(folder, runner.output/f'{scale}-inputs.zip')
        unpack_evidence(runner.output/f'{scale}-inputs.zip', runner.output/'archive-check'/scale)
        source['scales'][scale]['inputs_archive'] = record
        source['scales'][scale]['workloads'] = workloads
        save_json(runner.output/'source.json', source)
    # Linear pilot extrapolation is a feasibility estimate, not a measured final result.
    projections = {}
    for scale, n in SCALES.items():
        pilots = [p for p in source['pilots'] if p['scale'] == scale]
        projections[scale] = dict(estimated_raw_evidence_bytes=sum(p['compressed_raw_bytes']*(100*n/p['actual_jobs'])*(21 if p['demand']=='stressed' else 3) for p in pilots),
            estimated_pipeline_seconds=sum((p['simulation_seconds']+p['accounting_seconds']+p['native_C_seconds'])*(100*n/p['actual_jobs'])*(21 if p['demand']=='stressed' else 3) for p in pilots))
    source['feasibility'] = dict(projections=projections, total_estimated_compressed_raw_bytes=sum(p['estimated_raw_evidence_bytes'] for p in projections.values()),
        method='Per-demand pilot raw ZIP size and runtime scaled by final target/pilot realized jobs; 3 low + 21 stressed + 3 overloaded per scale. Excludes analysis copies, native C outputs, plots and inputs; ZIP ratios and runtime may change.',
        peak_memory='Not measured. Full-size accounting and C memory costs remain to be checked before large batches.',
        matrix_status='81-run design prepared; execution/storage gate pending measured projection review.')
    save_json(runner.output/'source.json', source)
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dataset', type=Path, required=True, help='Downloaded roadNet-TX.txt.gz (kept in ignored cache)')
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--ref', default=DEFAULT_REF)
    args = parser.parse_args()
    runner = Runner(new_output(args.output_root), args.ref)
    runner.report['limitations'] = ['Preparation and reduced feasibility pilots; no final MEDIUM/LARGE results.',
                                     'Synthetic demand/directions/resources; road connectivity is the only observed input.']
    try:
        build_tools(runner)
        source = prepare(runner, args.dataset)
        runner.report['cases'] = {f'{p["scale"]}-{p["demand"]}': p for p in source['pilots']}
        runner.finish()
        source['preparation_execution'] = {k: v for k, v in runner.report.items() if k != 'cases'}
        save_json(runner.output/'source.json', source)
        print('Preparation passed; artifacts:', runner.output, flush=True)
    except Exception as exc:
        runner.report.update(status='failed', error=str(exc))
        runner.save()
        raise


if __name__ == '__main__':
    main()
