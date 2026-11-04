#!/usr/bin/env python3
"""CFS arithmetic trees in the manuscript's persistent-root pebbling model.

Every value is one projective-coordinate triple. P and U cost one each;
G costs zero work. This experiment does not construct arithmetic circuits.
The CFS library supplies the actual tree and published KSS ordering. Its
direct ghost correction is expanded into P,U, and its terminal operation
is represented by producing the tree's root triple. The common residue
extraction from this triple is outside the measured tree computation.
"""
from __future__ import annotations

from functools import lru_cache
import hashlib
import json
import os
from pathlib import Path
import random
import shlex
import subprocess
import sys

HERE=Path(__file__).resolve().parent
REPO=HERE.parents[1]
APP=HERE
SOURCE=Path(os.environ.get('CFS_SOURCE', REPO/'build'/'external'/'compressed-ec-dlog')).resolve()
OUT=REPO/'build'/'applications'/'cfs'/'results'
BUILD=REPO/'build'/'applications'/'cfs'
if not (SOURCE/'rns_addition_tree_run.py').is_file():
    raise SystemExit(f'CFS source not found at {SOURCE}; see applications/cfs/README.md')
revision=subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()
if revision!='7fedcc5b102d5d89f3a0229fd6ca56bfb41b4bda':
    raise SystemExit(f'Unexpected CFS revision {revision}; see applications/cfs/README.md')
dirty=subprocess.check_output(['git','-C',str(SOURCE),'status','--porcelain','--untracked-files=no'],text=True)
if dirty:
    raise SystemExit(f'CFS source has tracked changes: {SOURCE}')
sys.path[:0]=[str(APP),str(SOURCE)]
from qarton.algorithms import PebblingBinaryTree
from qarton.circuit import BoolType, InPlaceCircuit, UInt, UIntType
from ecdlp.curve_params import curve_parameters, nb_leaves, dl_precomputed_multiples
from ecdlp.elliptic_curve import ProjPoint
from ecdlp.util import addition_formulas
from rns_addition_tree_run import tree_full_computation
from sympy import isprime
import shared_scheduler


class RecordedPublicTree(PebblingBinaryTree):
    def __init__(self,*args,**kwargs):
        self.trace=[]
        super().__init__(*args,**kwargs)

    def _nothing_to_pebble(self,v):
        self.trace.append(('R' if v==self._root_node else 'P',v))
        super()._nothing_to_pebble(v)

    def _ghost_to_pebble(self,v):
        self.trace.append(('P',v))
        super()._ghost_to_pebble(v)

    def _pebble_to_nothing(self,v):
        self.trace.append(('U',v))
        super()._pebble_to_nothing(v)

    def _pebble_to_ghost(self,v):
        self.trace.append(('G',v))
        super()._pebble_to_ghost(v)

    def _ghost_to_nothing(self,v):
        self.trace.append(('C',v))
        super()._ghost_to_nothing(v)


def public_tree(leaves):
    leaf=InPlaceCircuit()
    a=leaf.add_input_output(BoolType())
    b=leaf.add_input_output(BoolType())
    leaf.cx_reg(a,b)
    leaf.freeze(f'cfs_tree_leaf_{leaves}')
    node=InPlaceCircuit()
    a,b,c=[node.add_input_output(BoolType()) for _ in range(3)]
    node.cx_reg(a,c)
    node.cx_reg(b,c)
    node.freeze(f'cfs_tree_node_{leaves}')
    # Dummy circuits are used only to obtain the author's graph and ordering.
    # Resources and values are checked independently below.
    tree=RecordedPublicTree(leaf_circuits=(leaf,)*leaves,node_circuit=node,root_circuit=node)
    return tree


class UnitModel:
    def __init__(self,children,leaves,root):
        self.children={v:list(cs) for v,cs in children.items()}
        self.leaves=set(leaves)
        self.root=root

    @lru_cache(None)
    def descendants(self,v):
        return (v,)+tuple(x for c in self.children[v] for x in self.descendants(c))

    def check(self,trace,initial=None,final=True):
        state=dict.fromkeys(self.children,0)
        state.update(initial or {})
        live=sum(x==1 for x in state.values())
        peak=live
        work=0
        counts=dict(P=0,U=0,G=0)
        for op,v in trace:
            assert op in counts and v in state
            if op in ('P','U'):
                assert all(state[c]==1 for c in self.children[v]),(op,v,'missing inputs')
            if op=='P':
                assert state[v] in (0,2)
                live+=1
                peak=max(peak,live)
                state[v]=1
                work+=1
            elif op=='U':
                assert state[v]==1
                live-=1
                state[v]=0
                work+=1
            else:
                assert state[v]==1
                live-=1
                state[v]=2
            counts[op]+=1
        if final:
            assert state[self.root]==1 and live==1
            assert all(state[v]==0 for v in state if v!=self.root)
        return dict(work=work,space=peak,depth=len(trace),counts=counts,state=state)


class BoundaryPlanner:
    """Select boundaries within the unchanged recursive block procedure."""
    def __init__(self,model,cap,scheduler):
        self.m,self.cap,self.scheduler=model,cap,scheduler
        self.boundaries={}
        self.max_boundary=(cap+1)//2

    @lru_cache(None)
    def options(self,v):
        out={(v,)}
        if self.m.children[v]:
            l,r=self.m.children[v]
            for a in self.options(l):
                for b in self.options(r):
                    if len(a)+len(b)<=self.max_boundary:out.add(a+b)
        return tuple(sorted(out,key=lambda a:(len(a),a)))

    def cuts(self,v):
        return tuple(c for c in self.options(v) if len(c)>1)

    @lru_cache(None)
    def fast_space(self,v):
        return self.m.check(self.scheduler.sequential_pebble(v),final=False)['space']

    def order(self,cut):
        return sorted(cut,key=lambda v:(-self.fast_space(v),v))

    @lru_cache(None)
    def cleanup(self,v):
        if v in self.m.leaves:return ()
        initial={x:2 for x in self.m.descendants(v) if x not in self.m.leaves}
        candidates=[]
        for cut in self.cuts(v):
            block=self.scheduler.contract(v,self.order(cut),False)
            try:
                trace=block+[(op,x) for c in self.order(cut) for op,x in self.cleanup(c)]
            except ValueError:
                continue
            row=self.m.check(trace,initial=initial,final=False)
            assert all(row['state'][x]==0 for x in self.m.descendants(v))
            # The final tree root remains stored throughout every cleanup.
            if row['space']<=self.cap-1:
                candidates.append((row['work'],row['space'],cut,tuple(trace)))
        if not candidates:raise ValueError(('no feasible cleanup',v,self.cap))
        best=min(candidates,key=lambda a:a[:3])
        self.boundaries[v]=self.order(best[2])
        return best[3]

    def run(self):
        candidates=[]
        for cut in self.cuts(self.m.root):
            try:
                tail=[(op,v) for c in self.order(cut) for op,v in self.cleanup(c)]
            except ValueError:
                continue
            trace=self.scheduler.contract(self.m.root,self.order(cut),True)+tail
            row=self.m.check(trace)
            if row['space']<=self.cap:
                candidates.append((row['work'],row['space'],cut,trace))
        if not candidates:raise ValueError(('no feasible root',self.cap))
        best=min(candidates,key=lambda a:a[:3])
        self.boundaries[self.m.root]=self.order(best[2])
        native=self.scheduler.fast_clean(self.m.root,self.boundaries,True)
        assert native==best[3],'DP composition differs from the manuscript core'
        return native,dict(root_boundary=list(best[2]),cleanup_boundaries=self.boundaries)


def normalize(trace):
    out=[]
    for op,v in trace:
        out.extend([('P',v),('U',v)] if op=='C' else [('P' if op=='R' else op,v)])
    return out


def native_check(model,trace,cap,expected):
    order=[]
    def visit(v):
        order.append(v)
        for c in model.children[v]:visit(c)
    visit(model.root)
    ids={v:i+1 for i,v in enumerate(order)}
    lines=[f'{len(order)} {cap}']
    for v in order:
        lines.append(' '.join(str(ids[c]) for c in model.children[v]) if model.children[v] else '0 0')
    lines += [f'{op} {ids[v]}' for op,v in trace]
    output=subprocess.check_output([str(BUILD/'check_trace')],input='\n'.join(lines)+'\n',text=True)
    assert tuple(map(int,output.split()))==tuple(expected[k] for k in ('work','space','depth'))


def leaf_order(model,v):
    return [v] if v in model.leaves else [x for c in model.children[v] for x in leaf_order(model,c)]


def selected_windows(curve,indices):
    c=curve_parameters(curve)
    powers=dl_precomputed_multiples(curve)
    assert len(indices)==nb_leaves(curve)
    selected=[]
    for i,index in enumerate(indices):
        group=powers[i*c.window:(i+1)*c.window]
        assert 0<=index<1<<len(group)
        point=ProjPoint(0,1,0,c.ec)
        for take,coords in zip(UIntType(len(group)).to_register(UInt(index)),group):
            if take:point+=ProjPoint(*coords,c.ec)
        selected.append(tuple(int(x) for x in point.to_coordinates()))
    return selected


def replay_values(model,trace,curve,prime,indices,seed):
    c=curve_parameters(curve)
    windows=selected_windows(curve,indices)
    leaves={v:tuple(x%prime for x in value) for v,value in zip(leaf_order(model,model.root),windows)}
    a,b3=c.ec.a%prime,(3*c.ec.b)%prime
    def value(v,live):
        if v in leaves:return leaves[v]
        l,r=model.children[v]
        return tuple(int(x) for x in addition_formulas(*live[l],*live[r],a,b3,prime))
    width=prime.bit_length()
    def encode(t):return sum(x<<(i*width) for i,x in enumerate(t))
    live,ghosts={},{}
    rng=random.Random(seed)
    phase=0
    for op,v in trace:
        if op=='P':
            assert v not in live
            live[v]=value(v,live)
            if v in ghosts:phase^=(ghosts.pop(v)&encode(live[v])).bit_count()&1
        elif op=='U':
            assert live[v]==value(v,live)
            del live[v]
        else:
            mask=rng.getrandbits(3*width)
            phase^=(mask&encode(live.pop(v))).bit_count()&1
            assert v not in ghosts
            ghosts[v]=mask
    assert set(live)=={model.root} and not ghosts and phase==0
    root=live[model.root]
    sparse_windows=[{index:point} for index,point in zip(indices,windows)]
    reference=tree_full_computation(curve,indices,prime,windows=sparse_windows)
    assert root[0]*root[2]%prime==reference
    return dict(indices=indices,root=list(root),residue=reference,seed=seed)


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    # The adapter changes only translations of root output and clear().
    # contract() and fast_clean() are the original shared C++ templates.
    source=(APP/'schedule.cpp').read_text()
    assert source.count("trace.push_back({x==root ? 'R' : 'P', x});")==1
    assert source.count("trace.push_back({'C', x});")==1
    source=source.replace('#include "../sequential_schedule.hpp"','#include "sequential_schedule.hpp"')
    source=source.replace("trace.push_back({x==root ? 'R' : 'P', x});","trace.push_back({'P', x});")
    source=source.replace("trace.push_back({'C', x});","pebble(x); unpebble(x);")
    (BUILD/'unit_schedule.cpp').write_text(source)
    binary=BUILD/'unit_schedule'
    compiler=shlex.split(os.environ.get('CXX', 'c++'))
    subprocess.run(compiler+['-std=c++17','-O2','-UNDEBUG','-I',str(REPO),str(BUILD/'unit_schedule.cpp'),'-o',str(binary)],check=True)
    subprocess.run(compiler+['-std=c++17','-O2','-UNDEBUG','-I',str(REPO),str(HERE/'check_trace.cpp'),'-o',str(BUILD/'check_trace')],check=True)
    result=dict(model=dict(work='P+U',space='value registers including leaves and the retained root',depth='every nonempty sequential action is a round',endpoint='root coordinate triple retained, all other values and ghosts removed'),
                root_scope='Coherent coordinate-tree evaluation; common residue extraction/accumulator update is outside this comparison.',
                baseline='Public CFS KSS ordering, with direct corrections expanded to P,U and root represented by a retained value.',
                algorithm='Original sequential_schedule.hpp block contract and recursive cleanup; boundaries minimize unit work within that procedure, not over arbitrary schedules.',
                cases=[])
    expected=json.loads((HERE/'expected.json').read_text())
    specifications=[(row['curve'],row['prime']) for row in expected]
    for curve,prime in specifications:
        assert isprime(prime)
        tree=public_tree(nb_leaves(curve))
        model=UnitModel(tree._label_to_children,set(tree._leaf_input_register),tree._root_node)
        baseline=normalize(tree.trace)
        b=model.check(baseline)
        old_path=OUT/'reference_traces'/f'{curve}_fresh.json'
        if old_path.exists():
            old=json.loads(old_path.read_text())
            assert model.children==old['model']['children']
            assert baseline==normalize(old['rows'][old['baseline']]['trace'])
        with shared_scheduler.SharedScheduler(model, binary) as scheduler:
            planner=BoundaryPlanner(model,b['space'],scheduler)
            trace,choices=planner.run()
        ours=model.check(trace)
        reference=next(item for item in expected if item['curve']==curve)
        observed={'curve':curve, 'prime':prime, 'leaves':len(model.leaves), 'space':b['space'], 'kss_work':b['work'], 'ours_work':ours['work']}
        if observed != reference: raise ValueError(f'{curve}: {observed} != {reference}')
        assert ours['space']<=b['space']
        native_check(model,baseline,b['space'],b)
        native_check(model,trace,b['space'],ours)
        row=dict(curve=curve,prime=prime,leaves=len(model.leaves),space_budget=b['space'],
                 baseline={k:v for k,v in b.items() if k!='state'},ours={k:v for k,v in ours.items() if k!='state'},
                 reduction_percent=100*(1-ours['work']/b['work']),tree=model.children,root=model.root,
                 baseline_trace=baseline,ours_trace=trace,boundaries=choices,validation=[])
        c=curve_parameters(curve)
        sizes=[len(dl_precomputed_multiples(curve)[i*c.window:(i+1)*c.window]) for i in range(len(model.leaves))]
        rng=random.Random(22112)
        cases=[r['indices'] for r in old['basis_checks'][old['baseline']]] if old_path.exists() else [[rng.randrange(1<<n) for n in sizes] for _ in range(4)]
        for i,indices in enumerate(cases):
            for seed in range(8):
                x=replay_values(model,baseline,curve,prime,indices,seed)
                y=replay_values(model,trace,curve,prime,indices,seed)
                assert x==y
                if old_path.exists():assert x['residue']==old['basis_checks'][old['baseline']][i]['residue']
                row['validation'].append(x)
        result['cases'].append(row)
        (OUT/'cfs.json').write_text(json.dumps(result,indent=2)+'\n')
        print(curve,'leaves',len(model.leaves),'space',b['space'],'work',b['work'],'->',ours['work'],'reduction',row['reduction_percent'],flush=True)
    files=[HERE/'run.py',HERE/'check_trace.cpp',REPO/'sequential_schedule.hpp',REPO/'quantum_computer.hpp',REPO/'pebbling_common.hpp',APP/'schedule.cpp',APP/'shared_scheduler.py',SOURCE/'ecdlp/curve_params.py',SOURCE/'ecdlp/util.py',SOURCE/'rns_addition_tree_run.py']
    result['source_sha256']={str(p.relative_to(REPO)) if p.is_relative_to(REPO) else 'cfs/'+str(p.relative_to(SOURCE)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    result['cfs_revision']=subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()
    result['validation_summary']=dict(arithmetic_inputs_per_curve=4,measurement_masks_per_input=8,paired_schedule_replays=4*8*len(specifications),native_state_machine_replays=2*len(specifications),checks=['all action preconditions','old and new value-register occupancy','final root and complete cleanup','actual CFS arithmetic values','root residue agrees with author reference','phase cancellation','C++ recursive core equals chosen block composition'])
    (OUT/'cfs.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
