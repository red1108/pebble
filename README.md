# Pebbling algorithms

C++ implementations for *Optimal (Parallel) Spooky Pebbling on Binary Trees*.

| File | Purpose |
|---|---|
| [kss.cpp](kss.cpp) | Sequential KSS algorithm |
| [sequential.cpp](sequential.cpp) | Our sequential block cleanup |
| [parallel.cpp](parallel.cpp) | Our parallel schedule |
| [quantum_computer.hpp](quantum_computer.hpp) | State, actions, and resource counters |
| [pebbling_common.hpp](pebbling_common.hpp) | Shared scheduling and CLI helpers |

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
