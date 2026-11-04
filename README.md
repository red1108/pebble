# Spooky pebbling on binary trees

Code and numerical comparisons for [*Optimal (Parallel) Spooky Pebbling on Binary Trees*](https://arxiv.org/abs/2610.09434).

## Reproduce the results

From the repository root, with Python 3 and a C++17 compiler:

```sh
python3 applications/complete_trees/run.py
python3 applications/associative_reductions/run.py
```

These reproduce Sections 6.1 and 6.2. For the CFS point-addition trees in
Section 6.3, follow [the CFS setup](applications/cfs/README.md). Figure
scripts and comparison details are in [applications/README.md](applications/README.md).
Generated files are stored under `build/`.

## Code

- `core/` contains the shared pebbling model, schedules, boundary selection,
  KSS baseline, and KMRV round packing.
- `cli/` contains standalone algorithm entry points.
- `applications/` contains the three manuscript comparisons. Each uses the
  shared core rather than a separate implementation of the tree algorithm.

For example, compile and run the sequential schedule:

```sh
mkdir -p build
c++ -std=c++17 -O2 -I core cli/sequential.cpp -o build/sequential
./build/sequential 10 11
```

Set `CXX` to choose the C++ compiler used by the reproduction scripts. Keep
assertions enabled when running the checks.

## Cost model

Work counts pebble placements and uncomputations; ghosting costs no work. Space
counts value registers, including leaves, the output, and old and new values
simultaneously occupied during a round. Parallel depth counts nonempty rounds.
Each round has a compute/uncompute phase followed by a ghosting phase. These
are abstract schedule costs, not circuit gate counts or physical runtime.

The KMRV comparison assumes associativity and preserves input order. Its
prefix path and our balanced tree evaluate the same reduction. The CFS
comparison retains the root coordinate triple and clears all other values and
ghosts. Residue extraction and accumulation are outside its scope.

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
