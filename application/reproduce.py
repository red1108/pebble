#!/usr/bin/env python3
"""Reproduce the CFS application tables from the public circuits."""
import argparse
from importlib.metadata import distribution, version
import json
from pathlib import Path
import subprocess
import sys

from fetch_sources import COMMIT as CFS_COMMIT, fetch
from verify_trace import verify

HERE = Path(__file__).resolve().parent
QARTON_COMMIT = "d55356bab03c5b7454915683e052defd9c0f48f6"
# curve, prime, and the paper's (baseline work, our work, compiled peak, cap).
CASES = {
    "p224": ("p224", 262139, (9306464, 7365187, 1165, 1165)),
    "p256": ("p256", 521177, (11559064, 8939833, 1303, 1325)),
    "p256-second-prime": ("p256", 499979, (11640184, 9007225, 1303, 1325)),
}


def run(script, *args, log):
    command = [sys.executable, str(HERE / script), *map(str, args)]
    with log.open("w") as output:
        result = subprocess.run(command, cwd=HERE, stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        tail = "\n".join(log.read_text().splitlines()[-12:])
        raise RuntimeError(f"{script} failed; see {log}\n{tail}")


def reproduce(name, output):
    curve, prime, expected = CASES[name]
    benchmark_path = output / f"{name}_benchmark.json"
    cut_path = output / f"{name}_cut.json"
    validation_path = output / f"{name}_validation.json"

    print(f"{name}: rebuilding circuits and comparing schedules...", flush=True)
    run("cfs_benchmark.py", "--curve", curve, "--prime", prime,
        "--qM-cofac", 2, "--Mpwp-cofac", 2, "--output", benchmark_path,
        log=output / f"{name}_benchmark.log")
    contract = json.loads(benchmark_path.read_text())
    cut = contract["cut"]
    cut_path.write_text(json.dumps(cut, indent=2) + "\n")
    print(f"{name}: checking circuits, values, phases, and cleanup...", flush=True)
    run("validate_cut_trace.py", benchmark_path, cut_path, "--output", validation_path,
        log=output / f"{name}_validation.log")

    validation = json.loads(validation_path.read_text())
    baseline, ours = validation["baseline_summary"], validation["summary"]
    baseline_trace = contract["rows"][contract["baseline"]]
    our_trace = dict(ours, trace=cut["trace"])
    for seed in range(32):
        verify(contract, baseline_trace, seed)
        verify(contract, our_trace, seed)

    actual = (baseline["work"], ours["work"], ours["compiled_qubits"], contract["cap"])
    if actual != expected or baseline["compiled_qubits"] != expected[2]:
        raise RuntimeError(f"{name}: measured resources {actual} differ from the paper's {expected}")
    if not cut["cut_audit"]["all_cut_sizes_enumerated"]:
        raise RuntimeError(f"{name}: incomplete cut search")

    return dict(curve=curve, prime=prime, leaves=contract["leaves"], cap=contract["cap"],
                qM_cofac=2, Mpwp_cofac=2, baseline=baseline, ours=ours,
                reduction_percent=100*(baseline["work"]-ours["work"])/baseline["work"],
                cut_audit=cut["cut_audit"], basis_trials_per_schedule=validation["basis_trials"],
                phase_trials_per_schedule=validation["phase_checks"]["superpositions"],
                independent_parity_trials_per_trace=32)


def print_tables(results):
    print("\nToffoli/AND counts per RNS prime; peak qubits are equal for both schedules.")
    print(f"{'Curve':<7} {'Prime':>7} {'CFS work':>13} {'Our work':>13} {'Reduction':>10} {'Peak / cap':>13}")
    for row in results.values():
        peak = f"{row['ours']['compiled_qubits']} / {row['cap']}"
        label = "P-" + row["curve"][1:]
        print(f"{label:<7} {row['prime']:>7} {row['baseline']['work']:>13,} "
              f"{row['ours']['work']:>13,} {row['reduction_percent']:>9.2f}% {peak:>13}")
    if "p256" in results:
        row = results["p256"]
        print("\nP-256 / 521177 circuit calls:")
        print(f"{'Operation':<27} {'CFS':>6} {'Ours':>6}")
        for label, key in [("Leaf lookup", "P:leaf"), ("Leaf uncomputation", "U:leaf"),
                           ("Add computation", "P:node"), ("Add uncomputation", "U:node"),
                           ("Direct phase correction", "C:node"), ("Root operation", "R:root")]:
            print(f"{label:<27} {row['baseline']['counts'][key]:>6} {row['ours']['counts'][key]:>6}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cases", nargs="*", default=list(CASES), metavar="CASE",
                        help="p224, p256, p256-second-prime (default: all three)")
    parser.add_argument("--output", type=Path, default=HERE / "results",
                        help="directory for generated JSON and logs")
    args = parser.parse_args()
    for name in args.cases:
        if name not in CASES:
            parser.error(f"unknown case: {name}")
    if sys.version_info < (3, 12) or not __debug__:
        parser.error("use Python 3.12 or newer, without -O")
    metadata = json.loads(distribution("qarton").read_text("direct_url.json") or "{}")
    if metadata.get("vcs_info", {}).get("commit_id") != QARTON_COMMIT:
        parser.error("install the pinned Qarton revision with pip install -r application/requirements.txt")
    fetch()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = {name: reproduce(name, output) for name in dict.fromkeys(args.cases)}
    summary = dict(source_commits=dict(cfs=CFS_COMMIT, qarton=QARTON_COMMIT),
                   dependencies={name: version(name) for name in ("qarton", "sympy", "tabulate", "tqdm")},
                   instances=results)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print_tables(results)
    print(f"\nFull traces, checks, and logs: {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, ModuleNotFoundError) as error:
        print(f"Reproduction stopped: {error}", file=sys.stderr)
        print("Setup instructions: application/README.md", file=sys.stderr)
        sys.exit(1)
