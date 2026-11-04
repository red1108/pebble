"""Call the shared C++ sequential scheduler for a supplied dependency tree."""
import subprocess


class SharedScheduler:
    def __init__(self, model, binary):
        self.labels = [None] + sorted(model.children)
        self.ids = {v: i for i, v in enumerate(self.labels) if i}
        self.process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True)
        lines = [f"{len(self.ids)} {self.ids[model.root]}"]
        for v in self.labels[1:]:
            children = model.children[v]
            assert len(children) in (0, 2)
            lines.append(" ".join(str(self.ids[c]) for c in children) if children else "0 0")
        self.process.stdin.write("\n".join(lines) + "\n")
        self.process.stdin.flush()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        try:
            self.process.stdin.close()
            self.process.wait(timeout=10)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            self.process.stdout.close()

    def request(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        count = self.process.stdout.readline()
        if not count:
            raise RuntimeError("The shared C++ scheduler stopped before returning its trace")
        trace = []
        for _ in range(int(count)):
            op, vertex = self.process.stdout.readline().split()
            trace.append((op, self.labels[int(vertex)]))
        return trace

    def frontier(self, vertices):
        return f"{len(vertices)} " + " ".join(str(self.ids[v]) for v in vertices)

    def sequential_pebble(self, vertex):
        return self.request(f"fast {self.ids[vertex]}")

    def contract(self, vertex, boundary, root=False):
        return self.request(f"block {self.ids[vertex]} {int(root)} {self.frontier(boundary)}")

    def fast_clean(self, vertex, boundaries, root=False):
        lines = [f"clean {self.ids[vertex]} {int(root)} {len(boundaries)}"]
        for v, boundary in boundaries.items():
            lines.append(f"{self.ids[v]} {self.frontier(boundary)}")
        return self.request("\n".join(lines))
