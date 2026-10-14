"""Independent verifier of exported traces. Does not import the scheduler.

Checks prerequisites, states, live values, conservative all-scratch quantum
envelope, exact kernel costs, root single use, and absence of final garbage.
Measurement phase cancellation is checked with independent random parity masks.
This certifies the schedule, not the internals of the published arithmetic.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import random


def verify(data, row, seed=0):
    spec = data["model"]
    graph, leaves, root = spec["children"], set(spec["leaves"]), spec["root"]
    cost = {(r["op"], r["vertex"]): r for r in spec["costs"]}
    values, ghosts, phase, root_calls = {}, {}, 0, 0
    work, peak, counts, live_peak = 0, data["fixed_qubits"], Counter(), 0
    rng = random.Random(seed)
    # Arbitrary prescribed deterministic functions, deliberately not associative.
    # Random 64-bit truth values at each vertex give a phase-ledger stress test.
    prescribed = {v: rng.getrandbits(64) for v in graph}
    for step, (action, vertex) in enumerate(row["trace"]):
        kind = "root" if vertex == root else "leaf" if vertex in leaves else "node"
        if action in ("P", "C", "U", "R"):
            assert all(c in values for c in graph[vertex]), (step, action, "dependency")
        if action == "P":
            assert vertex not in values and vertex != root
            values[vertex] = prescribed[vertex]
            if vertex in ghosts:
                phase ^= (ghosts.pop(vertex) & values[vertex]).bit_count() & 1
        elif action == "G":
            assert vertex in values and vertex not in leaves and vertex != root
            assert vertex not in ghosts
            mask = rng.getrandbits(64)
            ghosts[vertex] = mask
            phase ^= (mask & values[vertex]).bit_count() & 1
        elif action == "C":
            assert vertex in ghosts and vertex not in values
            phase ^= (ghosts.pop(vertex) & prescribed[vertex]).bit_count() & 1
        elif action == "U":
            assert vertex in values and vertex not in ghosts
            assert values[vertex] == prescribed[vertex]
        elif action == "R":
            root_calls += 1
            assert root_calls == 1
        else:
            raise AssertionError(action)
        c = cost[action, vertex if kind == "leaf" else kind]
        work += c["work"]
        counts[action + ":" + kind] += 1
        live_peak = max(live_peak, len(values))
        peak = max(peak, data["fixed_qubits"] + data["node_width"] * len(values) + c["scratch"])
        if action in ("G", "U"):
            values.pop(vertex)
    assert not values and not ghosts and not phase and root_calls == 1
    assert work == row["work"] and peak == row["peak_qubits"]
    assert counts == row["counts"] and live_peak == row["peak_live"]
    assert peak <= data["cap"] and row["compiled_qubits"] <= data["compiled_cap"]
    # Conservative alternative: charge inverse lookups equal to forward lookups.
    inverse_upper = sum(cost["P", v]["work"] for op, v in row["trace"] if op == "U" and v in leaves)
    return dict(work=work, conservative_qubits=peak, compiled_qubits=row["compiled_qubits"],
                symmetric_lookup_work=work+inverse_upper, events=len(row["trace"]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    out = {}
    for file in args.files:
        data = json.loads(Path(file).read_text())
        checked = {}
        for name, row in data["rows"].items():
            for seed in range(32):
                checked[name] = verify(data, row, seed)
        out[file] = dict(rows=checked, parity_mask_trials_per_trace=32)
    Path(args.output).write_text(json.dumps(out, indent=2) + "\n")
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
