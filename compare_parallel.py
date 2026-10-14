#!/usr/bin/env python3
"""Compare associative reductions at a common space budget."""
import argparse
import math
import subprocess
import sys

from compare import build_programs, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("heights", nargs="*", type=int, default=[4, 6, 8, 10, 12, 14, 16],
                        help="balanced-tree heights in [4, 20]")
    args = parser.parse_args()
    if any(h<4 or h>20 for h in args.heights):
        parser.error("heights must be in [4, 20]")
    print("Building KMRV, the paper schedule, and the optimized tree algorithm...", flush=True)
    try:
        binaries = build_programs(["kmrv", "parallel", "parallel_optimized"])
    except ValueError as error:
        parser.error(str(error))
    print("N = 2^(h-1) inputs; path length l = N-1; space = ceil(2.47 log2(l)).")
    print("Same associative operation, charged input loads, and complete cleanup; only the output remains.")
    print("KMRV and optimized tree use the same packing pass; the paper schedule is unchanged.")
    print("KMRV path = raw path rounds; total depth includes input preparation and cleanup.")
    print("Reduction compares the optimized tree with packed KMRV.\n")
    header = (f"{'h':>3} {'inputs':>8} {'space':>6} {'KMRV path':>11} {'KMRV total':>12} "
              f"{'Paper tree':>11} {'Optimized':>11} {'reduction':>10}")
    print(header, flush=True)
    print('-'*len(header), flush=True)
    rows = []
    for h in args.heights:
        inputs = 1<<(h-1)
        length = inputs-1
        space = math.ceil(2.47*math.log2(length))
        kmrv = run(binaries['kmrv'], h, space)
        ours = run(binaries['parallel'], h, space)
        optimized = run(binaries['parallel_optimized'], h, space)
        if not {'depth', 'path rounds', 'path peak'} <= kmrv.keys() or any(
                'depth' not in metrics for metrics in (ours, optimized)):
            raise ValueError('missing parallel cost metrics')
        if not 1 <= kmrv['path peak'] <= space:
            raise ValueError('KMRV path exceeds the shared budget')
        if kmrv['path rounds'] != 2*length-1:
            raise ValueError('unexpected retained-output KMRV path depth')
        reduction = 100*(kmrv['depth']-optimized['depth'])/kmrv['depth']
        print(f"{h:>3} {inputs:>8,} {space:>6} {kmrv['path rounds']:>11,} "
              f"{kmrv['depth']:>12,} {ours['depth']:>11,} {optimized['depth']:>11,} "
              f"{reduction:>9.2f}%", flush=True)
        rows.append((h, space, kmrv, ours, optimized))

    print("\nWork and actual peak space:")
    header = (f"{'h':>3} {'space':>6} {'KMRV work':>13} {'Paper work':>13} {'Optimized work':>15} "
              f"{'KMRV peak':>10} {'Paper peak':>11} {'Opt. peak':>10}")
    print(header)
    print('-'*len(header))
    for h, space, kmrv, ours, optimized in rows:
        print(f"{h:>3} {space:>6} {kmrv['total work']:>13,} {ours['total work']:>13,} "
              f"{optimized['total work']:>15,} {kmrv['peak live']:>10} "
              f"{ours['peak live']:>11} {optimized['peak live']:>10}")


if __name__ == '__main__':
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
