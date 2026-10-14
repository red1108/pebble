# Algorithm code

This repository contains the C++ pebbling implementations and the CFS
application reproduction code.
Keep manuscript sources, figures, checkpoints, paper build scripts, and test
folders out of this repository unless the user asks for them.

Keep the author's simple recursive style. QuantumComputer owns the state,
action checks, and counters; scheduling functions use its public interface.
Keep traversal, block execution, and recursive cleanup in sequential_schedule.hpp.
The CFS adapter must reuse that code rather than duplicate the schedules.
Use temporary files outside the repository for verification. Put compiled
binaries in the ignored build/ directory or outside the repository.
Preserve unrelated user edits. Do not push unless asked.
