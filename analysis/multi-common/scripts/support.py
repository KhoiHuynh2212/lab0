"""F-only graph, evidence and compatibility checks; no shared simulator imports."""

import csv
import hashlib
import json
import re
from collections import Counter, defaultdict, deque
from itertools import groupby
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

COMMON = Path(__file__).resolve().parents[1]
REPO = COMMON.parents[1]
CONFIG_FIELDS = ['queue_sys_id', 'max_queue_size', 'num_servers', 'server_function',
                 'server_function_rate', 'source', 'sink', 'edges_in', 'edges_out']
METRIC_FIELDS = ['time-step', 'queue-sys-id', 'num-jobs-in-queue',
                 'num-jobs-in-service', 'num-of-errors']
EVENT_HEADER = 'time job IN queue event details'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256') if hasattr(hashlib, 'file_digest') else None
        if digest is not None:
            return digest.hexdigest()
        value = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
        return value.hexdigest()


def read_csv(path):
    with Path(path).open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def write_csv(path, fields, rows):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def save_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def orient_network(nodes, edges, exits):
    """Return distances and only original connections that reduce exit distance."""
    require(len(nodes) == len(set(nodes)) and nodes, 'Duplicate or empty node set')
    require(set(exits) and set(exits) <= set(nodes), 'Missing or unknown exit')
    neighbors = {q: set() for q in nodes}
    for a, b in edges:
        require(a in neighbors and b in neighbors and a != b, 'Invalid graph edge')
        neighbors[a].add(b)
        neighbors[b].add(a)
    distance = {q: 0 for q in exits}
    queue = deque(sorted(exits))
    while queue:
        q = queue.popleft()
        for other in sorted(neighbors[q]):
            if other not in distance:
                distance[other] = distance[q] + 1
                queue.append(other)
    require(set(distance) == set(nodes), 'Node cannot reach an exit')
    return distance, {q: sorted(v for v in neighbors[q] if distance[v] == distance[q] - 1)
                      for q in nodes}


def validate_model(model):
    nodes = model['nodes']
    require(all(re.fullmatch(r'q[0-9]+', q) for q in nodes), 'Unsafe queue ID')
    distance, outgoing = orient_network(nodes, model['edges'], model['exits'])
    require(model['distance'] == distance and model['outgoing'] == outgoing,
            'Direction or distance differs from the exit-directed graph')
    ids = set()
    for q in nodes:
        settings = model['settings'][q]
        require(settings['servers'] >= 1 and settings['capacity'] >= 0 and settings['rate'] >= 1,
                'Invalid server, capacity or rate setting')
    for job in model['jobs']:
        require(re.fullmatch(r'j[A-Za-z0-9_-]+', job['id']) is not None, 'Job ID must use a safe lowercase j prefix for C')
        require(job['id'] not in ids, 'Duplicate external job ID')
        ids.add(job['id'])
        require(job['entry'] in nodes and job['arrival'] >= 0, 'Invalid external arrival')
        require(job['work'] == distance[job['entry']] + 1, 'Work does not match exit distance')


def write_case(folder, model):
    validate_model(model)
    folder.mkdir(parents=True, exist_ok=False)
    rows = []
    for q in model['nodes']:
        jobs = sorted((j for j in model['jobs'] if j['entry'] == q), key=lambda j: j['arrival'])
        source = f'{q}.wl' if jobs else ''
        if jobs:
            write_csv(folder / source, ['Job Id', 'arrival time', 'Tasks'],
                      [{'Job Id': j['id'], 'arrival time': j['arrival'],
                        'Tasks': repr([f"0: W {j['work']}"])} for j in jobs])
        settings = model['settings'][q]
        rows.append(dict(zip(CONFIG_FIELDS, [q, settings['capacity'], settings['servers'],
            'Sub-1', settings['rate'], source, 'completed.wl' if q in model['exits'] else '',
            ';'.join(v for v in model['nodes'] if q in model['outgoing'][v]),
            ';'.join(model['outgoing'][q])])))
    write_csv(folder / 'config.csv', CONFIG_FIELDS, rows)
    save_json(folder / 'model.json', model)


def iter_events(path):
    """Read ordered events incrementally; reject regressions even across batch boundaries."""
    previous = -1
    with Path(path).open(encoding='utf-8') as stream:
        for line in stream:
            line = line.rstrip('\r\n')
            if not line.strip() or line == EVENT_HEADER:
                continue
            fields = line.split(maxsplit=5)
            require(len(fields) == 6 and fields[0].isdigit() and fields[2] == 'IN', 'Malformed event')
            t = int(fields[0])
            require(t >= previous, 'Events not in time order')
            previous = t
            yield (t, fields[1], fields[3], fields[4], fields[5])


def parse_events(path):
    """List compatibility helper for small fixture consumers."""
    return list(iter_events(path))


def metric_snapshots(path, nodes):
    """Yield exactly one complete snapshot at a time, including idle timesteps."""
    with Path(path).open(newline='', encoding='utf-8') as stream:
        rows = csv.DictReader(stream)
        for expected_t, (t, group) in enumerate(groupby(rows, lambda r: int(r['time-step']))):
            require(t == expected_t, 'Noncontiguous metric times')
            snapshot = {}
            for row in group:
                q = row['queue-sys-id']
                require(q in nodes and q not in snapshot, 'Unknown or duplicate metric row')
                snapshot[q] = [int(row[k]) for k in METRIC_FIELDS[2:]]
            require(set(snapshot) == nodes, 'Missing queue snapshot')
            yield t, snapshot


def compatible_events(source, destination, model, *, count_only=False):
    """Infer only model-valid same-tick transfers; the independent ledger must verify delivery."""
    require(source.resolve() != destination.resolve() and not destination.exists(), 'Refusing log overwrite')
    validate_model(model)
    nodes = set(model['nodes'])
    inserted, count = [], 0
    with destination.open('x', encoding='utf-8') as output:
        output.write(EVENT_HEADER + '\n')
        for _, group in groupby(iter_events(source), lambda e: e[0]):
            events = list(group)
            existing = Counter((t, j, q, via) for t, j, q, action, via in events if action == 'ARRIVE-VIA')
            for t, job, origin, action, target in events:
                require(origin in nodes, 'Unknown event queue')
                output.write(f'{t} {job} IN {origin} {action} {target}\n')
                if action == 'DEPART-VIA' and target in nodes:
                    require(target in model['outgoing'][origin], 'Transfer uses a forbidden edge')
                    key = (t, job, target, origin)
                    require(existing[key] <= 1, 'Duplicate downstream arrival')
                    if not existing[key]:
                        output.write(f'{t} {job} IN {target} ARRIVE-VIA {origin}\n')
                        count += 1
                        if not count_only:
                            inserted.append({'time': t, 'job': job, 'from': origin, 'to': target})
    return count if count_only else inserted


def verify_case(folder, adapted, model):
    """Reconcile every event with the original E snapshots, including internally lost work."""
    validate_model(model)
    nodes = set(model['nodes'])
    config = read_csv(folder / 'config.csv')
    require(len(config) == len(model['nodes']) and [r['queue_sys_id'] for r in config] == model['nodes'],
            'Configuration queue set/order differs from model')
    for row in config:
        q = row['queue_sys_id']
        settings = model['settings'][q]
        source = f'{q}.wl' if any(j['entry'] == q for j in model['jobs']) else ''
        require([row[k] for k in CONFIG_FIELDS[1:]] == [str(settings['capacity']), str(settings['servers']),
                'Sub-1', str(settings['rate']), source, 'completed.wl' if q in model['exits'] else '',
                ';'.join(v for v in model['nodes'] if q in model['outgoing'][v]), ';'.join(model['outgoing'][q])],
                'Configuration settings differ from model')
        if source:
            expected = [{'Job Id': j['id'], 'arrival time': str(j['arrival']), 'Tasks': repr([f"0: W {j['work']}"])}
                        for j in sorted((j for j in model['jobs'] if j['entry'] == q), key=lambda j: j['arrival'])]
            require(read_csv(folder / source) == expected, 'Source workload differs from model')
    events = iter_events(adapted)
    event = next(events, None)
    offered = {j['id']: j for j in model['jobs']}
    arrivals, completed, dropped = set(), {}, {}
    current, transfers, servers, visits = {}, {}, {}, {}
    errors = Counter()
    area_wait, area_busy = Counter(), Counter()
    boundary = 0
    for t, snapshot in metric_snapshots(folder / 'qmet.csv', nodes):
        boundary = t + 1
        while event is not None and event[0] == t:
            _, j, q, action, detail = event
            event = next(events, None)
            require(j in offered and q in nodes, 'Unknown job or queue')
            key = (j, q)
            if action == 'ARRIVE-VIA':
                require(j not in current and key not in visits, 'Duplicate or overlapping visit')
                if detail.endswith('.wl'):
                    spec = offered[j]
                    require(j not in arrivals and detail == f'{q}.wl' and
                            (q, t) == (spec['entry'], spec['arrival']), 'Incorrect external arrival')
                    arrivals.add(j)
                else:
                    require(transfers.pop(j, None) == (q, t, detail), 'Arrival lacks matching transfer')
                visits[key] = dict(job=j, queue=q, arrival=t, start=None, stop=None, end=None,
                                   outcome='unfinished', task_starts=0, task_stops=0)
                current[j] = q
                continue
            require(current.get(j) == q and key in visits, 'Event lacks an active visit')
            visit = visits[key]
            if action == 'ENTERS-SERVER':
                slot = (q, int(detail))
                require(0 <= slot[1] < model['settings'][q]['servers'] and slot not in servers and
                        visit['start'] is None, 'Invalid or overlapping service')
                servers[slot] = j
                visit['slot'], visit['start'] = slot, t
            elif action in {'TASK-START', 'TASK-STOP'}:
                require(visit['start'] is not None and visit['stop'] is None, 'Task outside service')
                require(detail == f'0 "W {model["distance"][q] + 1}"', 'Unexpected remaining work')
                name = 'task_starts' if action == 'TASK-START' else 'task_stops'
                visit[name] += 1
                expected_t = visit['start'] if action == 'TASK-START' else visit['start'] + model['settings'][q]['rate']
                require(t == expected_t and visit[name] == 1, 'Wrong task timing or duplicate operation')
            elif action == 'EXITS-SERVER':
                require(visit['start'] is not None and visit['stop'] is None and
                        servers.pop((q, int(detail)), None) == j, 'Invalid service exit')
                require(t - visit['start'] == model['settings'][q]['rate'] and
                        visit['task_starts'] == visit['task_stops'] == 1, 'Incomplete service evidence')
                visit['stop'] = t
            elif action == 'DEPART-VIA':
                require(visit['stop'] == t, 'Departure without completed service')
                visit.update(end=t, outcome='transferred' if detail in nodes else 'completed')
                del current[j]
                if detail in nodes:
                    require(detail in model['outgoing'][q], 'Invalid route')
                    transfers[j] = (detail, t, q)
                else:
                    require(q in model['exits'] and detail == 'completed.wl' and j not in completed,
                            'Incorrect or duplicate final completion')
                    completed[j] = t
            elif action == 'ERROR':
                require(detail == 'QUEUE-FULL' and visit['start'] is None and visit['arrival'] == t,
                        'Unexpected error or non-immediate rejection')
                errors[q] += 1
                dropped[j] = t
                visit.update(end=t, outcome='dropped')
                del current[j]
            else:
                raise ValueError(f'Unknown event: {action}')
        require(not transfers, 'Unaccounted transfer at metric boundary')
        busy_counts = Counter(slot[0] for slot in servers)
        present_counts = Counter(current.values())
        for q, actual in snapshot.items():
            busy = busy_counts[q]
            waiting = present_counts[q] - busy
            require(actual == [waiting, busy, errors[q]], 'Event/occupancy/error mismatch')
            require(0 <= waiting <= model['settings'][q]['capacity'], 'Waiting capacity exceeded')
            area_wait[q] += waiting
            area_busy[q] += busy
    require(boundary, 'Missing queue measurements')
    require(event is None, 'Event outside observation')
    if model.get('time_limit') is not None:
        require(boundary == model['time_limit'], 'Wrong time-limit boundary')
    require(arrivals == {j for j, spec in offered.items() if spec['arrival'] < boundary}, 'Missing external job')
    require(len(arrivals) == len(completed) + len(dropped) + len(current), 'Network accounting mismatch')
    if model.get('time_limit') is None:
        require(not current and len(arrivals) == len(offered), 'Undrained or lost job')
    sink = read_csv(folder / 'completed.wl')
    require(len(sink) == len(completed) and len({r['Job Id'] for r in sink}) == len(sink), 'Sink count/ID mismatch')
    require({r['Job Id']: int(r['arrival time']) for r in sink} == completed and
            all(r['Tasks'] == '[]' for r in sink), 'Sink does not match terminal events')
    residence_wait, residence_busy = Counter(), Counter()
    for visit in visits.values():
        q = visit['queue']
        end = boundary if visit['end'] is None else visit['end']
        start = end if visit['start'] is None else visit['start']
        residence_wait[q] += start - visit['arrival']
        if visit['start'] is not None:
            residence_busy[q] += (end if visit['stop'] is None else visit['stop']) - visit['start']
    for q in model['nodes']:
        require([residence_wait[q], residence_busy[q]] == [area_wait[q], area_busy[q]], 'Residence/occupancy mismatch')
    h = model['horizon']
    before = sum(t < h for t in completed.values())
    # Independent hand-calculated outcomes apply only when explicitly supplied.
    if 'expected_completed' in model:
        require(completed == model['expected_completed'], 'Hand-calculated completions differ')
    if 'expected_dropped' in model:
        require(dropped == model['expected_dropped'], 'Hand-calculated losses differ')
    return {'offered': len(offered), 'arrived': len(arrivals), 'completed': completed, 'dropped': dropped,
            'unfinished': sorted(current), 'unarrived': sorted(set(offered) - arrivals),
            'completed_before_horizon': before, 'metric_timesteps': boundary,
            'waiting_car_steps': sum(area_wait.values()), 'service_car_steps': sum(area_busy.values()),
            'visits': list(visits.values()), 'fully_drained': not current and len(arrivals) == len(offered)}


def incremental_metrics(source, target, queue):
    require(not target.exists() and source.resolve() != target.resolve(), 'Refusing metric overwrite')
    rows = read_csv(source)
    previous = 0
    for t, row in enumerate(rows):
        require(row['queue-sys-id'] == queue and int(row['time-step']) == t, 'Unfiltered or missing metric time')
        cumulative = int(row['num-of-errors'])
        require(cumulative >= previous, 'Decreasing cumulative errors')
        row['num-of-errors'] = cumulative - previous
        previous = cumulative
    write_csv(target, METRIC_FIELDS, rows)


def check_c_visits(path, ledger):
    remaining = {(v['job'], v['queue']): v for v in ledger['visits']}
    count = 0
    with Path(path).open(newline='', encoding='utf-8') as stream:
        for row in csv.DictReader(stream):
            v = remaining.pop((row['job-id'], row['queue-sys-id']), None)
            require(v is not None, 'C omitted or duplicated a queue visit')
            wait = '' if v['start'] is None else str(v['start'] - v['arrival'])
            service = 0 if v['stop'] is None else v['stop'] - v['start']
            total = '' if v['end'] is None else str(v['end'] - v['arrival'])
            queued = str(int(total) - service) if total != '' else wait
            require([row[k] for k in ['arrival-time', 'time-in-queue', 'time-in-service',
                    'response-time', 'turn-around-time']] == [str(v['arrival']), queued, str(service), wait, total],
                    'C visit timing differs from checked events')
            count += 1
    require(not remaining, 'C omitted or duplicated a queue visit')
    return count


def check_c_system(path, ledger, model):
    arrivals = {j['id']: j['arrival'] for j in model['jobs']}
    buckets = defaultdict(list)
    for j, t in ledger['completed'].items():
        buckets[t].append(t - arrivals[j])
    rows = read_csv(path)
    require([int(r['time-step']) for r in rows] == sorted(buckets), 'Wrong system departure timestamps')
    for row in rows:
        values = sorted(buckets[int(row['time-step'])])
        n = len(values)
        median = values[n // 2] if n % 2 else (values[n // 2 - 1] + values[n // 2] + 1) // 2
        expected = [(sum(values) + n // 2) // n, median,
                    values[(10 * n + 99) // 100 - 1], values[(90 * n + 99) // 100 - 1], n]
        require([int(row[k]) for k in ['Avg-response-time', 'median-response-time',
                '10-response-time', '90-response-time', 'throughput']] == expected,
                'C system values differ from final sink completions')
    return len(rows)


def check_c_use(increments, output):
    rows, actual = read_csv(increments), read_csv(output)
    require(len(rows) == len(actual), 'C USE row count mismatch')
    history = {w: deque() for w in (10, 100, 1000)}
    totals = {w: [0, 0, 0] for w in history}
    for t, (row, measured) in enumerate(zip(rows, actual)):
        require(int(measured['time-step']) == t, 'C USE time mismatch')
        sample = [int(int(row['num-jobs-in-service']) > 0), int(row['num-jobs-in-queue']), int(row['num-of-errors'])]
        for w in history:
            history[w].append(sample)
            old = history[w].popleft() if len(history[w]) > w else [0, 0, 0]
            totals[w] = [a + b - c for a, b, c in zip(totals[w], sample, old)]
            n = len(history[w])
            expected = [(totals[w][0] + n // 2) // n, (totals[w][1] + n // 2) // n, totals[w][2]]
            require([int(measured[f'{label}({w})']) for label in ['Utilization', 'Saturation', 'Errors']] == expected,
                    'Changed C activity/saturation/error-window contract')
    return len(rows)


def pack_evidence(folder, archive):
    require(not archive.exists(), 'Refusing archive overwrite')
    require(not archive.resolve().is_relative_to(folder.resolve()), 'Archive must be outside its inputs')
    members = {p.relative_to(folder).as_posix(): sha(p) for p in sorted(folder.rglob('*')) if p.is_file()}
    require('checksums.json' not in members, 'Reserved archive index filename')
    with ZipFile(archive, 'x', ZIP_DEFLATED, compresslevel=9) as output:
        for name in members:
            output.write(folder / name, name)
        output.writestr('checksums.json', json.dumps(members, indent=2) + '\n')
    return {'sha256': sha(archive), 'bytes': archive.stat().st_size, 'members': len(members) + 1}


def unpack_evidence(archive, destination):
    require(not destination.exists(), 'Refusing extraction overwrite')
    with ZipFile(archive) as source:
        names = source.namelist()
        require(len(names) == len(set(names)), 'Duplicate archive member')
        for name in names:
            require(not ('\\' in name or ':' in name) and not Path(name).is_absolute() and
                    '..' not in Path(name).parts, 'Unsafe archive member')
        index = json.loads(source.read('checksums.json'))
        require(set(index) == set(names) - {'checksums.json'}, 'Archive index mismatch')
        for name, digest in index.items():
            value = hashlib.sha256()
            with source.open(name) as member:
                for block in iter(lambda: member.read(1024 * 1024), b''):
                    value.update(block)
            require(value.hexdigest() == digest, 'Archive checksum mismatch')
        destination.mkdir(parents=True)
        source.extractall(destination)
    return index
