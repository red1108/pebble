# Pebble manuscript

`main.tex` is the tracked manuscript. It uses `references.bib` and
`figs/binary_pebbling_concept.png` from this repository.

[checkpoints/checkpoint0930.tex](checkpoints/checkpoint0930.tex) preserves the
author-provided manuscript before the September 30 revision, byte for byte.
Keep that checkpoint unchanged; continue editing `main.tex`.

When a PDF build is requested, run `python3 build.py` from any directory. The PDF is written to
`../paper/main.pdf`, and LaTeX intermediates go to `../paper/.build/`.
No PDF or build outputs belong in this Git repository.

`sequential.cpp` compares sequential KSS and block-cleanup schedules. It counts
work as pebble plus unpebble, excluding ghosting. `parallel.cpp` builds the
manuscript's ParallelPebble, equal-depth block, and batched forest schedule.
It checks first-phase active-set conflicts, second-phase ghosting, the round's
transient space, and final cleanup. Both use `pebbling_common.hpp` for tree
geometry, metrics, and `pebble_traversal`. This is a schedule simulator, not CPU
multithreading.

The paper's routine names use snake_case in C++:

| Manuscript | C++ |
|---|---|
| SequentialPebble | `sequential_pebble` |
| ParallelPebble | `parallel_pebble` |
| FastClean | `fast_clean` |
| Boundary | `build_boundary` |
| Parallel block contraction | `contract` |

The sequential Contract steps remain inside `fast_clean`; the parallel Resolve
stages remain inside `run_parallel`. The uniform-cut control uses
`uniform_contract`, and `contract_peak` gives the balanced-boundary space cost.

From this repository, build and test without leaving binaries in Git:

```sh
mkdir -p ../paper/.build
clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic sequential.cpp -o ../paper/.build/pebble-sequential
clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic parallel.cpp -o ../paper/.build/pebble-parallel
../paper/.build/pebble-sequential --test
../paper/.build/pebble-parallel --test
../paper/.build/pebble-sequential 10 11
../paper/.build/pebble-parallel 10 11
```

In the sequential comparison at height 10 and space 11, KSS uses work 13,823
and block cleanup uses 7,654; both peak at 11 pebbles. `--controls` includes
two regression schedules. The sequential `--test` suite checks 1,018
height/budget cases and 4,072 full
schedules. The parallel suite checks round legality and resources across all
budgets through height 8 and selected larger cases.

The surrounding local workspace is mapped in `../README.md`. Historical
manuscripts, superseded reports and old handoffs are indexed in
`../archive/README.md`, outside this Git repository. The parent `.ignore`
keeps archives, build outputs and external dependencies out of default searches.
