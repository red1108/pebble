"""Build the concision review, coloring changed sentences blue.

Run from any directory with `python3 build_review.py`. Generated sources and
compiler output stay outside the repository. The original and clean revision
are also built to compare pagination and verify that color adds no text.
"""
from pathlib import Path
import concurrent.futures
import difflib
import hashlib
import json
import shutil
import subprocess

REPO = Path(__file__).resolve().parent
REVIEW = REPO.parent / 'reviews/2026-09-30-concision'
BUILD = REPO.parent / 'paper/.build/concision-review'


def sentences(line):
    """Split prose at sentence ends, respecting TeX groups and inline math."""
    start = depth = 0
    math = False
    i = 0
    while i < len(line):
        char = line[i]
        if char == '\\':
            i += 2
            continue
        if char == '$':
            math = not math
        if char == '{':
            depth += 1
        elif char == '}':
            depth -= 1
        if not math and depth == 0 and char in '.!?' and (
                i+1 == len(line) or line[i+1].isspace()):
            yield line[start:i+1]
            start = i+1
        i += 1
    if start < len(line):
        yield line[start:]


def annotate(before, after):
    old_lines, new_lines = before.splitlines(True), after.splitlines(True)
    marked = list(new_lines)
    changed = []
    for kind, a, b, c, d in difflib.SequenceMatcher(
            None, old_lines, new_lines, autojunk=False).get_opcodes():
        if kind not in ('replace', 'insert'):
            continue
        original = {s.strip() for line in old_lines[a:b] for s in sentences(line)}
        for index in range(c, d):
            line = new_lines[index]
            if not line.strip():
                continue
            # The concision revision changes prose only. Never wrap structural
            # TeX commands or displays in a color group spanning environments.
            assert not line.lstrip().startswith((r'\begin', r'\end', r'\label'))
            fragments = []
            for sentence in sentences(line):
                stripped = sentence.strip()
                if stripped and stripped not in original:
                    prefix = sentence[:len(sentence)-len(sentence.lstrip())]
                    suffix = sentence[len(sentence.rstrip()):]
                    fragments.append(prefix + r'{\color{blue}' + stripped + '}' + suffix)
                    changed.append({'line': index+1, 'text': stripped})
                else:
                    fragments.append(sentence)
            marked[index] = ''.join(fragments)
    return ''.join(marked), changed


def build(name, source):
    folder = BUILD / name
    folder.mkdir(parents=True, exist_ok=True)
    tex = folder / f'{name}.tex'
    tex.write_text(source)
    result = subprocess.run([
        'latexmk', '-pdf', '-silent', f'-outdir={folder}',
        '-interaction=nonstopmode', '-halt-on-error', str(tex),
    ], cwd=REPO, text=True, capture_output=True)
    (folder/'build-output.txt').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(name + '\n' + result.stdout[-5000:] + result.stderr[-5000:])
    print(f'{name}: build passed', flush=True)
    return folder / f'{name}.pdf'


def main():
    before = (REVIEW/'before.tex').read_text()
    after = (REPO/'revised.tex').read_text()
    metadata = json.loads((REVIEW/'checks.json').read_text())
    assert hashlib.sha256(before.encode()).hexdigest() == metadata['baseline_sha256']
    assert hashlib.sha256(after.encode()).hexdigest() == metadata['revised_sha256']
    marked, changed = annotate(before, after)
    assert changed
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        tasks = {name: pool.submit(build, name, text) for name, text in (
            ('before', before), ('clean', after), ('marked', marked))}
        outputs = {name: task.result() for name, task in tasks.items()}
    shutil.copy2(outputs['marked'], REVIEW/'revised.pdf')
    (REVIEW/'blue-sentences.json').write_text(json.dumps(changed, indent=2)+'\n')
    print(f'Blue sentences: {len(changed)}')
    print(REVIEW/'revised.pdf')


if __name__ == '__main__':
    main()
