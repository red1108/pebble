# Manuscript comparisons

Run all commands from the repository root. The scripts check their results
against the manuscript values in each directory's `expected.json`; they write
new results and executables only under `build/`.

## Section 6.1: complete binary trees

```sh
python3 applications/complete_trees/run.py
python3 applications/complete_trees/plot.py
```

The original sequential KSS schedule and our sequential schedule use the
minimum budget `H+1` for every `H=5,...,17`. The script measures work and
average memory utilization, including ghost-only steps. The plot reproduces
Figure 2; it requires Matplotlib and reads the generated `results.json`.

## Section 6.2: associative reductions

```sh
python3 applications/associative_reductions/run.py
python3 applications/associative_reductions/plot.py
```

For each height the scripts run both schedules at the KMRV implementation's
measured minimum executable budget. They compare work and parallel depth,
including input preparation and complete cleanup. The plot reproduces Figure 3
and requires Matplotlib. The evaluation trees differ, so the comparison
assumes an associative operation on the same ordered inputs.
The implementation accepts budgets of at least `H+1`; its minimum refers to
that accepted range, not to all possible prefix-path implementations. The
runner checks that every smaller accepted budget fails during input preparation.

## Section 6.3: CFS point-addition trees

See [`cfs/README.md`](cfs/README.md) for the pinned source setup and commands.
The scripts use the original CFS trees and KSS ordering, with both schedules
translated to the same retained-root endpoint. They reproduce the work table
for P-224, P-256, P-384, and P-521, and check action legality, phase
cancellation, and representative arithmetic values. This is a value-register
and unit-work comparison, not an estimate of full ECDLP circuit cost.

## Optional plotting dependency

Install Matplotlib in a virtual environment before running either plot script:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install 'matplotlib==3.11.2'
```

Use `.venv/bin/python` for the two `plot.py` commands. Figure output is placed
beside the generated results under `build/applications/`.
