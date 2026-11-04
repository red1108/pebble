#!/usr/bin/env python3
"""Reproduce the sequential work and occupancy comparison in Section 6.1."""
import json
import os
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = ROOT / 'build' / 'applications' / 'complete_trees'


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    binary = BUILD / 'measure'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + [
        '-std=c++17', '-O2', '-UNDEBUG', '-I', str(ROOT / 'core'),
        str(HERE / 'measure.cpp'), '-o', str(binary)], check=True)
    expected = json.loads((HERE / 'expected.json').read_text())
    result = []
    for row in expected:
        h, space = row['height'], row['budget']
        values = {}
        for kind, output_key in [('kss', 'kss'), ('sequential', 'ours')]:
            metrics = json.loads(subprocess.check_output(
                [str(binary), kind, str(h), str(space)], text=True))
            for key, value in row[output_key].items():
                if metrics[key] != value:
                    raise ValueError(f'H={h} {output_key} {key}: {metrics[key]} != {value}')
            metrics['utilization'] = metrics['area'] / (space * metrics['depth'])
            values[output_key] = metrics
        result.append({'height': h, 'budget': space, **values})
        print(f"H={h:2} space={space:2} work={values['kss']['work']:8} -> {values['ours']['work']:8}  "
              f"utilization={values['kss']['utilization']:.2%} -> {values['ours']['utilization']:.2%}", flush=True)
    path = BUILD / 'results.json'
    path.write_text(json.dumps(result, indent=2) + '\n')
    print(f'All results match the manuscript data. Saved to {path}')


if __name__ == '__main__':
    main()
