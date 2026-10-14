#!/usr/bin/env python3
import os
from pathlib import Path
import shlex
import subprocess

repo = Path(__file__).resolve().parents[1]
build = repo.parent / 'paper' / '.build'
build.mkdir(parents=True, exist_ok=True)
compiler = shlex.split(os.environ.get('CXX', 'c++'))
flags = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic']
for source, name in [('kss.cpp', 'kss'), ('sequential.cpp', 'sequential'),
                     ('parallel.cpp', 'parallel'), ('tests/check.cpp', 'check')]:
    subprocess.run(compiler + flags + [str(repo / source), '-o', str(build / ('pebble-' + name))],
                   check=True)

subprocess.run([str(build / 'pebble-check')], check=True, cwd=build)
for name in ['dependency', 'conflict', 'space', 'ghost', 'duplicate', 'unpebble_ghost']:
    result = subprocess.run([str(build / 'pebble-check'), name], capture_output=True, text=True, cwd=build)
    assert result.returncode != 0 and 'assert' in result.stderr.lower(), name

for name in ['kss', 'sequential', 'parallel']:
    binary = str(build / ('pebble-' + name))
    for args in [['0'], ['21'], ['-1'], ['2', '2'], ['2', '4'], ['10x'], ['3', '4x'], ['3', '4', '5']]:
        result = subprocess.run([binary] + args, capture_output=True, text=True, cwd=build)
        assert result.returncode == 1 and 'usage:' in result.stderr, (name, args)
    result = subprocess.run([binary], check=True, capture_output=True, text=True, cwd=build)
    expected = {'kss': 13823, 'sequential': 7654, 'parallel': 6823}[name]
    assert f'total work: {expected}\n' in result.stdout
    assert 'peak live: 11\n' in result.stdout
    if name == 'parallel':
        assert 'depth: 5037\n' in result.stdout
        assert 'cumulative storage: 32370\n' in result.stdout
print('PASS: invalid rounds rejected, command-line checks, and h=10/s=11 regression.')
