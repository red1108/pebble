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
    parser.add_argument("--experimental", action="store_true",
                        help="also show the separate cut-search and packing variant")
    args = parser.parse_args()
    if any(h<4 or h>20 for h in args.heights):
        parser.error("heights must be in [4, 20]")
    print("Building KMRV and the manuscript's parallel algorithm...", flush=True)
    names = ["kmrv", "parallel"]
    if args.experimental:
        names.append("parallel_optimized")
    try:
        binaries = build_programs(names)
    except ValueError as error:
        parser.error(str(error))
    print("N = 2^(h-1) inputs; path length l = N-1; space = ceil(2.47 log2(l)).")
    print("Same associative operation, charged input loads, and complete cleanup; only the output remains.")
    print("Our schedule follows the manuscript's forest-stage algorithm: fixed rules, no depth search or packing pass.")
    print("KMRV total includes input preparation, cleanup, and its packing pass.")
    print("Reduction compares the manuscript algorithm with packed KMRV.\n")
    header = (f"{'h':>3} {'inputs':>8} {'space':>6} {'KMRV path':>11} {'KMRV total':>12} "
              f"{'Our depth':>11} {'reduction':>10}")
    if args.experimental:
        header += f" {'Experimental':>13}"
    print(header, flush=True)
    print('-'*len(header), flush=True)
    rows = []
    for h in args.heights:
        inputs = 1<<(h-1)
        length = inputs-1
        space = math.ceil(2.47*math.log2(length))
        kmrv = run(binaries['kmrv'], h, space)
        ours = run(binaries['parallel'], h, space)
        experimental = run(binaries['parallel_optimized'], h, space) if args.experimental else None
        if not {'depth', 'path rounds', 'path peak'} <= kmrv.keys() or 'depth' not in ours:
            raise ValueError('missing parallel cost metrics')
        if experimental is not None and 'depth' not in experimental:
            raise ValueError('missing experimental depth')
        if not 1 <= kmrv['path peak'] <= space:
            raise ValueError('KMRV path exceeds the shared budget')
        if kmrv['path rounds'] != 2*length-1:
            raise ValueError('unexpected retained-output KMRV path depth')
        reduction = 100*(kmrv['depth']-ours['depth'])/kmrv['depth']
        line = (f"{h:>3} {inputs:>8,} {space:>6} {kmrv['path rounds']:>11,} "
                f"{kmrv['depth']:>12,} {ours['depth']:>11,} {reduction:>9.2f}%")
        if experimental is not None:
            line += f" {experimental['depth']:>13,}"
        print(line, flush=True)
        rows.append((h, space, kmrv, ours, experimental))

    print("\nWork and actual peak space:")
    header = (f"{'h':>3} {'space':>6} {'KMRV work':>13} {'Our work':>13} "
              f"{'KMRV peak':>10} {'Our peak':>9}")
    if args.experimental:
        header += f" {'Exp. work':>13} {'Exp. peak':>10}"
    print(header)
    print('-'*len(header))
    for h, space, kmrv, ours, experimental in rows:
        line = (f"{h:>3} {space:>6} {kmrv['total work']:>13,} {ours['total work']:>13,} "
                f"{kmrv['peak live']:>10} {ours['peak live']:>9}")
        if experimental is not None:
            line += f" {experimental['total work']:>13,} {experimental['peak live']:>10}"
        print(line)


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
