# Spooky pebbling on binary trees

Implementations for *Optimal (Parallel) Spooky Pebbling on Binary Trees*.
The tree has `h` vertex levels and `n = 2^h - 1` vertices. Each run starts
empty and finishes with only the root pebbled, with every ghost cleared.

| File | Algorithm |
|---|---|
| [kss.cpp](kss.cpp) | Sequential KSS, following the original traversal |
| [sequential.cpp](sequential.cpp) | Our sequential balanced-boundary cleanup |
| [parallel.cpp](parallel.cpp) | Our parallel rounds, block cleanup, and forest resolution |
| [quantum_computer.hpp](quantum_computer.hpp) | Abstract computer: state, legal actions, and resource counts |
| [pebbling_common.hpp](pebbling_common.hpp) | Shared scheduling and input/output helpers |

The function names follow the paper in snake_case: `sequential_pebble`,
`release`, `boundary`, `contract`, `fast_clean`, `parallel_pebble`, and `resolve`.
`fast_clean(qc, x, r, b, rho)` takes the computer, subtree root, height, local
budget, and root-retention flag. The parallel `contract(x, r, k, b)` uses an equal-depth
cut `k`. `resolve_space(r)` is the paper's budget `p(r)`.

`QuantumComputer` owns the state and counters. Scheduling functions receive
it by reference; independent instances do not share state. Its four actions
accept either an index or a vector of indices. Each call is one round, and
`execute` combines computations/uncomputations with a second measurement phase.

```cpp
QuantumComputer qc(2, 3);
qc.pebble(vector<int>{2, 3});
qc.execute({{1}, {}, {2, 3}});
qc.correction(vector<int>{2, 3});
qc.unpebble(vector<int>{2, 3});
assert(qc.finished());
cout << qc.metrics().work() << endl; // 7
```

Work is `place + unpebble`; `measure` creates ghosts and costs no work.
`correction` recomputes a ghost and corrects its phase, leaving a pebble.
It counts one placement and one correction, hence one work. Ordinary
`pebble` on a ghost does the same correction automatically. `unpebble`
requires a pebble. `metrics()` exposes the counters for read-only access,
including `occupied`, `peak`, `depth`, and `area`. Scalar calls also count
rounds and storage, so sequential depth equals the total action count.

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
resources, independent computer instances, scalar/vector calls, and
rejection of illegal operations. These are finite execution
checks; the asymptotic arguments are in the paper.

`main.tex` is the manuscript source, with `references.bib` and `figs/` beside
it. When a PDF build is needed, `python3 build.py` writes `../paper/main.pdf`
and keeps temporary files in `../paper/.build/`.
