"""Fetch the public CFS circuits at the revision used in the paper."""
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
URL = "https://gitlab.inria.fr/capsule/qarton-projects/compressed-ec-dlog.git"
COMMIT = "7fedcc5b102d5d89f3a0229fd6ca56bfb41b4bda"


def fetch():
    target = HERE / "public" / "compressed-ec-dlog"
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "clone", "--no-checkout", URL, str(target)], check=True)
        subprocess.run(["git", "-C", str(target), "checkout", "--detach", COMMIT], check=True)
    actual = subprocess.check_output(["git", "-C", str(target), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(target), "status", "--porcelain", "--untracked-files=no"], text=True)
    if actual != COMMIT or dirty:
        raise RuntimeError(f"{target} must be an unchanged checkout of {COMMIT}; leaving it untouched.")
    print("CFS source:", COMMIT, flush=True)


if __name__ == "__main__":
    fetch()
