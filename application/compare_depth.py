#!/usr/bin/env python3
"""Compare the depths of the two CFS schedules under the same workspace cap.

Run reproduce.py first. This reuses its circuit costs and public CFS trace,
and regenerates block cleanup through the current shared C++ scheduler.
OR-Tools is only used to schedule these fixed traces; it does not choose cuts.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import random

from ortools.sat.python import cp_model
import ortools

from circuit_model import Cost, Model
from cut_planner import CutPlanner
from shared_scheduler import SharedScheduler

HERE = Path(__file__).resolve().parent
CASES = ["p224", "p256", "p256-second-prime"]


def read_case(path):
    data = json.loads(path.read_text())
    spec = data['model']
    costs = {(c['op'], c['vertex']): Cost(c['work'], c['scratch']) for c in spec['costs']}
    model = Model(spec['children'], set(spec['leaves']), spec['root'],
                  data['node_width'], data['fixed_qubits'], costs)
    baseline = data['rows'][data['baseline']]['trace']
    assert model.check(baseline)['work'] == data['rows'][data['baseline']]['work']
    with SharedScheduler(model) as scheduler:
        trace, decisions = CutPlanner(model, data['cap'], scheduler).run()
    assert model.check(trace)['work'] == data['rows'][data['best']]['work']
    return data, model, {'cfs': baseline, 'block_cleanup': trace}, decisions


def active(model, op, v):
    return [v] + (model.children[v] if op != 'G' else [])


def counted(model, op, v, metric):
    if metric == 'kernel':
        return op != 'G'
    return v in model.leaves and (op == 'P' or metric == 'symmetric')


def dependencies(model, trace):
    last, edges = {}, set()
    for i, (op, v) in enumerate(trace):
        for x in active(model, op, v):
            if x in last:
                edges.add((last[x], i))
            last[x] = i
    return sorted(edges)


def resource_bounds(model, trace, cap):
    available = cap - model.fixed
    unused, consumers, heavy = set(), [], []
    forward, inverse, query_volume = 0, 0, 0
    for op, v in trace:
        if v in model.leaves:
            query_volume += model.width + model.cost(op, v).scratch
            if op == 'P':
                forward += 1
                unused.add(v)
            else:
                assert op == 'U' and v not in unused, 'A lookup was erased without being used'
                inverse += 1
        elif op != 'G':
            children = model.children[v]
            extra = model.cost(op, v).scratch + model.width * (op in ('P', 'U'))
            heavy.append(extra + model.width * len(children))
            if any(c in model.leaves for c in children):
                consumers.append(extra)
                unused.difference_update(children)
    assert not unused and consumers
    # Until the first consumer runs, every fresh leaf in a query batch is live.
    # That consumer needs at least min(consumers) additional qubits.
    batch = (available - min(consumers)) // model.width
    assert batch > 0
    return dict(forward_calls=forward, inverse_calls=inverse,
                max_forward_batch=batch,
                forward=(forward+batch-1)//batch,
                symmetric=max((forward+batch-1)//batch,
                              (query_volume+available-1)//available),
                kernel=len(heavy) if 2*min(heavy)>available else 0,
                consumer_extra_qubits=min(consumers))


def check_schedule(model, trace, times, cap, metric):
    """Replay batches independently of the solver, including all live scratch."""
    assert len(times) == len(trace) and min(times) >= 0
    groups = defaultdict(list)
    for i, time in enumerate(times):
        groups[time].append(i)
    for before, after in dependencies(model, trace):
        assert times[after] >= times[before] + (trace[before][0] != 'G')
    peak, peak_live, max_queries = model.fixed, 0, 0
    query_times = set()
    ordered = []
    batches = []
    for seed in range(32):
        state = dict.fromkeys(model.children, 0)
        rng = random.Random(seed)
        values = {v: rng.getrandbits(64) for v in state}
        ghosts, phase = {}, 0
        for time, indices in sorted(groups.items()):
            # Measurements occur at the boundary after preceding kernels finish.
            zero = [i for i in indices if trace[i][0] == 'G']
            ops = [i for i in indices if trace[i][0] != 'G']
            for i in zero:
                _, v = trace[i]
                assert state[v] == 1 and v not in model.leaves and v != model.root
                state[v] = 2
                ghosts[v] = rng.getrandbits(64)
                phase ^= (ghosts[v] & values[v]).bit_count() & 1
            touched = set()
            live = sum(s == 1 for v, s in state.items() if v != model.root)
            allocated, scratch, queries = 0, 0, 0
            for i in ops:
                op, v = trace[i]
                registers = set(active(model, op, v))
                assert not touched & registers, (time, op, v, 'shared register')
                touched |= registers
                assert all(state[c] == 1 for c in model.children[v])
                assert state[v] in ({0, 2} if op == 'P' else
                                    {0} if op == 'R' else {1} if op == 'U' else {2})
                allocated += op == 'P'
                scratch += model.cost(op, v).scratch
                queries += counted(model, op, v, metric)
            occupied = model.fixed + (live + allocated) * model.width + scratch
            assert occupied <= cap, (time, occupied, cap)
            if seed == 0:
                peak = max(peak, occupied)
                peak_live = max(peak_live, live + allocated)
                max_queries = max(max_queries, queries)
                if queries:
                    query_times.add(time)
                ordered.extend(trace[i] for i in zero + ops)
                batches.append(dict(time=time, measurements=zero, operations=ops,
                                    qubits=occupied, counted_calls=queries))
            for i in ops:
                op, v = trace[i]
                if op in ('P', 'C') and state[v] == 2:
                    phase ^= (ghosts.pop(v) & values[v]).bit_count() & 1
                state[v] = 1 if op in ('P', 'R') else 0
        assert state[model.root] == 1
        assert all(s == 0 for v, s in state.items() if v != model.root)
        assert not ghosts and phase == 0
    serial = model.check(ordered)
    original = model.check(trace)
    assert Counter(map(tuple, ordered)) == Counter(map(tuple, trace))
    assert serial['work'] == original['work'] and serial['counts'] == original['counts']
    return dict(depth=len(query_times), peak_qubits=peak, peak_live=peak_live,
                maximum_batch=max_queries, work=serial['work'], counts=serial['counts'],
                parity_trials=32, batches=batches, trace=ordered)


def remove_idle_layers(model, trace, times, metric):
    groups = defaultdict(list)
    for i, time in enumerate(times):
        groups[time].append(i)
    stride = 1 if metric == 'kernel' else len(trace)+1
    layer, offset = -1, 0
    out = [0]*len(trace)
    for indices in (groups[t] for t in sorted(groups)):
        if any(counted(model, *trace[i], metric) for i in indices):
            layer += 1
            time, offset = layer*stride, 1
        elif metric == 'kernel':
            time = layer+1  # measurements at the end of the preceding round
        else:
            assert layer >= 0, 'CFS must start with a leaf lookup'
            time = layer*stride+offset
            offset += 1
        for i in indices:
            out[i] = time
    return out


def solve(model, trace, cap, metric, seconds, workers, hint=None):
    n = len(trace)
    queries = [i for i, (op, v) in enumerate(trace) if counted(model, op, v, metric)]
    # Free arithmetic is ordered in the gaps between oracle layers. n+1 slots
    # suffice even when every non-query operation in a gap is serialized.
    stride = 1 if metric == 'kernel' else n + 1
    horizon = (len(queries) + 1) * stride
    cp = cp_model.CpModel()
    bounds = resource_bounds(model, trace, cap)
    starts = [cp.new_int_var(0, horizon - 1, f't{i}') for i in range(n)]
    duration = [int(op != 'G') for op, _ in trace]
    intervals, demands, placed = [], [], {}
    edges = dependencies(model, trace)
    for before, after in edges:
        cp.add(starts[after] >= starts[before] + duration[before])
    for i, (op, v) in enumerate(trace):
        scratch = model.cost(op, v).scratch
        if scratch:
            intervals.append(cp.new_fixed_size_interval_var(starts[i], 1, f'kernel{i}'))
            demands.append(scratch)
        if op == 'P':
            placed[v] = i
        elif op in ('G', 'U'):
            birth = placed.pop(v)
            end = starts[i] + duration[i]
            length = cp.new_int_var(1, horizon, f'life{i}')
            cp.add(length == end - starts[birth])
            intervals.append(cp.new_interval_var(starts[birth], length, end, f'value{i}'))
            demands.append(model.width)
    assert not placed
    cp.add_cumulative(intervals, demands, cap - model.fixed)
    layers = [cp.new_int_var(0, len(queries)-1, f'q{i}') for i in queries]
    for i, q in zip(queries, layers):
        cp.add(starts[i] == q * stride)
    depth = cp.new_int_var(1, len(queries), 'depth')
    cp.add(depth >= bounds[metric])
    cp.add_max_equality(depth, [q + 1 for q in layers])
    for i in range(n):
        cp.add(starts[i] + duration[i] <= depth * stride)
    # Propagate query precedence directly, across intervening free kernels.
    query_map = dict(zip(queries, layers))
    predecessors = defaultdict(list)
    for before, after in edges:
        predecessors[after].append(before)
    previous_queries = []
    for i in range(n):
        prior = set().union(*(previous_queries[j] for j in predecessors[i]))
        if i in query_map:
            for j in prior:
                cp.add(query_map[i] >= query_map[j] + 1)
            prior = {i}
        previous_queries.append(prior)
    if metric != 'kernel':
        # These redundant layer constraints make the resource lower bounds
        # visible without reasoning through the gaps between oracle calls.
        cp.add_cumulative(
            [cp.new_fixed_size_interval_var(q, 1, f'query{i}') for i, q in zip(queries, layers)],
            [model.width + model.cost(*trace[i]).scratch for i in queries], cap-model.fixed)
        # Every newly loaded CFS leaf must be used by a node before erasure.
        # Before the first such use all leaves from this batch are still live.
        # The consumer needs its scratch, plus a target register for P/U.
        capacity = bounds['max_forward_batch']
        forward = [q for i, q in zip(queries, layers) if trace[i][0] == 'P']
        cp.add_cumulative([cp.new_fixed_size_interval_var(q, 1, f'forward{j}')
                           for j, q in enumerate(forward)], [1]*len(forward), capacity)
        cp.add(depth >= (len(forward)+capacity-1)//capacity)
    cp.minimize(depth)
    # A feasible serial hint, using exactly the original trace.
    now, query_layer = 0, {}
    hint_times = remove_idle_layers(model, trace, hint['starts'], metric) if hint else None
    if hint:
        assert list(map(tuple, hint['source_trace'])) == list(map(tuple, trace))
        cp.add(depth <= max(hint_times[i]//stride for i in queries)+1)
    for i in range(n):
        if i in queries:
            now = (now+stride-1)//stride*stride
            query_layer[i] = now//stride
        if hint_times is not None:
            now = hint_times[i]
            if i in queries:
                query_layer[i] = now//stride
        cp.add_hint(starts[i], now)
        now += duration[i]
    for i, q in zip(queries, layers):
        cp.add_hint(q, query_layer[i])
    cp.add_hint(depth, max(query_layer.values())+1)
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = seconds
    solver.parameters.num_workers = workers
    solver.parameters.random_seed = 0
    status = solver.solve(cp)
    if status not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        raise RuntimeError(f'No schedule returned: {solver.status_name(status)}')
    times = remove_idle_layers(model, trace, [solver.value(t) for t in starts], metric)
    checked = check_schedule(model, trace, times, cap, metric)
    assert checked['depth'] <= round(solver.objective_value)
    checked.update(status=solver.status_name(status), lower_bound=max(solver.best_objective_bound, bounds[metric]),
                   resource_bounds=bounds,
                   seconds=solver.wall_time, starts=times, stride=stride,
                   source_trace=trace, source_trace_sha256=hashlib.sha256(
                       json.dumps(trace, separators=(',', ':')).encode()).hexdigest())
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cases', nargs='*', default=CASES)
    parser.add_argument('--input', type=Path, default=HERE/'results')
    parser.add_argument('--output', type=Path, default=HERE/'results'/'depth')
    parser.add_argument('--metrics', nargs='+', choices=['forward', 'symmetric', 'kernel'],
                        default=['forward', 'symmetric', 'kernel'])
    parser.add_argument('--schedules', nargs='+', choices=['cfs', 'block_cleanup'],
                        default=['cfs', 'block_cleanup'])
    parser.add_argument('--seconds', type=float, default=120)
    parser.add_argument('--workers', type=int, default=4)
    parser.add_argument('--verify', type=Path, nargs='+', help='replay saved schedules without searching')
    args = parser.parse_args()
    if any(case not in CASES for case in args.cases):
        parser.error('cases must be p224, p256, or p256-second-prime')
    if not __debug__:
        parser.error('run without -O so verification is enabled')
    if args.seconds <= 0 or args.workers < 1:
        parser.error('seconds and workers must be positive')
    if args.verify:
        for path in args.verify:
            saved = json.loads(path.read_text())
            data, model, traces, _ = read_case(args.input/f'{saved["case"]}_benchmark.json')
            assert saved['model'] == data['model'] and saved['cap'] == data['cap']
            for metric, rows in saved['metrics'].items():
                for name, row in rows.items():
                    assert list(map(tuple, row['source_trace'])) == list(map(tuple, traces[name]))
                    check = check_schedule(model, traces[name], row['starts'], data['cap'], metric)
                    for key in ('depth', 'peak_qubits', 'work', 'counts'):
                        assert check[key] == row[key], (path, metric, name, key)
                    assert resource_bounds(model, traces[name], data['cap'])[metric] <= row['depth']
            print(f'{path}: all schedules replayed; current C++ traces match', flush=True)
        return
    args.output.mkdir(parents=True, exist_ok=True)
    for case in args.cases:
        data, model, traces, decisions = read_case(args.input/f'{case}_benchmark.json')
        path = args.output/f'{case}.json'
        previous = json.loads(path.read_text()) if path.exists() else {}
        if previous.get('model') != data['model'] or previous.get('cap') != data['cap']:
            previous = {}
        result = dict(case=case, prime=data['prime'], cap=data['cap'],
                      fixed_qubits=model.fixed, width=model.width, decisions=decisions,
                      ortools=ortools.__version__, model=data['model'], metrics=previous.get('metrics', {}))
        for metric in args.metrics:
            result['metrics'].setdefault(metric, {})
            for name in args.schedules:
                trace = traces[name]
                old = result['metrics'][metric].get(name)
                if old and list(map(tuple, old['source_trace'])) != list(map(tuple, trace)):
                    old = None
                row = solve(model, trace, data['cap'], metric, args.seconds, args.workers, old)
                if old:
                    bound = max(row['lower_bound'], old['lower_bound'])
                    if old['depth'] < row['depth']:
                        row = old
                    row['lower_bound'] = bound
                if row['depth'] == row['lower_bound']:
                    row['status'] = 'OPTIMAL'
                assert row['lower_bound'] <= row['depth']
                row['resource_bounds'] = resource_bounds(model, trace, data['cap'])
                result['metrics'][metric][name] = row
                print(f'{case} {metric} {name}: depth={row["depth"]}, '
                      f'bound={row["lower_bound"]:g}, peak={row["peak_qubits"]}, '
                      f'{row["status"]}', flush=True)
                path.write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()
