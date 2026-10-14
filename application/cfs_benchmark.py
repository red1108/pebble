"""Reproduce and replace only the schedule in the pinned public CFS circuits."""
from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CFS_SOURCE = HERE / "public" / "compressed-ec-dlog"
if not (CFS_SOURCE / "rns_addition_tree_cost.py").is_file():
    raise SystemExit("Run python application/fetch_sources.py to fetch the pinned CFS source.")
sys.path.insert(0, str(CFS_SOURCE))

import rns_addition_tree_cost as cfs
from qarton.algorithms import PebblingBinaryTree
from qarton.circuit import AndFanoutResourceReporter, QuantumSimulator, UInt
from qarton.modular_arithmetic import ModInt
from rns_addition_tree_run import tree_full_computation
from scheduler import Cost, Model, Planner, reversible_baseline


class ScheduledTree(PebblingBinaryTree):
    def __init__(self, *args, schedule=None, **kwargs):
        self.schedule = schedule
        self.trace = []
        super().__init__(*args, **kwargs)

    def _nothing_to_pebble(self, t):
        self.trace.append(("R" if t == self._root_node else "P", t))
        super()._nothing_to_pebble(t)

    def _ghost_to_pebble(self, t):
        self.trace.append(("P", t))
        super()._ghost_to_pebble(t)

    def _pebble_to_nothing(self, t):
        self.trace.append(("U", t))
        super()._pebble_to_nothing(t)

    def _pebble_to_ghost(self, t):
        self.trace.append(("G", t))
        super()._pebble_to_ghost(t)

    def _ghost_to_nothing(self, t):
        self.trace.append(("C", t))
        super()._ghost_to_nothing(t)

    def pebble_root(self):
        if self.schedule is None:
            return super().pebble_root()
        for op, v in self.schedule:
            if op in ("P", "R"):
                self.place(v)
            elif op == "U":
                self._pebble_to_nothing(v)
            elif op == "G":
                self._pebble_to_ghost(v)
            elif op == "C":
                self._ghost_to_nothing(v)
        assert all(s == (1 if v == self._root_node else 0) for v, s in self.pebbles.items())


def qinputs(circuit):
    return sum(x.datatype.register_length() for x in circuit.input_signature if not x.classical)


def model_for(tree, node, phase, root):
    width = node.input_signature[0].datatype.register_length()
    fixed = qinputs(tree)
    costs = {}
    def cost(qc):
        return Cost(AndFanoutResourceReporter(qc).ccx_count(), qc.nbr_qubits() - qinputs(qc))
    for label, qc in tree._node_circuit.items():
        if label in tree._leaf_input_register:
            costs["P", label] = cost(qc)
            costs["U", label] = cost(qc.inverse())
    costs["P", "node"] = cost(node)
    costs["U", "node"] = cost(node.inverse())
    costs["C", "node"] = cost(phase)
    costs["G", "node"] = Cost(0, 0)
    costs["R", "root"] = cost(root)
    return Model(tree._label_to_children, set(tree._leaf_input_register),
                 tree._root_node, width, fixed, costs)


def summarize(qc, model):
    checked = model.check(qc.trace)
    actual = AndFanoutResourceReporter(qc).ccx_count()
    assert checked["work"] == actual, (checked["work"], actual)
    checked.pop("state")
    checked["compiled_qubits"] = qc.nbr_qubits()
    # This is *not* gate depth: the author leaf backend is a symbolic lookup.
    checked["trace"] = qc.trace
    return checked


def verify_values(qc, label, prime, lengths, trials, qM_cofac=1, Mpwp_cofac=1):
    """Public mixed symbolic simulator, with real ghost/phase control flow."""
    rng = random.Random(19092026)
    curve = cfs.curve_parameters(label)
    qc.make_real()
    cases = []
    for i in range(trials):
        indices = [rng.randrange(t) for t in lengths]
        residue = tree_full_computation(label, indices, prime)
        side0 = rng.randrange(1 << 12)
        side1 = rng.randrange(curve.ec.q)
        args = (*tuple(UInt(x) for x in indices), (UInt(side0), ModInt(side1, curve.ec.q)))
        # The same caller-selected root cofactors are used for both schedules.
        expected = (*args[:-1],
                    (UInt(side0 + qM_cofac * residue),
                     ModInt((side1 + Mpwp_cofac * residue) % curve.ec.q, curve.ec.q)))
        actual = qc.simulate(args)
        assert actual == expected, (i, actual[-1], expected[-1])
        cases.append(dict(indices=indices, residue=residue, side0=side0, side1=side1))
    return cases


def verify_phases(qc, label, prime, lengths, trials, qM_cofac=1, Mpwp_cofac=1):
    rng = random.Random(23092026)
    curve = cfs.curve_parameters(label)
    qc.make_real()
    for trial in range(trials):
        initial, expected = {}, {}
        for j, amp in enumerate((0.5, -0.5, 0.5j, -0.5j)):
            indices = [rng.randrange(t) for t in lengths]
            residue = tree_full_computation(label, indices, prime)
            side0, side1 = j, rng.randrange(curve.ec.q)
            args = (*tuple(UInt(x) for x in indices), (UInt(side0), ModInt(side1, curve.ec.q)))
            out = (*args[:-1],
                   (UInt(side0 + qM_cofac * residue),
                    ModInt((side1 + Mpwp_cofac * residue) % curve.ec.q, curve.ec.q)))
            initial[args] = amp
            expected[out] = amp
        actual = QuantumSimulator(qc).simulate_state(initial)
        assert set(actual) == set(expected), (trial, "support mismatch")
        assert all(abs(actual[x] - a) < 1e-10 for x, a in expected.items()), (trial, "relative phase")
    return dict(superpositions=trials, terms_per_superposition=4, amplitude_error_tolerance=1e-10)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--curve", default="p256")
    p.add_argument("--prime", type=int, default=521177)
    p.add_argument("--trials", type=int, default=0)
    p.add_argument("--phase-trials", type=int, default=0)
    p.add_argument("--qM-cofac", type=int, default=1)
    p.add_argument("--Mpwp-cofac", type=int, default=1)
    p.add_argument("--output", default="results/p256.json")
    args = p.parse_args()
    random.seed(19092026)
    curve = cfs.curve_parameters(args.curve)
    bits, rns_len, qm, u = cfs.rns_parameters(cfs.rns_bit_size(args.curve))
    assert args.prime.bit_length() == bits
    from sympy import isprime
    assert isprime(args.prime)
    a, b3 = curve.ec.a % args.prime, 3 * curve.ec.b % args.prime
    leaves, lengths = cfs.leaf_circuits(args.curve, args.prime, a, b3)
    node = cfs.ECRNSPointAddition(args.prime, a, b3)
    phase = cfs.ECRNSPointPhaseCorrection(args.prime, a, b3)
    root = cfs.ECRNSRootPointAdditionSide(args.prime, a, b3, curve.ec.q, qm + u,
                                         args.qM_cofac, args.Mpwp_cofac)
    kw = dict(leaf_circuits=leaves, node_circuit=node,
              node_phase_correction=phase, root_circuit=root)
    native = ScheduledTree(**kw)
    m = model_for(native, node, phase, root)
    rows = {"published_kss": summarize(native, m)}
    compiled_cap = native.nbr_qubits()
    # Whole-kernel scratch reservation is a conservative envelope. The Qarton
    # allocator may exploit lifetimes *inside* a kernel and use fewer qubits.
    cap = max(compiled_cap, rows["published_kss"]["peak_qubits"])
    print("Baseline", {k:v for k,v in rows["published_kss"].items() if k != "trace"}, flush=True)
    print("Kernel costs", {str(k): vars(v) for k,v in m.costs.items() if k[1] in ("node","root")}, flush=True)
    candidates = []
    for budget in sorted(set([cap] + list(range(m.fixed + 3*m.width, cap + 1, m.width)))):
        for forced in [None, 1, 2, 3]:
            try:
                plan, decisions = Planner(m, budget).run(forced)
            except ValueError:
                continue
            qc = ScheduledTree(**kw, schedule=tuple(plan))
            result = summarize(qc, m)
            if result["compiled_qubits"] > compiled_cap:
                continue
            name = f"ours_budget{budget}_root{decisions['root_depth']}"
            result["decisions"] = decisions
            rows[name] = result
            candidates.append((result["work"], result["compiled_qubits"], name, qc))
    best = min(candidates, key=lambda x:x[:3]) if candidates else None
    result = dict(curve=args.curve, prime=args.prime, leaves=len(leaves),
                  qM_cofac=args.qM_cofac, Mpwp_cofac=args.Mpwp_cofac,
                  fixed_qubits=m.fixed, node_width=m.width, cap=cap,
                  compiled_cap=compiled_cap,
                  rns_primes_log2=rns_len, author_lookup_inverse_ccx=0,
                  baseline="published_kss", best=best[2] if best else None, rows=rows)
    reversible = ScheduledTree(**kw, schedule=tuple(reversible_baseline(m)))
    revrow = summarize(reversible, m)
    revrow["within_same_budget"] = revrow["compiled_qubits"] <= compiled_cap and revrow["peak_qubits"] <= cap
    result["ordinary_reversible_baseline"] = revrow
    print("Reversible baseline", {k:v for k,v in revrow.items() if k != "trace"}, flush=True)
    result["model"] = dict(children=m.children, leaves=sorted(m.leaves), root=m.root,
                           costs=[dict(op=op, vertex=v, **vars(c)) for (op,v),c in m.costs.items()])
    if best:
        print("Best", best[:3], "improvement", 1-best[0]/rows["published_kss"]["work"], flush=True)
    if args.trials:
        result["basis_checks"] = {}
        for name, qc in [("published_kss", native)] + ([(best[2], best[3])] if best else []):
            print("Simulating", name, args.trials, flush=True)
            result["basis_checks"][name] = verify_values(
                qc, args.curve, args.prime, lengths, args.trials,
                args.qM_cofac, args.Mpwp_cofac)
    if args.phase_trials:
        result["phase_checks"] = {}
        for name, qc in [("published_kss", native)] + ([(best[2], best[3])] if best else []):
            print("Phase check", name, args.phase_trials, flush=True)
            result["phase_checks"][name] = verify_phases(
                qc, args.curve, args.prime, lengths, args.phase_trials,
                args.qM_cofac, args.Mpwp_cofac)
    path = HERE / args.output
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, indent=2) + "\n")
    print(path, flush=True)


if __name__ == "__main__":
    main()
