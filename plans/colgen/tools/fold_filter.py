"""Snapshot filter for the column generation stack's fold (fold-map.md section 2).

Rewrites every commit of BASE..TIP by transforming its whole tree: paths moved by MOVES, files
dropped by DROPS, files reset to BASE's blob by RESET, and the content RULES applied to the text
of the files that land in the example or in the renamed pure package. Because each commit's
tree is transformed as a snapshot, a file that later commits edit is moved in all of them at
once and the diffs follow it: no conflicts. A commit whose tree equals its new parent's is
dropped. Writes objects only: no ref and no branch moves; prints old -> new per commit.

    python plans/colgen/tools/fold_filter.py origin/main feature/column-generation-pure-python
        [--keep-pure-name] [--report out.tsv]
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

EXAMPLE = "examples/column_generation/"

# MARK: Path moves (first match on the original path)

MOVES = [
    ("cpp/rcspp/column_generation.hpp", EXAMPLE + "cpp/rcspp/column_generation.hpp"),
    ("cpp/rcspp/column_generation/", EXAMPLE + "cpp/rcspp/column_generation/"),
    ("cpp/rcspp_backends/", EXAMPLE + "cpp/rcspp/column_generation/"),
    ("tests/cpp/gurobi/", EXAMPLE + "tests/cpp/gurobi/"),
    ("tests/cpp/highs/", EXAMPLE + "tests/cpp/highs/"),
    ("tests/cpp/test_column_generation", EXAMPLE + "tests/cpp/test_column_generation"),
    ("python/bindings/column_generation", EXAMPLE + "bindings/column_generation"),
    ("python/bindings/graph_pricer.hpp", EXAMPLE + "bindings/graph_pricer.hpp"),
    (
        "python/src/rcspp/_core/column_generation.pyi",
        EXAMPLE + "python/typings/rcspp/_core/cpp_column_generation.pyi",
    ),
    ("python/src/rcspp/column_generation/", EXAMPLE + "python/cpp_column_generation/"),
    # Named after the example's package: the renamed pure tests take the old names (D-160).
    ("tests/python/test_column_generation", EXAMPLE + "tests/python/test_cpp_column_generation"),
    (
        "tests/python/column_generation_fakes.py",
        EXAMPLE + "tests/python/cpp_column_generation_fakes.py",
    ),
    ("examples/python/column_generation_loop.py", EXAMPLE + "python/column_generation_loop.py"),
]

PURE_MOVES = [
    ("python/src/rcspp/python_column_generation/", "python/src/rcspp/column_generation/"),
    ("tests/python/test_python_column_generation", "tests/python/test_column_generation"),
    ("tests/python/python_column_generation_fakes.py", "tests/python/column_generation_fakes.py"),
]

# MARK: Drops and resets (original paths)

# The stack's CMake files of the layer, its master solvers and their tests: the example's are
# written by hand in the manual pass. The find_package consumers of the components go (#80).
DROPS = re.compile(
    r"^(cpp/rcspp_backends/(gurobi|highs)/CMakeLists\.txt"
    r"|cpp/rcspp/column_generation/((gurobi|highs)/)?CMakeLists\.txt"
    r"|tests/cpp/(gurobi|highs)/CMakeLists\.txt"
    r"|tests/find_package/column_generation\w*\.cpp)$"
)

# Files whose every stack change is column generation's: BASE's blob in every commit.
RESET = {
    "cpp/rcspp/rcspp.hpp",
    "tests/cpp/test_main.cpp",
    "python/src/rcspp/__init__.py",
    "python/src/rcspp/_core/__init__.pyi",
}

# MARK: Content rules (on the destination path)

TEXT = (".py", ".pyi", ".cpp", ".hpp", ".md", ".txt", ".cmake")

EXAMPLE_RULES = [
    (re.compile(r"rcspp_backends/(gurobi|highs)/"), r"rcspp/column_generation/\1/"),
    # The package leaves rcspp: its relative imports of rcspp become absolute.
    (
        re.compile(r"from \.\.\.?_core\.column_generation import"),
        "from rcspp._core.cpp_column_generation import",
    ),
    (re.compile(r"from \.\.\.?_core\.graph import"), "from rcspp._core.graph import"),
    (re.compile(r"from \.\.graph import"), "from rcspp.graph import"),
    # The stub leaves rcspp/_core/ (typings/rcspp/_core/ resolves no relative import).
    (re.compile(r"^from \.graph import", re.MULTILINE), "from rcspp._core.graph import"),
    (re.compile(r"\brcspp\._core\.column_generation\b"), "rcspp._core.cpp_column_generation"),
    (re.compile(r"\brcspp\.column_generation\b"), "cpp_column_generation"),
    (re.compile(r"\bcolumn_generation_fakes\b"), "cpp_column_generation_fakes"),
    (
        re.compile(r"^(from|import) test_column_generation", re.MULTILINE),
        r"\1 test_cpp_column_generation",
    ),
    (
        re.compile(r"\bRCSPP_USE_(?:COLUMN_GENERATION_)?(GUROBI|HIGHS)\b"),
        r"RCSPP_USE_COLUMN_GENERATION_EXAMPLE_\1",
    ),
    (re.compile(r"\bRCSPP_USE_COLUMN_GENERATION\b"), "RCSPP_USE_COLUMN_GENERATION_EXAMPLE"),
    (
        re.compile(r"\bUSE_(?:COLUMN_GENERATION_)?(GUROBI|HIGHS)\b"),
        r"USE_COLUMN_GENERATION_EXAMPLE_\1",
    ),
    (re.compile(r"\bUSE_COLUMN_GENERATION\b"), "USE_COLUMN_GENERATION_EXAMPLE"),
]

PURE_RULES = [
    (re.compile(r"\brcspp\.python_column_generation\b"), "rcspp.column_generation"),
    (re.compile(r"\bpython_column_generation_fakes\b"), "column_generation_fakes"),
    (
        re.compile(r"python/src/rcspp/python_column_generation/"),
        "python/src/rcspp/column_generation/",
    ),
]


def git(*args, data=None, env=None):
    result = subprocess.run(
        ["git", *args], input=data, capture_output=True, check=True, env=env
    )
    return result.stdout


def move(path, moves):
    for source, destination in moves:
        if path.startswith(source):
            return destination + path[len(source) :]
    return None


class Filter:
    def __init__(self, base, rename_pure):
        self.base = base
        self.moves = MOVES + (PURE_MOVES if rename_pure else [])
        self.rename_pure = rename_pure
        self.blob_cache = {}
        self.base_blobs = {}
        for path in RESET:
            line = git("ls-tree", base, "--", path).decode().split()
            self.base_blobs[path] = line[2] if line else None

    def rules_for(self, destination):
        if self.rename_pure and destination == ".flake8":
            return PURE_RULES
        if not destination.endswith(TEXT):
            return []
        if destination.startswith(EXAMPLE):
            return EXAMPLE_RULES
        if self.rename_pure and destination.startswith(("python/src/rcspp/", "tests/python/")):
            return PURE_RULES
        return []

    def blob(self, sha, destination):
        rules = self.rules_for(destination)
        if not rules:
            return sha
        key = (sha, destination)
        if key not in self.blob_cache:
            text = git("cat-file", "blob", sha).decode("utf-8")
            new_text = text
            for pattern, replacement in rules:
                new_text = pattern.sub(replacement, new_text)
            if new_text == text:
                self.blob_cache[key] = sha
            else:
                data = new_text.encode("utf-8")
                written = git("hash-object", "-w", "--stdin", data=data)
                self.blob_cache[key] = written.decode().strip()
        return self.blob_cache[key]

    def tree(self, commit, index_file):
        entries = {}
        listing = git("ls-tree", "-r", "-z", "--full-tree", commit).decode("utf-8")
        for record in filter(None, listing.split("\0")):
            meta, path = record.split("\t", 1)
            mode, kind, sha = meta.split()
            if DROPS.match(path):
                continue
            if path in RESET:
                if self.base_blobs[path] is None:
                    continue
                sha = self.base_blobs[path]
            destination = move(path, self.moves) or path
            if destination in entries:
                sys.exit(f"{commit}: two files land at {destination}")
            if kind == "blob":
                sha = self.blob(sha, destination)
            entries[destination] = (mode, kind, sha)
        index = "".join(f"{m} {k} {s}\t{p}\0" for p, (m, k, s) in sorted(entries.items()))
        env = dict(os.environ, GIT_INDEX_FILE=index_file)
        git("read-tree", "--empty", env=env)
        git("update-index", "-z", "--index-info", data=index.encode("utf-8"), env=env)
        return git("write-tree", env=env).decode().strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("base")
    parser.add_argument("tip")
    parser.add_argument("--keep-pure-name", action="store_true")
    parser.add_argument("--report")
    arguments = parser.parse_args()

    fold = Filter(arguments.base, rename_pure=not arguments.keep_pure_name)
    commits = git("rev-list", "--reverse", f"{arguments.base}..{arguments.tip}").decode().split()
    parent = git("rev-parse", arguments.base).decode().strip()
    parent_tree = git("rev-parse", f"{parent}^{{tree}}").decode().strip()
    rows = []
    with tempfile.TemporaryDirectory() as scratch:
        index_file = os.path.join(scratch, "index")
        for commit in commits:
            tree = fold.tree(commit, index_file)
            subject = git("log", "-1", "--format=%s", commit).decode().strip()
            if tree == parent_tree:
                rows.append((commit[:8], "-", "empty", subject))
                continue
            fields = git("log", "-1", "--format=%an%x00%ae%x00%aI%x00%cn%x00%ce%x00%cI", commit)
            an, ae, ad, cn, ce, cd = fields.decode().split("\0")
            env = dict(
                os.environ,
                GIT_AUTHOR_NAME=an,
                GIT_AUTHOR_EMAIL=ae,
                GIT_AUTHOR_DATE=ad,
                GIT_COMMITTER_NAME=cn,
                GIT_COMMITTER_EMAIL=ce,
                GIT_COMMITTER_DATE=cd.strip(),
            )
            message = git("log", "-1", "--format=%B", commit)
            parent = git("commit-tree", tree, "-p", parent, data=message, env=env).decode().strip()
            parent_tree = tree
            rows.append((commit[:8], parent[:8], "", subject))
    for row in rows:
        print("\t".join(row))
    print(f"tip\t{parent}")
    if arguments.report:
        with open(arguments.report, "w", encoding="utf-8") as report:
            report.writelines("\t".join(row) + "\n" for row in rows)


if __name__ == "__main__":
    main()
