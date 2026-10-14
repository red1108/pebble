# Spooky pebbling on binary trees

Implementations accompanying [*Optimal (Parallel) Spooky Pebbling on Binary
Trees*](https://arxiv.org/abs/2610.09434) by Mingyu Lee, Sanghyun Lee, and
Kabgyun Jeong.

The repository includes the sequential KSS baseline, sequential block cleanup,
and a parallel schedule for complete binary trees. It also reproduces the
paper's circuit-cost comparison on CFS RNS point-addition trees.

## Compare the sequential algorithms

Requirements: Python 3 and a C++17 compiler such as GCC or Clang.

```sh
git clone https://github.com/red1108/pebble.git
cd pebble
python3 compare.py
```

The script builds both sequential programs and compares heights
`4, 6, 8, 10, 12, 14, 16`. Each row reports work, work reduction relative to
KSS, and the peak number of pebbles, under a common budget `s = h + 1`.

To choose heights or a different budget:

```sh
python3 compare.py 6 10 14
python3 compare.py 10 12 --space 32
```

For `h = 10` and `s = 11`, KSS uses **13,823** work and block cleanup uses
**7,654**, a **44.63%** reduction. Both peak at **11** pebbles. KSS follows
its original minimum-space schedule even when the allowed budget is larger.

Set `CXX=clang++` or `CXX=g++` to choose a compiler. Compiled programs are
written to `build/`.

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

| File | Purpose |
|---|---|
| [compare.py](compare.py) | Build and compare the sequential algorithms |
| [kss.cpp](kss.cpp) | Sequential KSS baseline |
| [sequential.cpp](sequential.cpp) | Balanced-boundary selection for complete trees |
| [sequential_schedule.hpp](sequential_schedule.hpp) | Shared traversal, block execution, and recursive cleanup |
| [parallel.cpp](parallel.cpp) | Parallel tree and forest scheduling |
| [quantum_computer.hpp](quantum_computer.hpp) | Abstract machine state, action checks, and resource counters |
| [pebbling_common.hpp](pebbling_common.hpp) | Common input/output helpers |
| [application/](application/README.md) | CFS circuit adapter and reproduction scripts |

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
