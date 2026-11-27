#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The benchmark's tests import its shared package from the folder above, and rcspp from
a build of this tree (the ``build`` fixture), as the runners do."""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "runners"))

from column_generation_benchmark import source_tree  # noqa: E402


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--all-features-file",
        help="the benchmark file test_benchmark_all_features.py runs and checks, instead of its "
        "quick file (for example all_features.toml, run by hand)",
    )


@pytest.fixture(scope="session")
def build() -> Path:
    """The build rcspp is imported from: ``RCSPP_BENCHMARK_BUILD``, else the tree's only build.

    The test skips when the tree has no build; any other problem (several builds, a stale copy
    of the sources, rcspp imported from elsewhere) fails it.
    """
    chosen = os.environ.get("RCSPP_BENCHMARK_BUILD")

    if chosen is None and not source_tree.find_builds():
        pytest.skip("no build of rcspp in this tree")

    return source_tree.use_tree(chosen)
