"""Mixed-depth frontier planner for the exported CFS tree model.

Enumerates every dependency cut of bounded size, rather than only a uniform
depth. This strictly contains the earlier block family on incomplete trees.
"""
from functools import lru_cache
from itertools import permutations

class CutPlanner:
    def __init__(self,model,cap,max_frontier=8):
        self.m,self.cap,self.max_frontier=model,cap,max_frontier
        self.choice={}

    @lru_cache(None)
    def height(self,v):
        return 0 if not self.m.children[v] else 1+max(self.height(c) for c in self.m.children[v])

    @lru_cache(None)
    def options(self,v):
        """All cuts of subtree v, including the stop-at-v cut."""
        out={(v,)}
        if self.m.children[v]:
            left,right=self.m.children[v]
            for a in self.options(left):
                for b in self.options(right):
                    if len(a)+len(b)<=self.max_frontier:
                        out.add(tuple(sorted(a+b)))
        return tuple(sorted(out,key=lambda x:(len(x),x)))

    def cuts(self,v):
        if not self.m.children[v]:return ()
        left,right=self.m.children[v]
        return tuple(a+b for a in self.options(left) for b in self.options(right)
                     if len(a)+len(b)<=self.max_frontier)

    def inner(self,v,frontier):
        stop=set(frontier);out=[]
        def visit(x):
            if x in stop:return
            for c in self.m.children[x]:visit(c)
            out.append(x)
        visit(v);return out

    def fast(self,v):
        t=[]
        for c in self.m.children[v]:t+=self.fast(c)
        t.append(('P',v))
        for c in self.m.children[v]:t.append(('U' if c in self.m.leaves else 'G',c))
        return t

    @lru_cache(None)
    def fast_peak(self,v):
        initial={x:2 for x in self.m.descendants(v) if x not in self.m.leaves}
        return self.m.check(self.fast(v),initial=initial,final=False)['peak_qubits']

    def block(self,v,frontier,root=False):
        # Independent subtree tasks each leave one equally wide output value.
        # Scheduling larger standalone peaks first minimizes max_i(peak_i+i*w).
        order=sorted(frontier,key=lambda x:(-self.fast_peak(x),x))
        inside=self.inner(v,frontier);t=[]
        for f in order:t+=self.fast(f)
        for x in inside:
            if x!=v:t.append(('P',x))
        t.append(('R' if root else 'C',v))
        for x in reversed(inside):
            if x!=v:t.append(('U',x))
        for f in order:t.append(('U' if f in self.m.leaves else 'G',f))
        return t

    @lru_cache(None)
    def cleanup(self,v):
        initial={x:2 for x in self.m.descendants(v) if x not in self.m.leaves}
        candidates=[]
        for cut in self.cuts(v):
            t=self.block(v,cut)
            try:
                for f in cut:
                    if f not in self.m.leaves:t+=self.cleanup(f)
            except ValueError:continue
            r=self.m.check(t,initial=initial,final=False)
            if all(r['state'][x]==0 for x in initial) and r['peak_qubits']<=self.cap:
                candidates.append((r['work'],r['peak_qubits'],cut,t))
        if not candidates:raise ValueError(('no cut cleanup',v,self.cap))
        best=min(candidates,key=lambda x:(x[0],x[1],x[2]));self.choice[v]=best[2]
        return tuple(best[3])

    def run(self):
        candidates=[]
        for cut in self.cuts(self.m.root):
            t=self.block(self.m.root,cut,True)
            try:
                for f in cut:
                    if f not in self.m.leaves:t+=self.cleanup(f)
            except ValueError:continue
            r=self.m.check(t)
            if r['peak_qubits']<=self.cap:candidates.append((r['work'],r['peak_qubits'],cut,t))
        if not candidates:raise ValueError(('no root cut',self.cap))
        best=min(candidates,key=lambda x:(x[0],x[1],x[2]))
        return list(best[3]),dict(root_cut=best[2],cleanup_cuts=dict(self.choice))
