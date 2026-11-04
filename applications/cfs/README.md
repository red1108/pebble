# CFS point-addition tree comparison

Run from the repository root. Requirements are Python 3.12 or later, Git, a
C++17 compiler, and the pinned Qarton and Python dependencies below.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r applications/cfs/requirements.txt
.venv/bin/python applications/cfs/fetch_sources.py
.venv/bin/python applications/cfs/run.py
```

The source fetch pins `compressed-ec-dlog` to
`7fedcc5b102d5d89f3a0229fd6ca56bfb41b4bda`. The requirements pin Qarton to
`d55356bab03c5b7454915683e052defd9c0f48f6`. Source and generated output stay
under `build/`. An existing checkout of the pinned CFS revision can be used
instead by setting `CFS_SOURCE` to its absolute path. The checkout must have
no tracked changes. Run Python without `-O` so that validation assertions
remain enabled.

The code obtains each CFS tree and the public KSS ordering from the pinned
source. It translates direct phase corrections to a pebble and an unpebble,
and translates the root to a retained coordinate triple. Our schedule uses
the shared `sequential_schedule.hpp` block contraction and recursive cleanup
through the unit-cost adapter in `schedule.cpp`.
It chooses a feasible boundary minimizing work within this procedure, not
among arbitrary pebbling strategies. Both schedules are checked with the same
space budget and final state.

`run.py` checks the four rows against `expected.json`. It replays each trace
through the native state machine and on four representative input selections
with eight seeded measurement masks each. This checks legality, stored values,
phase cancellation, and agreement of the root residue with the public CFS
reference for those cases. These finite checks do not establish arithmetic
circuit costs or prove correctness for all inputs. The root residue extraction
and accumulator update are outside the measured work.
