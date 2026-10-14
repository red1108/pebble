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

## Parallel spooky pebbling

The parallel algorithm evaluates independent subtrees concurrently and batches
their cleanup within the available space. Depth counts synchronous rounds.

Measured parallel depths at the minimum space budget `s = h + 1`:

| Height | Space budget | Parallel depth |
|---:|---:|---:|
| 4 | 5 | 45 |
| 6 | 7 | 269 |
| 8 | 9 | 1,109 |
| 10 | 11 | 5,037 |
| 12 | 13 | 23,381 |
| 14 | 15 | 102,229 |
| 16 | 17 | 424,560 |

The construction is asymptotically depth-optimal. These are the achieved
depths of this implementation, not certified exact minima for individual
heights.

## Run an individual algorithm

```sh
mkdir -p build
c++ -std=c++17 -O2 kss.cpp -o build/kss
c++ -std=c++17 -O2 sequential.cpp -o build/sequential
c++ -std=c++17 -O2 parallel.cpp -o build/parallel

./build/kss 10 11
./build/sequential 10 11
./build/parallel 10 11
```

Each program accepts `[height [space]]`. The defaults are height 10 and the
minimum feasible budget. Height counts vertex levels, so a complete tree
has `n = 2^h - 1` vertices. Supported heights are 1 through 20. For `h >= 2`,
the budget must satisfy `h + 1 <= s <= n`; a single leaf needs one pebble.

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

The parallel program simulates the paper's execution model. Its reported
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
├── kss.cpp                       # Sequential KSS baseline
├── parallel.cpp                  # Parallel tree and forest scheduling
├── pebbling_common.hpp           # Common input/output helpers
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
