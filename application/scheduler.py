"""Finite block schedules for CFS's exact, possibly incomplete binary tree.

No algebraic reassociation or change of node function. Frontier depth is tuned
independently for initial root construction and each ghost-cleanup subtree.
This is optimization within an explicit block family, not global optimality.
"""
from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache

Event = tuple[str, str]


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


class Planner:
    def __init__(self, model, cap):
        self.m = model
        self.cap = cap
        self.decisions = {}

    @lru_cache(None)
    def height(self, v):
        return 0 if not self.m.children[v] else 1 + max(map(self.height, self.m.children[v]))

    def cut(self, v, depth):
        if depth == 0 or v in self.m.leaves:
            return [v], []
        frontier, inner = [], []
        for c in self.m.children[v]:
            f, n = self.cut(c, depth - 1)
            frontier += f
            inner += n
        return frontier, inner + [v]  # inner is child-before-parent

    def fast(self, v):
        t = []
        for c in self.m.children[v]:
            t += self.fast(c)
        t += [("P", v)]
        for c in self.m.children[v]:
            t += [("U" if c in self.m.leaves else "G", c)]
        return t

    def block(self, v, depth, root=False):
        frontier, inner = self.cut(v, depth)
        # Larger subtree first lowers the peak of depth-first frontier production.
        frontier.sort(key=lambda x: -self.height(x))
        t = []
        for f in frontier:
            t += self.fast(f)
        for u in inner:
            if u != v:
                t.append(("P", u))
        t.append(("R" if root else "C", v))
        for u in reversed(inner):
            if u != v:
                t.append(("U", u))
        for f in frontier:
            t.append(("U" if f in self.m.leaves else "G", f))
        return t, frontier

    @lru_cache(None)
    def cleanup(self, v):
        if v in self.m.leaves:
            return ()
        initial = {x: 2 for x in self.m.descendants(v) if x not in self.m.leaves}
        candidates = []
        for k in range(1, self.height(v) + 1):
            t, f = self.block(v, k)
            try:
                for u in f:
                    t += self.cleanup(u)
            except ValueError:
                continue
            r = self.m.check(t, initial=initial, final=False)
            assert all(r["state"][x] == 0 for x in initial)
            if r["peak_qubits"] <= self.cap:
                candidates.append((r["work"], r["peak_qubits"], k, t))
        if not candidates:
            raise ValueError(("no block cleanup under cap", v, self.cap))
        _, _, k, t = min(candidates)
        self.decisions[v] = k
        return tuple(t)

    def run(self, forced_root_depth=None):
        candidates = []
        depths = ([forced_root_depth] if forced_root_depth is not None else
                  range(1, self.height(self.m.root) + 1))
        for k in depths:
            t, f = self.block(self.m.root, k, root=True)
            try:
                for u in f:
                    t += self.cleanup(u)
            except ValueError:
                continue
            r = self.m.check(t)
            if r["peak_qubits"] <= self.cap:
                candidates.append((r["work"], r["peak_qubits"], k, t))
        if not candidates:
            raise ValueError(("no root plan under cap", self.cap))
        _, _, k, t = min(candidates)
        return t, {"root_depth": k, "cleanup_depths": self.decisions}


def reversible_baseline(model):
    """Ordinary reversible recursive compute--use--uncompute, same graph."""
    def inverse(trace):
        return [("U" if op == "P" else "P", v) for op, v in reversed(trace)]
    def clean(v):
        parts = [clean(c) for c in model.children[v]]
        forward = [e for t in parts for e in t]
        return forward + [("P", v)] + inverse(forward)
    branches = [clean(c) for c in model.children[model.root]]
    forward = [e for t in branches for e in t]
    return forward + [("R", model.root)] + inverse(forward)
