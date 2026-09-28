# Manuscript workflow

This repository's `main.tex` is the only active manuscript source. Do not
create or edit another active `main.tex` elsewhere in the workspace. Keep
`references.bib` and required files in `figs/` tracked here.

Build with `python3 build.py`. It writes `../paper/main.pdf` and temporary
files under `../paper/.build/`, outside this Git repository.
`main.cpp` is the only active C++ comparison source. Put compiled binaries
in `../paper/.build/`, never in this repository.

Commit each user-requested manuscript revision in this repository after
checking its diff and build. Do not push unless asked. Preserve unrelated
changes and follow `../paper/EDITORIAL_GUIDE.md` for prose edits.
