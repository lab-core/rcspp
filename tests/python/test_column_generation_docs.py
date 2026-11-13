#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The example of the column generation page (docs/advanced/column-generation.md) runs
and prints what the page says it prints.

The Python blocks of the page's section "The column generation package" run in order, in one
namespace, as a reader would paste them. A block marked ``<!-- docs test: fragment -->`` uses
names the page does not define and is not run. The block marked
``<!-- docs test: gurobipy master -->`` replaces the highspy master of the first run: the
example runs once with each master solver installed.
"""

import contextlib
import importlib.util
import io
import re
from pathlib import Path

import pytest

PAGE = Path(__file__).resolve().parents[2] / "docs" / "advanced" / "column-generation.md"
SECTION = "## The column generation package"

# What the example prints, as the page shows it in its comments.
EXPECTED = [
    "ColumnGenerationStatus.Optimal 37.5",
    "0.50 x [0, 1, 2, 4], cost 24.0",
    "0.50 x [0, 1, 3, 4], cost 25.0",
    "0.50 x [0, 2, 3, 4], cost 26.0",
    "44.0",
]

# The lines of the first run that the gurobipy block replaces.
HIGHSPY_MASTER = re.compile(
    r"^# The master:.*?^master = MasterProblem\(HighspyMasterSolver\(model\)\)\n",
    re.MULTILINE | re.DOTALL,
)

BLOCK = re.compile(r"(?:<!-- docs test: ([a-z ]+) -->\s*\n)?```python\n(.*?)```", re.DOTALL)


def page_blocks() -> list[tuple[str | None, str]]:
    """The Python blocks of the package's section, each with its mark (``None`` for
    none)."""
    text = PAGE.read_text(encoding="utf-8")
    start = text.index(SECTION)
    end = text.index("\n## ", start + len(SECTION))
    return [(match.group(1), match.group(2)) for match in BLOCK.finditer(text[start:end])]


def example(master: str) -> list[str]:
    """The blocks a reader runs, in order, with the first run's master over `master`."""
    blocks = page_blocks()
    run = [code for mark, code in blocks if mark is None]

    if master == "gurobipy":
        (gurobipy,) = [code for mark, code in blocks if mark == "gurobipy master"]
        first, substitutions = HIGHSPY_MASTER.subn(lambda _: gurobipy, run[0])
        assert substitutions == 1, "the first run's highspy master is not where the test expects"
        run[0] = first

    return run


def has_gurobi_license() -> bool:
    """Whether gurobipy is installed and Gurobi can build a model on this machine."""
    if importlib.util.find_spec("gurobipy") is None:
        return False

    import gurobipy

    try:
        gurobipy.Model().dispose()
    except gurobipy.GurobiError:
        return False

    return True


def test_the_page_marks_only_known_blocks() -> None:
    marks = {mark for mark, _ in page_blocks()}
    assert marks <= {None, "fragment", "gurobipy master"}
    assert "gurobipy master" in marks


def test_the_page_shows_what_the_test_checks() -> None:
    text = PAGE.read_text(encoding="utf-8")

    for line in EXPECTED:
        assert line in text, f"the page no longer shows {line!r}: update EXPECTED"


@pytest.mark.parametrize(
    "master",
    [
        pytest.param(
            "highspy",
            marks=pytest.mark.skipif(
                importlib.util.find_spec("highspy") is None, reason="highspy is not installed"
            ),
        ),
        pytest.param(
            "gurobipy",
            marks=pytest.mark.skipif(
                not has_gurobi_license(), reason="gurobipy not installed, or no Gurobi license"
            ),
        ),
    ],
)
def test_the_example_prints_what_the_page_shows(master: str) -> None:
    namespace: dict[str, object] = {"__name__": "docs_example"}
    output = io.StringIO()

    with contextlib.redirect_stdout(output):
        for position, code in enumerate(example(master)):
            compiled = compile(code, f"{PAGE.name}, block {position}", "exec")
            exec(compiled, namespace)  # nosec B102 - the repository's own page

    printed = output.getvalue().splitlines()

    for line in EXPECTED:
        assert any(
            line in printed_line for printed_line in printed
        ), f"{line!r} not printed; printed: {printed}"
