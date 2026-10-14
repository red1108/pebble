# Spooky pebbling on binary trees

This repository is the implementation of [*Optimal (Parallel) Spooky Pebbling on Binary
Trees*](https://arxiv.org/abs/2610.09434).

The repository includes the sequential [KSS25](https://quantum-journal.org/papers/q-2025-02-18-1636/)
algorithm as a baseline and compares it with our sequential algorithm based on
block cleanup. It also provides a parallel schedule for complete binary trees
and a CFS circuit adapter that reproduces the three CFS comparisons and the
P-256 circuit-call table.

## Compare the sequential algorithms

Requirements: Python 3 and a C++17 or later compiler.

```sh
git clone https://github.com/red1108/pebble.git
cd pebble
python3 compare.py
```

Both algorithms use the same budget `s = h + 1`. Work counts placements
and uncomputations.

| Height | KSS work | Our work | Work reduction |
|---:|---:|---:|---:|
| 4 | 71 | 60 | 15.49% |
| 6 | 479 | 351 | 26.72% |
| 8 | 2,687 | 1,683 | 37.37% |
| 10 | 13,823 | 7,654 | 44.63% |
| 12 | 67,583 | 34,390 | 49.11% |
| 14 | 319,487 | 149,756 | 53.13% |
| 16 | 1,474,559 | 648,405 | 56.03% |

To choose heights or a different budget:

```sh
python3 compare.py 6 10 14
python3 compare.py 10 12 --space 32
```

KSS follows its original minimum-space schedule even when the allowed budget
is larger.

Set `CXX=clang++` or `CXX=g++` to choose a compiler. Compiled programs are
written to `build/`.

## Compare parallel associative reductions

For an associative operation, the same ordered inputs can be evaluated by a
left fold or a balanced tree. We compare the retained-output variant of the
[KMRV A-sequence construction](https://arxiv.org/abs/2510.08432) on the prefix
path with our balanced-tree schedule. Both runs start empty, retain only the
final output, and clear every other pebble and ghost.

```sh
python3 compare_parallel.py
python3 compare_parallel.py 8 10 12
```

For balanced-tree height `h`, there are `N = 2^(h-1)` inputs and the prefix
path has length `ℓ = N-1`. Both algorithms receive the same total budget
`s = ceil(2.47 log₂ ℓ)`, including loaded inputs and the output register.

| Height | Shared space | Packed KMRV | Paper tree | Optimized tree | Depth reduction |
|---:|---:|---:|---:|---:|---:|
| 4 | 7 | 20 | 33 | 20 | 0.00% |
| 6 | 13 | 75 | 149 | 80 | -6.67% |
| 8 | 18 | 316 | 453 | 278 | 12.03% |
| 10 | 23 | 1,289 | 1,991 | 1,052 | 18.39% |
| 12 | 28 | 5,153 | 8,333 | 4,514 | 12.40% |
| 14 | 33 | 20,976 | 35,858 | 14,020 | 33.16% |
| 16 | 38 | 85,028 | 151,676 | 58,891 | 30.74% |

Reduction is `100 × (1 − optimized tree depth / packed KMRV depth)`.
Negative values mean the optimized tree is deeper. These are measured
schedule depths; neither implementation searches for a global optimum.

`parallel.cpp` keeps the paper's schedule. The separate
`parallel_optimized.cpp` chooses block cuts by estimated depth before packing.
At each boundary batch it subtracts the roots already retained, starts the
maximum number of subtrees whose minimum workspaces fit, and divides the
remaining workspace among them. Independent cleanup subtrees are batched in
the same way. Extra space can therefore be used inside each subtree as well
as to run more subtrees at once. When the whole tree fits, it computes all
levels and uncomputes every level except the root directly.

The optimized tree and KMRV both use `round_packing.hpp`. It moves compatible
actions from the next round into the current round, including partial
batches. Every computation and uncomputation still uses inputs present at
the start of the round, active sets remain disjoint, and transient space
includes values removed during that round. The pass preserves action counts.

Changing cuts and batching can trade more work for less depth. At height 16,
the paper tree uses 384,475 operations, the optimized tree uses 562,611, and
KMRV uses 554,813. The script reports work and actual peak space for all three.

The KMRV adapter uses the same `QuantumComputer` actions and space checks as
the tree implementation. It loads side inputs as needed, measures them after
use, and clears all input ghosts at the end. Path rounds are split into
batches if necessary, then the shared packing pass combines compatible actions.
Every load and final erasure costs one unit of work. The script also reports
the raw KMRV path depth, `2ℓ−1`, separately from the total depth above, which
includes input preparation and cleanup.

This comparison assumes associativity and preserves input order. It compares
two evaluation trees; the general fixed-tree algorithm does not require
associativity. Supported comparison heights are 4 through 20.

## Run an individual algorithm

```sh
mkdir -p build
c++ -std=c++17 -O2 kss.cpp -o build/kss
c++ -std=c++17 -O2 sequential.cpp -o build/sequential
c++ -std=c++17 -O2 parallel.cpp -o build/parallel
c++ -std=c++17 -O2 parallel_optimized.cpp -o build/parallel_optimized
c++ -std=c++17 -O2 kmrv.cpp -o build/kmrv

./build/kss 10 11
./build/sequential 10 11
./build/parallel 10 23
./build/parallel_optimized 10 23
./build/kmrv 10 23
```

The complete-tree programs accept `[height [space]]`, defaulting to height
10 and the minimum feasible budget. Height counts vertex levels, so a complete tree
has `n = 2^h - 1` vertices. Supported heights are 1 through 20. For `h >= 2`,
the budget must satisfy `h + 1 <= s <= n`; a single leaf needs one pebble.

`kmrv` takes the corresponding balanced-tree height; its default space is
`ceil(2.47 log₂ ℓ)`.

Every run starts with an empty tree and finishes with only the root pebbled
and no remaining ghosts. Assertions check action legality and cleanup;
compile without `-DNDEBUG` to keep these checks enabled.

## Cost model

- **Work:** placements plus uncomputations. Measurement creates a ghost and
  contributes no work. Recomputing a ghost also corrects its phase.
- **Space:** the peak number of occupied registers, including leaves and the
  retained root. Each pebble occupies one register in the tree examples.
- **Parallel depth:** synchronous rounds. Computations and uncomputations in
  a round require disjoint active sets; measurements follow in a second phase.
- **Cumulative storage:** the sum of occupied registers over rounds. Round
  occupancy includes both registers present at its start and newly placed
  pebbles, including values uncomputed or measured during that round.

The parallel programs simulate the paper's execution model. Their reported
depth is a schedule cost, not elapsed CPU time.

## Reproduce the circuit application

The [application guide](application/README.md) explains how to rebuild the
three CFS comparisons and the P-256 circuit-call table. This requires
Python 3.12+, Git, a C++17 compiler, and the pinned Python dependencies.
After setup, run:

```sh
python application/reproduce.py
```

The application uses circuit-specific gate and qubit costs. Its traversal,
block execution, and recursive cleanup reuse the same C++ scheduling functions
as the complete-tree implementation.

## Source layout

```text
pebble/
├── .gitignore
├── README.md
├── compare.py                    # Build and compare the sequential algorithms
├── compare_parallel.py           # Compare associative reductions against KMRV
├── kmrv.cpp                      # Retained-output KMRV construction and input adapter
├── kss.cpp                       # Sequential KSS baseline
├── parallel.cpp                  # Original parallel schedule from the paper
├── parallel_optimized.cpp        # Space-aware batching and packed execution
├── parallel_schedule.hpp         # Paper schedule and shared tree helpers
├── round_packing.hpp             # Shared action packing for KMRV and optimized tree
├── pebbling_common.hpp           # Round composition and input/output helpers
├── quantum_computer.hpp          # State, action checks, and resource counters
├── sequential.cpp                # Balanced-boundary selection
├── sequential_schedule.hpp       # Shared traversal, block execution, and cleanup
└── application/                  # CFS circuit adapter and reproduction
    ├── README.md
    ├── cfs_benchmark.py
    ├── circuit_model.py
    ├── cut_planner.py
    ├── fetch_sources.py
    ├── reproduce.py
    ├── requirements.txt
    ├── schedule.cpp
    ├── shared_scheduler.py
    ├── validate_cut_trace.py
    └── verify_trace.py
```

## Citation

```bibtex
@misc{lee2026spookypebbling,
  title         = {Optimal (Parallel) Spooky Pebbling on Binary Trees},
  author        = {Mingyu Lee and Sanghyun Lee and Kabgyun Jeong},
  year          = {2026},
  eprint        = {2610.09434},
  archivePrefix = {arXiv},
  primaryClass  = {cs.CC},
  url           = {https://arxiv.org/abs/2610.09434}
}
```
