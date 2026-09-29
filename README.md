# Pebble manuscript

`main.tex` is the tracked manuscript. It uses `references.bib` and
`figs/binary_pebbling_concept.png` from this repository.

Run `python3 build.py` from any directory. The PDF is written to
`../paper/main.pdf`, and LaTeX intermediates go to `../paper/.build/`.
No PDF or build outputs belong in this Git repository.

`sequential.cpp` compares sequential KSS and block-cleanup schedules. It counts
work as pebble plus unpebble, excluding ghosting. `parallel.cpp` builds the
manuscript's round-based Sweep, equal-depth block, and batched forest schedule.
It checks first-phase active-set conflicts, second-phase ghosting, the round's
transient space, and final cleanup. Both use `pebbling_common.hpp` for tree
geometry, metrics, and `fast_pebble` traversal. This is a schedule simulator, not CPU
multithreading.

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
