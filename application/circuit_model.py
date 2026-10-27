"""Circuit costs and trace replay for the fixed CFS dependency tree."""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Cost:
    work: int
    scratch: int


class Model:
    def __init__(self, children, leaves, root, width, fixed, costs):
        self.children = children
        self.leaves = leaves
        self.root = root
        self.width = width
        self.fixed = fixed
        self.costs = costs

    def kind(self, v):
        return "root" if v == self.root else "leaf" if v in self.leaves else "node"

    def cost(self, op, v):
        return self.costs[(op, v if v in self.leaves else self.kind(v))]

    def descendants(self, v):
        out = [v]
        for c in self.children[v]:
            out.extend(self.descendants(c))
        return out

    def check(self, trace, initial=None, final=True):
        """Independent state replay including ephemeral quantum workspace."""
        state = {v: 0 for v in self.children}
        if initial:
            state.update(initial)
        work = 0
        live = sum(x == 1 for v, x in state.items() if v != self.root)
        peak = self.fixed + live * self.width
        peak_live = live
        counts = {}
        for i, (op, v) in enumerate(trace):
            before = state[v]
            supported = all(state[c] == 1 for c in self.children[v])
            if op in ("P", "R", "C", "U"):
                assert supported, (i, op, v, "missing dependency")
            if op == "P":
                assert before in (0, 2), (i, op, v, before)
                state[v] = 1
                live += v != self.root
            elif op == "R":
                assert v == self.root and before == 0
                state[v] = 1
            elif op == "G":
                assert before == 1 and v not in self.leaves and v != self.root
                state[v] = 2
            elif op == "U":
                assert before == 1 and v != self.root
                state[v] = 0
            elif op == "C":
                assert before == 2 and v != self.root
                state[v] = 0
            else:
                raise ValueError(op)
            # P allocates output before scratch; U/G still hold output on entry.
            cost = self.cost(op, v)
            peak = max(peak, self.fixed + live * self.width + cost.scratch)
            peak_live = max(peak_live, live)
            if op in ("U", "G"):
                live -= 1
            work += cost.work
            key = op + ":" + self.kind(v)
            counts[key] = counts.get(key, 0) + 1
        if final:
            assert state[self.root] == 1
            assert all(x == 0 for v, x in state.items() if v != self.root)
        return dict(work=work, peak_qubits=peak, peak_live=peak_live, counts=counts,
                    events=len(trace), state=state)

    def check_rounds(self, rounds, cap):
        """Check simultaneous supports and scratch, independently of scheduling."""
        state = dict.fromkeys(self.children, 0)
        trace = []
        peak = self.fixed
        forward = symmetric = 0
        for row in rounds:
            ops = row['ops']
            assert ops
            touched = set()
            live = sum(s == 1 for v,s in state.items() if v != self.root)
            for op,v in ops:
                assert op in ('P','U','C','R')
                support = {v,*self.children[v]}
                assert not touched & support
                touched.update(support)
                assert all(state[c] == 1 for c in self.children[v])
                allowed = (0,2) if op == 'P' else (1,) if op == 'U' else (2,) if op == 'C' else (0,)
                assert state[v] in allowed
            used = self.fixed + (live+sum(op == 'P' for op,v in ops))*self.width
            used += sum(self.cost(op,v).scratch for op,v in ops)
            assert used <= cap, (used,cap,ops)
            peak = max(peak,used)
            forward += any(op == 'P' and v in self.leaves for op,v in ops)
            symmetric += any(v in self.leaves for op,v in ops)
            for op,v in ops:
                state[v] = 1 if op in ('P','R') else 0
                trace.append((op,v))
            for v in row['measure']:
                assert state[v] == 1 and v not in self.leaves and v != self.root
                state[v] = 2
                trace.append(('G',v))
        serial = self.check(trace)
        assert serial['state'] == state
        return dict(kernel_depth=len(rounds), forward_lookup_depth=forward,
                    symmetric_lookup_depth=symmetric, peak_qubits=peak,
                    work=serial['work'], counts=serial['counts'], trace=trace)
