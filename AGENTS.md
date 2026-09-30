# Manuscript workflow

This repository's `main.tex` is the only active manuscript source. Do not
create or edit another active `main.tex` elsewhere in the workspace. Keep
`references.bib` and required files in `figs/` tracked here.

Build only when the user explicitly requests a PDF build, using `python3 build.py`.
It writes `../paper/main.pdf` and temporary
files under `../paper/.build/`, outside this Git repository.
`sequential.cpp` and `parallel.cpp` are the active C++ schedulers. Shared tree,
metric, and `pebble_traversal` code lives in `pebbling_common.hpp`. Put compiled
binaries in `../paper/.build/`, never in this repository.

Commit each user-requested manuscript revision in this repository after
checking its diff; build only when requested. Do not push unless asked. Preserve unrelated
changes and follow `../paper/EDITORIAL_GUIDE.md` for prose edits.

## Author's writing preferences

- Follow the reader's knowledge and the problem, choice, and guarantee sequence.
  Define the objects and conventions needed before using them in an argument.
- Remove repeated explanations while preserving logical links and smooth flow.
  Group shared meaning rather than enumerating every state. Keep pseudocode
  inputs and outputs concise and explicit.
- Avoid semicolon-heavy prose. Prefer full stops or connectors that state the
  actual relation, such as "so", "because", "since", "and", or "while".
  Pseudocode instruction separators may remain.
- Preserve the latest author draft and its notes and TODOs. When asked a question
  or for wording in chat, answer without editing files unless requested.
- Treat individual corrections as evidence for general writing principles,
  not as rigid templates for every paragraph or paper.

For other workspace material, start with `../README.md`. Historical files are
indexed in `../archive/README.md`; consult them only for a specific historical
question. The parent `.ignore` excludes archives and third-party sources from
default searches. Use an explicit path with `rg --no-ignore` when needed.
