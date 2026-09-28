# Pebble manuscript

`main.tex` is the tracked manuscript. It uses `references.bib` and
`figs/binary_pebbling_concept.png` from this repository.

Run `python3 build.py` from any directory. The PDF is written to
`../paper/main.pdf`, and LaTeX intermediates go to `../paper/.build/`.
No PDF or build outputs belong in this Git repository.

`main.cpp` is the tracked sequential KSS/block-cleanup comparison. It counts
work as pebble plus unpebble, excluding ghosting. From this repository, build
and test it without leaving a binary in Git:

```sh
mkdir -p ../paper/.build
clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic main.cpp -o ../paper/.build/pebble-benchmark
../paper/.build/pebble-benchmark --test
../paper/.build/pebble-benchmark 10 11
```

At height 10 and space 11, KSS uses work 13,823 and block cleanup uses 7,654;
both peak at 11 pebbles. `--controls` includes two regression schedules. The
`--test` suite checks 1,018 height/budget cases and 4,072 full schedules.
