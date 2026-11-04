#!/usr/bin/env python3
"""Reproduce the equal-space KMRV comparison in Section 6.2."""
import json
import os
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = ROOT / 'build' / 'applications' / 'associative_reductions'


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    binaries = {}
    for kind in ('kmrv', 'parallel'):
        binary = BUILD / kind
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + [
            '-std=c++17', '-O2', '-UNDEBUG', '-I', str(ROOT / 'core'),
            str(ROOT / 'cli' / (kind + '.cpp')),
            '-o', str(binary)], check=True)
        binaries[kind] = binary
    expected = json.loads((HERE / 'expected.json').read_text())
    result = []
    for row in expected:
        h, space = row['height'], row['budget']
        for smaller in range(h + 1, space):
            attempt = subprocess.run([str(binaries['kmrv']), str(h), str(smaller)],
                                     capture_output=True, text=True)
            if attempt.returncode == 0:
                raise ValueError(f'H={h}: KMRV also succeeds with budget {smaller}')
            if 'KMRV input preparation does not fit this space budget' not in attempt.stderr:
                raise RuntimeError(f'H={h} budget={smaller}: unexpected failure: {attempt.stderr}')
        values = {}
        for kind, output_key in [('kmrv', 'kmrv'), ('parallel', 'ours')]:
            output = subprocess.check_output([str(binaries[kind]), str(h), str(space)], text=True)
            metrics = {}
            for line in output.splitlines():
                key, sep, value = line.partition(': ')
                if sep and value.isdigit():
                    metrics[key] = int(value)
            measured = {'work': metrics['total work'], 'depth': metrics['depth']}
            if measured != row[output_key]:
                raise ValueError(f'H={h} {output_key}: {measured} != {row[output_key]}')
            if metrics['peak live'] > space:
                raise ValueError(f'H={h}: {output_key} exceeds budget')
            values[output_key] = measured
        result.append({'height': h, 'budget': space, **values})
        print(f"H={h:2} space={space:2} work={values['kmrv']['work']:8} -> {values['ours']['work']:8}  "
              f"depth={values['kmrv']['depth']:7} -> {values['ours']['depth']:7}", flush=True)
    path = BUILD / 'results.json'
    path.write_text(json.dumps(result, indent=2) + '\n')
    print(f'All results match the manuscript table. Saved to {path}')


if __name__ == '__main__':
    main()
