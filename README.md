# Spooky pebbling on binary trees

Implementations for *Optimal (Parallel) Spooky Pebbling on Binary Trees*.
The tree has `h` vertex levels and `n = 2^h - 1` vertices. Each run starts
empty and finishes with only the root pebbled, with every ghost cleared.

| File | Algorithm |
|---|---|
| [kss.cpp](kss.cpp) | Sequential KSS, following the original traversal |
| [sequential.cpp](sequential.cpp) | Our sequential balanced-boundary cleanup |
| [parallel.cpp](parallel.cpp) | Our parallel rounds, block cleanup, and forest resolution |
| [pebbling_common.hpp](pebbling_common.hpp) | Tree state, actions, counters, and `sequential_pebble` |

The function names follow the paper in snake_case: `sequential_pebble`,
`release`, `boundary`, `contract`, `fast_clean`, `parallel_pebble`, and `resolve`.
`fast_clean(x, r, b, rho)` takes the subtree root, height, local budget, and
root-retention flag. The parallel `contract(x, r, k, b)` uses an equal-depth
cut `k`. `resolve_space(r)` is the paper's budget `p(r)`.

Work is `place + unpebble`; ghosting costs no work. `pebble` also clears an
existing ghost and increments `exorcise`. `unpebble` requires a pebble, so
clearing a ghost and erasing its value takes two explicit actions.

The parallel program simulates the paper's synchronous rounds. It reports
depth and cumulative storage as well as work. Each round checks all
computation and uncomputation preconditions together and requires disjoint
active sets. Ghosting follows in the second phase. Space and cumulative
storage count `U_t = P_(t-1) union P_t^+`, including values uncomputed during
the round. This measures schedule depth rather than CPU running time.

Build and check with a C++17 compiler and Python 3:

```sh
python3 tests/check.py
../paper/.build/pebble-kss 10 11
../paper/.build/pebble-sequential 10 11
../paper/.build/pebble-parallel 10 11
```

Arguments are `[height [space]]`, defaulting to height 10 and the minimum
budget. Heights 1 through 20 are supported. For `h >= 2`, the budget must
satisfy `h + 1 <= s <= n`; a single leaf uses one pebble. KSS always follows
its minimum-space schedule, even when given a larger budget. Assertions
check legality and cleanup, so compile without `-DNDEBUG`.

At `h = 10, s = 11`:

| Algorithm | Work | Peak pebbles | Parallel depth |
|---|---:|---:|---:|
| KSS | 13,823 | 11 | — |
| Our sequential | 7,654 | 11 | — |
| Our parallel | 6,823 | 11 | 5,037 |

The parallel run has cumulative storage 32,370. The tests cover every
feasible budget through height 9 and selected cases through height 20.
They check final states, KSS counts, balanced-boundary costs, parallel
resources, and rejection of illegal rounds. These are finite execution
checks; the asymptotic arguments are in the paper.

`main.tex` is the manuscript source, with `references.bib` and `figs/` beside
it. When a PDF build is needed, `python3 build.py` writes `../paper/main.pdf`
and keeps temporary files in `../paper/.build/`.
