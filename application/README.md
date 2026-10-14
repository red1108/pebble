# CFS RNS point-addition application

These are the scripts used for the paper's application section. They compare
the public CFS schedule with our block cleanup on the same point-addition
tree, using the same arithmetic circuits, root operation, and space budget.

From the repository root, with Git and Python 3.12 or newer installed:

```sh
python3 -m venv application/.venv
source application/.venv/bin/activate
python -m pip install -r application/requirements.txt
python application/reproduce.py
```

The runner fetches the pinned CFS source, rebuilds the circuits, searches all
cuts in the recursive block family, and checks both schedules. It prints the
work/qubit table and the P-256 circuit-call table from the paper. To run one
instance:

```sh
python application/reproduce.py p224
python application/reproduce.py p256
python application/reproduce.py p256-second-prime
```

The first run generates public lookup tables and can take several minutes
and substantial disk space. Later runs reuse these tables. Dependencies,
caches, logs, and generated results are ignored by Git. Results go in
`application/results/`; use `--output PATH` to choose another directory.
The runner checks newly computed values against the paper's reported costs.

| Curve | RNS prime | CFS work | Our work | Reduction | Peak qubits / cap |
|---|---:|---:|---:|---:|---:|
| P-224 | 262139 | 9,306,464 | 7,365,187 | 20.86% | 1165 / 1165 |
| P-256 | 521177 | 11,559,064 | 8,939,833 | 22.66% | 1303 / 1325 |
| P-256 | 499979 | 11,640,184 | 9,007,225 | 22.62% | 1303 / 1325 |

Work uses the public Toffoli/AND cost model, including zero cost for inverse
dummy lookups. Peak qubits are measured by Qarton; the cap also accounts for
whole-kernel scratch. Both schedules use root cofactors `(2,2)`. The runs use
the public symbolic lookup and classical-function shortcuts. The results
measure this subroutine's work, not physical depth or a complete ECDLP attack.

The circuit checks use four basis cases and two four-term superpositions
per schedule. An independent replay checks each trace with 32 random parity
masks. The cut search is exhaustive within the recursive block template.

| Script | Purpose |
|---|---|
| `cfs_benchmark.py` | Build the public baseline and block schedules; export circuits' costs and traces |
| `scheduler.py` | Plan blocks with circuit costs and scratch-space accounting |
| `cut_planner.py`, `run_cut_search.py` | Search mixed-depth boundaries |
| `validate_cut_trace.py` | Compile the chosen trace and check values, phases, and space |
| `verify_trace.py` | Replay traces independently of the planner |
| `fetch_sources.py` | Fetch and check the public CFS revision |
| `reproduce.py` | Run the three paper instances and print both tables |

The scheduling and validation scripts are preserved from the original
application implementation. The circuit loader now uses the local pinned
checkout instead of paths elsewhere in the author's workspace.

Pinned public sources:

- [CFS compressed-ec-dlog](https://gitlab.inria.fr/capsule/qarton-projects/compressed-ec-dlog), commit `7fedcc5b102d5d89f3a0229fd6ca56bfb41b4bda`.
- [Qarton](https://gitlab.inria.fr/capsule/qarton), commit `d55356bab03c5b7454915683e052defd9c0f48f6`.

The upstream projects retain their own licenses. Their sources and generated
lookup data are downloaded during setup rather than included here.
