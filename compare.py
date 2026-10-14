#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def run(binary, height, space):
    result = subprocess.run([str(binary), str(height), str(space)],
                            check=True, capture_output=True, text=True)
    fields = {"place", "unpebble", "total work", "peak live"}
    metrics = {}
    for line in result.stdout.splitlines():
        key, _, value = line.partition(":")
        if key in fields:
            metrics[key] = int(value.strip())
    if metrics.keys() != fields:
        raise ValueError(f"{binary.name}: missing cost metrics")
    if metrics["total work"] != metrics["place"] + metrics["unpebble"]:
        raise ValueError(f"{binary.name}: inconsistent work count")
    if not 1 <= metrics["peak live"] <= space:
        raise ValueError(f"{binary.name}: invalid peak space")
    return metrics


def main():
    parser = argparse.ArgumentParser(description="Compare sequential KSS and our block cleanup.")
    parser.add_argument("heights", type=int, nargs="*", default=[4, 6, 8, 10, 12, 14, 16],
                        help="tree heights (default: 4 6 8 10 12 14 16)")
    parser.add_argument("--space", type=int,
                        help="common space budget for both algorithms (default: h+1, or 1 for a leaf)")
    args = parser.parse_args()

    cases = []
    for h in args.heights:
        if not 1 <= h <= 20:
            parser.error("heights must be in [1, 20]")
        minimum = 1 if h == 1 else h+1
        s = minimum if args.space is None else args.space
        if not minimum <= s <= (1 << h)-1:
            parser.error(f"height {h} needs a space budget in [{minimum}, {(1 << h)-1}]")
        cases.append((h, s))

    compiler = shlex.split(os.environ.get("CXX", "c++"))
    if not compiler or shutil.which(compiler[0]) is None:
        parser.error("C++ compiler not found; install one or set CXX (e.g. CXX=clang++)")

    repo = Path(__file__).resolve().parent
    build = repo / "build"
    build.mkdir(exist_ok=True)
    binaries = {}
    print("Building KSS and our sequential algorithm...", flush=True)
    for name in ["kss", "sequential"]:
        binary = build / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run(compiler + ["-std=c++17", "-O2", "-UNDEBUG",
                                   str(repo / (name + ".cpp")), "-o", str(binary)], check=True)
        binaries[name] = binary

    print("Work = pebble + unpebble; measurement costs no work.")
    print("Both algorithms use the same space budget in each row.")
    if args.space is not None:
        print("KSS keeps its original minimum-space schedule at larger budgets.")
    print()
    header = (f"{'h':>3} {'n':>10} {'budget':>7} {'KSS work':>14} {'Our work':>14} "
              f"{'reduction':>10} {'KSS peak':>9} {'Our peak':>9}")
    print(header, flush=True)
    print("-" * len(header), flush=True)
    for h, s in cases:
        kss = run(binaries["kss"], h, s)
        ours = run(binaries["sequential"], h, s)
        reduction = 100 * (kss["total work"] - ours["total work"]) / kss["total work"]
        print(f"{h:>3} {(1 << h)-1:>10,} {s:>7,} "
              f"{kss['total work']:>14,} {ours['total work']:>14,} {reduction:>9.2f}% "
              f"{kss['peak live']:>9,} {ours['peak live']:>9,}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        if error.stderr:
            print(error.stderr.rstrip(), file=sys.stderr)
        print(f"Comparison stopped: {error.cmd[0]} exited with code {error.returncode}.", file=sys.stderr)
        sys.exit(1)
    except (OSError, ValueError) as error:
        print(f"Comparison stopped: {error}", file=sys.stderr)
        sys.exit(1)
