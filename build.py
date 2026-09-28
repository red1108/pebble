"""Build the tracked manuscript with all generated files outside this repository."""

from pathlib import Path
import shutil
import subprocess


REPO = Path(__file__).resolve().parent
OUTPUT = REPO.parent / "paper"
BUILD = OUTPUT / ".build"


def main() -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        [
            "latexmk", "-pdf", "-silent", f"-outdir={BUILD}",
            "-interaction=nonstopmode", "-halt-on-error", "main.tex",
        ],
        cwd=REPO,
        text=True,
        capture_output=True,
    )
    if proc.returncode:
        raise RuntimeError(proc.stdout[-5000:] + proc.stderr[-5000:])
    shutil.copy2(BUILD / "main.pdf", OUTPUT / "main.pdf")
    print(f"Updated {OUTPUT / 'main.pdf'}; build files are in {BUILD}.")


if __name__ == "__main__":
    main()
