"""Choose boundaries using circuit costs; execute them with the root C++ scheduler."""
from functools import lru_cache


class CutPlanner:
    def __init__(self, model, cap, scheduler, max_frontier=None):
        self.m, self.cap, self.scheduler = model, cap, scheduler
        self.max_frontier = len(model.leaves) if max_frontier is None else max_frontier
        self.choice = {}

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

    @lru_cache(None)
    def fast_peak(self, v):
        initial = {x: 2 for x in self.m.descendants(v) if x not in self.m.leaves}
        trace = self.scheduler.sequential_pebble(v)
        return self.m.check(trace, initial=initial, final=False)['peak_qubits']

    def order(self, frontier):
        return sorted(frontier, key=lambda x: (-self.fast_peak(x), x))

    def block(self, v, frontier, root=False):
        return self.scheduler.contract(v, self.order(frontier), root)

    def evaluate(self, v, frontier, root=False):
        for f in frontier:
            if f not in self.m.leaves:
                self.choose_cleanup(f)
        boundaries = {x: self.order(cut) for x, cut in self.choice.items()}
        boundaries[v] = self.order(frontier)
        return self.scheduler.fast_clean(v, boundaries, root)

    @lru_cache(None)
    def choose_cleanup(self, v):
        initial = {x: 2 for x in self.m.descendants(v) if x not in self.m.leaves}
        candidates = []
        for cut in self.cuts(v):
            try:
                trace = self.evaluate(v, cut)
            except ValueError:
                continue
            row = self.m.check(trace, initial=initial, final=False)
            if all(row['state'][x]==0 for x in initial) and row['peak_qubits']<=self.cap:
                candidates.append((row['work'], row['peak_qubits'], cut))
        if not candidates:
            raise ValueError(('no cut cleanup', v, self.cap))
        self.choice[v] = min(candidates)[2]

    def run(self):
        candidates = []
        for cut in self.cuts(self.m.root):
            try:
                trace = self.evaluate(self.m.root, cut, True)
            except ValueError:
                continue
            row = self.m.check(trace)
            if row['peak_qubits']<=self.cap:
                candidates.append((row['work'], row['peak_qubits'], cut, trace))
        if not candidates:
            raise ValueError(('no root cut', self.cap))
        best = min(candidates, key=lambda x: (x[0], x[1], x[2]))
        return best[3], dict(root_cut=best[2], cleanup_cuts=dict(self.choice))

    def audit(self):
        groups = {name: dict(candidate_cuts=0, feasible_by_size={})
                  for name in ('root', 'cleanup')}
        for vertex in self.m.children:
            if not self.m.children[vertex]:
                continue
            root = vertex==self.m.root
            group = groups['root' if root else 'cleanup']
            initial = {} if root else {
                x: 2 for x in self.m.descendants(vertex) if x not in self.m.leaves}
            for cut in self.cuts(vertex):
                group['candidate_cuts'] += 1
                row = self.m.check(self.block(vertex, cut, root), initial=initial, final=False)
                if row['peak_qubits']<=self.cap:
                    counts = group['feasible_by_size']
                    counts[len(cut)] = counts.get(len(cut), 0)+1
        return dict(all_cut_sizes_enumerated=self.max_frontier>=len(self.m.leaves), **groups)
