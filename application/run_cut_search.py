"""Enumerate mixed-depth cuts and audit the full candidate domain."""
import argparse,json
from pathlib import Path
from cut_planner import CutPlanner
from scheduler import Model,Cost

def load(data):
    s=data['model'];cost={(r['op'],r['vertex']):Cost(r['work'],r['scratch']) for r in s['costs']}
    return Model(s['children'],set(s['leaves']),s['root'],data['node_width'],data['fixed_qubits'],cost)


def audit_cuts(planner):
    """Prefix feasibility is necessary for every recursive block schedule."""
    model = planner.m
    groups = {name: dict(candidate_cuts=0, feasible_by_size={})
              for name in ('root', 'cleanup')}
    for vertex in model.children:
        if not model.children[vertex]:
            continue
        root = vertex == model.root
        group = groups['root' if root else 'cleanup']
        initial = {} if root else {
            x: 2 for x in model.descendants(vertex) if x not in model.leaves}
        for cut in planner.cuts(vertex):
            group['candidate_cuts'] += 1
            row = model.check(planner.block(vertex, cut, root),
                              initial=initial, final=False)
            if row['peak_qubits'] <= planner.cap:
                counts = group['feasible_by_size']
                counts[len(cut)] = counts.get(len(cut), 0) + 1
    return dict(all_cut_sizes_enumerated=planner.max_frontier >= len(model.leaves),
                **groups)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--max-frontier',type=int,default=None)
    a=p.parse_args();data=json.loads(a.input.read_text());m=load(data)
    limit = len(m.leaves) if a.max_frontier is None else a.max_frontier
    if limit < 2:
        p.error('--max-frontier must be at least 2')
    planner = CutPlanner(m,data['cap'],limit)
    trace,decisions=planner.run();r=m.check(trace);r.pop('state')
    result=dict(work=r['work'],peak_qubits=r['peak_qubits'],trace=trace,decisions=decisions,max_frontier=limit,
                cut_audit=audit_cuts(planner))
    a.output.write_text(json.dumps(result,indent=2)+'\n');print({k:v for k,v in result.items() if k not in ('trace','decisions')})
