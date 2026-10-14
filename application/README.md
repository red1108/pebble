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
compiles both circuit schedules, and prints the work/qubit and circuit-call
tables. `CXX` selects the compiler. To reproduce one instance:

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

## Scheduling and validation

The application calls `sequential_pebble`, `contract`, and `fast_clean` from
[sequential_schedule.hpp](../sequential_schedule.hpp). Boundary selection
uses CFS's incomplete tree and measured circuit costs. The circuit adapter
maps the resulting operations to Qarton, including the direct phase-correction
circuit for a ghosted block root. The public CFS implementation supplies the
baseline schedule.

Each run enumerates all cuts in the recursive block family and checks work,
space, and complete cleanup. Circuit simulations check four basis cases and
two four-term superpositions per schedule. An independent trace replay uses
32 random parity masks per trace. This checks the selected schedules; the
cut search is limited to the stated block family.

| File | Purpose |
|---|---|
| [reproduce.py](reproduce.py) | Run the three instances and print both tables |
| [cfs_benchmark.py](cfs_benchmark.py) | Build circuits and export costs and traces |
| [cut_planner.py](cut_planner.py) | Select boundaries using circuit costs and space limits |
| [circuit_model.py](circuit_model.py) | Account for circuit work and scratch space |
| [shared_scheduler.py](shared_scheduler.py), [schedule.cpp](schedule.cpp) | Call the shared C++ scheduler and translate its operations |
| [validate_cut_trace.py](validate_cut_trace.py) | Compile a trace and check values, phases, and space |
| [verify_trace.py](verify_trace.py) | Replay traces independently of the planner |
| [fetch_sources.py](fetch_sources.py) | Fetch and verify the CFS source revision |

## Public dependencies

| Project | Revision |
|---|---|
| [CFS compressed-ec-dlog](https://gitlab.inria.fr/capsule/qarton-projects/compressed-ec-dlog) | `7fedcc5b102d5d89f3a0229fd6ca56bfb41b4bda` |
| [Qarton](https://gitlab.inria.fr/capsule/qarton) | `d55356bab03c5b7454915683e052defd9c0f48f6` |

The upstream projects retain their own licenses. Their sources are downloaded
during setup and are not included in this repository.
