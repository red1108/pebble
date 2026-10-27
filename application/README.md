# RNS point-addition application

Reproduce the application results in [*Optimal (Parallel) Spooky Pebbling on
Binary Trees*](https://arxiv.org/abs/2610.09434). The comparison uses the public
circuits of Chevignard, Fouque, and Schrottenloher (CFS), replacing their
schedule with block cleanup while keeping the tree, leaf order, arithmetic
circuits, root operation, and space budget fixed.

## Setup and run

Requirements: Git, Python 3.12 or newer, and a C++17 compiler (GCC or Clang).
Run these commands from the repository root in Bash or Zsh:

```sh
python3 -m venv application/.venv
source application/.venv/bin/activate
python -m pip install -r application/requirements.txt
python application/reproduce.py
```

The runner fetches the pinned CFS source, builds the shared C++ scheduler,
compiles the CFS, sequential block-cleanup, and current manuscript parallel
schedules, and prints their work, space, circuit-call, and depth tables. `CXX` selects the compiler. To reproduce one instance:

```sh
python application/reproduce.py p224
python application/reproduce.py p256
python application/reproduce.py p256-second-prime
```

JSON results, action traces, and logs are written to `application/results/`.
Use `--output PATH` to choose another directory. The first run generates
lookup tables, which take several minutes and several hundred MB of disk
space; subsequent runs reuse them. Downloaded sources and generated files
are excluded from Git.

## Expected results

The runner checks the measured resources against these reported values:

| Curve | RNS prime | CFS work | Block-cleanup work | Reduction | Peak qubits / cap |
|---|---:|---:|---:|---:|---:|
| P-224 | 262139 | 9,306,464 | 7,365,187 | 20.86% | 1165 / 1165 |
| P-256 | 521177 | 11,559,064 | 8,939,833 | 22.66% | 1303 / 1325 |
| P-256 | 499979 | 11,640,184 | 9,007,225 | 22.62% | 1303 / 1325 |

Both schedules have the listed peak qubits. Work counts Toffoli/AND operations
under the public cost model, which assigns zero cost to inverse dummy
lookups. Qarton measures the compiled peak; the space cap also accounts for
whole-kernel scratch. Root cofactors are `(2,2)` in every comparison.

The circuits use the public implementation's symbolic lookup and
classical-function shortcuts. These figures describe this subroutine's
resource costs, rather than physical execution time or a complete ECDLP
attack.

## The current parallel algorithm

The parallel column calls `parallel_pebble` and `parallel_cleanup` directly
from [parallel_schedule.hpp](../parallel_schedule.hpp), the same routines
used by Algorithms 5 and 6. It uses the fixed largest-feasible-cut rule
and no scheduling solver.

The CFS tree has 17 or 19 leaves. The adapter embeds it in a complete tree
of the same height, generates the shared schedule, and drops virtual-node
actions. The actual CFS tree, leaf order, and arithmetic stay fixed. Inverse
lookups erase leaves. Unused ghost recomputation/erasure pairs become the
public direct phase-correction call; empty shallow-leaf cleanup is omitted.

A reserve for one non-root arithmetic kernel leaves 7 value registers for P-224 and
8 for P-256. The output accumulators are already included in fixed workspace,
so cleanup uses the same value-register budget. Within each generated round,
calls are split in emitted order whenever their simultaneous scratch and live
value registers would exceed the common qubit cap. Calls never move across
these round boundaries.

| Curve | RNS prime | Parallel work | Reduction vs CFS | Forward-lookup layers | Unit-call layers | Reserved peak |
|---|---:|---:|---:|---:|---:|---:|
| P-224 | 262139 | 7,365,187 | 20.86% | 37 | 100 | 1165 |
| P-256 | 521177 | 9,393,163 | 18.74% | 36 | 105 | 1325 |
| P-256 | 499979 | 9,460,555 | 18.73% | 36 | 105 | 1325 |

Forward-lookup depth counts batches containing a forward lookup. Unit-call
depth assigns one unit to each lookup, inverse lookup, point addition,
uncomputation, phase correction, or root call; measurement is free. These
are circuit-call metrics. The public symbolic lookup model does not supply
gate-level T-depth. Parallel space includes the sum of concurrent kernel
scratch, whereas Qarton's compiled peak is measured on a serialization.

## CFS depth references

The runner also replays fixed feasible CFS retimings stored in
[cfs_reference_batches.json](cfs_reference_batches.json). It verifies their
source-trace and cost-model hashes, dependencies, disjoint supports, workspace,
and final cleanup. No solver is required for this replay.

| Curve | CFS serial lookup / call layers | CFS retimed lookup / call layers | Current parallel lookup / call layers |
|---|---:|---:|---:|
| P-224 | 70 / 194 | 19 / 71 | 37 / 100 |
| P-256 (both primes) | 82 / 228 | 16 / 74 | 36 / 105 |

The two reference depths use separately optimized feasible schedules. They
are not certified minima. On these small weighted instances, the fixed
manuscript schedule is shallower than serial CFS but deeper than the reference
retimings. Its P-256 work is also higher than sequential block cleanup.
The tables report this tradeoff without selecting a different planner for
individual instances.

## Scheduling and validation

Sequential block cleanup calls `sequential_pebble`, `contract`, and `fast_clean`
from [sequential_schedule.hpp](../sequential_schedule.hpp), with boundaries
selected for the actual CFS costs within the recursive block family.
The parallel adapter uses the complete-tree scheduling functions described
above. Both versions keep the public root cofactors `(2,2)`.

Each run recompiles all schedules. Four basis cases and two four-term
superpositions per schedule check values and relative phases. Independent
trace replay uses 32 random parity masks. Parallel batches additionally
check start-of-round dependencies, disjoint supports, and simultaneous scratch.
All expected work and depth values are checked by `reproduce.py`.

The optional `compare_depth.py` remains a separate experiment that retimes
fixed traces with CP-SAT. To search for additional CFS reference batches:

```sh
python -m pip install ortools==9.15.6755
python application/compare_depth.py p224 p256 --schedules cfs --seconds 90
```

Its results do not replace the direct manuscript parallel schedule.

| File | Purpose |
|---|---|
| [reproduce.py](reproduce.py) | Rebuild and verify all paper tables |
| [cfs_benchmark.py](cfs_benchmark.py) | Compile circuits and export costs and traces |
| [cut_planner.py](cut_planner.py) | Choose sequential block boundaries |
| [shared_scheduler.py](shared_scheduler.py), [schedule.cpp](schedule.cpp) | Call the shared sequential C++ routines |
| [parallel.cpp](parallel.cpp), [parallel_adapter.py](parallel_adapter.py) | Export and lower the shared parallel schedule |
| [circuit_model.py](circuit_model.py) | Check serial traces and simultaneous batches |
| [validate_cut_trace.py](validate_cut_trace.py), [validate_parallel_trace.py](validate_parallel_trace.py) | Check the compiled circuits and their phases |
| [verify_trace.py](verify_trace.py) | Independently replay measurement phases |
| [reference_depth.py](reference_depth.py) | Replay the fixed CFS depth references |
| [compare_depth.py](compare_depth.py) | Optional fixed-trace retiming experiment |
| [fetch_sources.py](fetch_sources.py) | Fetch and verify the pinned public CFS source |

## Public dependencies

| Project | Revision |
|---|---|
| [CFS compressed-ec-dlog](https://gitlab.inria.fr/capsule/qarton-projects/compressed-ec-dlog) | `7fedcc5b102d5d89f3a0229fd6ca56bfb41b4bda` |
| [Qarton](https://gitlab.inria.fr/capsule/qarton) | `d55356bab03c5b7454915683e052defd9c0f48f6` |

The upstream projects retain their own licenses. Their sources are downloaded
during setup and are not included in this repository.
