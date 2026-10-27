"""Replay the fixed feasible CFS reference batches without a scheduling solver."""
from collections import defaultdict
import hashlib
import json
from pathlib import Path
from circuit_model import Cost, Model


def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def check_reference(case, data):
    saved = json.loads(Path(__file__).with_name('cfs_reference_batches.json').read_text())['instances'][case]
    assert saved['model_sha256'] == digest(data['model']) and saved['cap'] == data['cap']
    spec = data['model']
    model = Model(spec['children'],set(spec['leaves']),spec['root'],
                  data['node_width'],data['fixed_qubits'],
                  {(c['op'],c['vertex']):Cost(c['work'],c['scratch']) for c in spec['costs']})
    trace = data['rows'][data['baseline']]['trace']
    baseline = model.check(trace)
    result = {}
    for metric, row in saved['schedules'].items():
        assert row['source_trace_sha256'] == digest(trace)
        assert len(row['starts']) == len(trace)
        groups = defaultdict(list)
        last = {}
        for i,(op,v) in enumerate(trace):
            for x in [v]+(model.children[v] if op != 'G' else []):
                if x in last:
                    prev = last[x]
                    assert row['starts'][i] >= row['starts'][prev]+(trace[prev][0] != 'G')
                last[x] = i
            groups[row['starts'][i]].append((op,v))
        rounds = []
        for _, actions in sorted(groups.items()):
            measured = [v for op,v in actions if op == 'G']
            if measured:
                assert rounds
                rounds[-1]['measure'].extend(measured)
            ops = [(op,v) for op,v in actions if op != 'G']
            if ops: rounds.append(dict(ops=ops,measure=[]))
        checked = model.check_rounds(rounds,data['cap'])
        field = {'forward':'forward_lookup_depth','symmetric':'symmetric_lookup_depth','kernel':'kernel_depth'}[metric]
        assert checked[field] == row['depth']
        assert checked['work'] == baseline['work'] and checked['counts'] == baseline['counts']
        result[metric] = dict(depth=checked[field],peak_qubits=checked['peak_qubits'],status='FEASIBLE')
    return result
