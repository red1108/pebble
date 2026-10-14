# Pebbling algorithms

C++ implementations for *Optimal (Parallel) Spooky Pebbling on Binary Trees*.

To compare the sequential algorithms, install Python 3 and a C++17 compiler
(GCC or Clang), then run:

```sh
python3 compare.py
```

The script builds both programs and compares KSS with our sequential block
cleanup at heights 4, 6, 8, 10, 12, 14, and 16. Each row uses the same budget
`s = h + 1` and reports work, peak pebbles, and work reduction relative to KSS.
Work counts placements and uncomputations, excluding measurement.

Choose heights or a different common budget with:

```sh
python3 compare.py 6 10 14
python3 compare.py 10 12 --space 32
```

KSS keeps its original minimum-space schedule when given a larger budget.
Set `CXX=clang++` or `CXX=g++` to select a compiler. Binaries go in `build/`.

The paper's RNS point-addition application is in [application/](application/README.md).
Its original circuit scripts and a reproduction runner rebuild the three CFS
comparisons and print the work/qubit and circuit-call tables. Setup requires
Python 3.12+ and the pinned Qarton dependency; see that folder's README.

| File | Purpose |
|---|---|
| [compare.py](compare.py) | Build and compare the two sequential algorithms |
| [kss.cpp](kss.cpp) | Sequential KSS algorithm |
| [sequential.cpp](sequential.cpp) | Our sequential block cleanup |
| [parallel.cpp](parallel.cpp) | Our parallel schedule |
| [quantum_computer.hpp](quantum_computer.hpp) | State, actions, and resource counters |
| [pebbling_common.hpp](pebbling_common.hpp) | Shared scheduling and CLI helpers |
| [application/](application/README.md) | Reproduce the paper's CFS application tables |

Build with a C++17 compiler:

```sh
mkdir -p build
c++ -std=c++17 -O2 kss.cpp -o build/kss
c++ -std=c++17 -O2 sequential.cpp -o build/sequential
c++ -std=c++17 -O2 parallel.cpp -o build/parallel

./build/kss 10 11
./build/sequential 10 11
./build/parallel 10 11
```

Arguments are `[height [space]]`, defaulting to height 10 and the minimum
space budget. The complete tree has `n = 2^h - 1` vertices. Heights 1 through
20 are supported; for `h >= 2`, use `h + 1 <= space <= n`. A leaf needs one
pebble. Each schedule finishes with only the root pebbled and no ghosts.

`QuantumComputer` accepts scalar actions, vector actions, and mixed rounds.
Work counts placements and uncomputations. Measurement creates a ghost;
correction recomputes it and leaves a pebble. Parallel depth counts rounds,
and space includes all registers occupied during each round. The program
simulates the schedule rather than CPU multithreading.

Assertions check action legality and cleanup, so compile without `-DNDEBUG`.
