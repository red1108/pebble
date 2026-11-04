# Spooky pebbling on binary trees

This repository contains the implementations and numerical comparisons for
[*Optimal (Parallel) Spooky Pebbling on Binary Trees*](https://arxiv.org/abs/2610.09434).
The current manuscript's Section 6 comparisons are collected in
[`applications/`](applications/README.md). They count abstract pebbling
operations, not circuit gates or physical execution time.

## Reproduce the manuscript results

Run from the repository root with Python 3 and a C++17 compiler:

```sh
python3 applications/complete_trees/run.py
python3 applications/associative_reductions/run.py
```

These commands reproduce Sections 6.1 and 6.2 for every height 5 through 17
and check the results against the values recorded with the manuscript. The
second comparison evaluates KMRV's prefix path and our balanced tree at the
same measured minimum executable KMRV budget. Both schedules include input
preparation and cleanup and retain only the output.

The CFS point-addition-tree comparison in Section 6.3 additionally requires
Python 3.12 or later and the pinned public CFS and Qarton sources. Follow
[`applications/cfs/README.md`](applications/cfs/README.md) to set up and run
it. The CFS comparison retains the root coordinate triple and clears all
other values and ghosts. It is a unit-cost tree evaluation; the specialized
CFS root operation and subsequent accumulation are outside the comparison.

Optional figure-generation commands and the scope of each comparison are in
[`applications/README.md`](applications/README.md). Compiled programs and
results are written under `build/` and are not tracked.

## Algorithms and cost model

The command-line entry points are `kss.cpp`, `sequential.cpp`, and
`parallel.cpp`. The shared sequential operations and complete-tree boundary
selection are in `sequential_schedule.hpp` and `complete_tree_schedule.hpp`;
the KSS baseline is in `kss_schedule.hpp`. The parallel schedule is in
`parallel_schedule.hpp`. `kmrv.cpp` implements the retained-output prefix-path
baseline and its input preparation and cleanup. The separate
`parallel_optimized.cpp` is experimental and is not the manuscript algorithm.

- **Work:** pebble placements and uncomputations; ghosting has zero work.
- **Space:** value registers, including leaves, the retained root, and old
  and new values simultaneously occupied in a parallel round.
- **Depth:** nonempty rounds, each with a compute/uncompute phase followed by
  a ghosting phase.
- **Sequential memory utilization:** cumulative register occupancy divided by
  the space budget and the number of sequential actions, including ghosting.

The fixed-tree algorithms do not require associativity. The comparison with
KMRV assumes an associative operation and preserves the input order, allowing
the two methods to use different evaluation trees.

The sequential complete-tree schedule can also be called directly from C++:

```cpp
#include "complete_tree_schedule.hpp"

int main() {
    int height = 10, space = 11;
    QuantumComputer computer(height, space);
    fast_clean(computer, 1, height, space, true);
    check_final(computer);
    print_metrics(computer);
}
```

The CFS application supplies its own tree and boundary choices to the shared
block cleanup in `sequential_schedule.hpp`. The core scheduler does not depend
on the CFS implementation.

For individual runs, compile with a C++17 compiler and keep assertions enabled:

```sh
mkdir -p build
c++ -std=c++17 -O2 sequential.cpp -o build/sequential
c++ -std=c++17 -O2 parallel.cpp -o build/parallel
./build/sequential 10 11
./build/parallel 10 20
```

Set `CXX` to select a compiler for the reproduction scripts. The older
`compare.py` and `compare_parallel.py` utilities use their own default samples
and budgets; the `applications/` commands above reproduce the manuscript.
The previous circuit-cost experiments are retained in `application/` for
reference. They are separate from the current paper's unit-cost CFS table.

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
