#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The source guard: which build rcspp comes from, and that it measures this tree."""

from pathlib import Path

import pytest
from column_generation_benchmark import source_tree
from column_generation_benchmark.source_tree import SourceTreeError

# MARK: A fake tree


def make_build(tree: Path, name: str) -> Path:
    """A build folder of `tree` with a ``_core`` for this Python and a copy of the
    sources."""
    package = tree / name / "python" / "rcspp"
    package.mkdir(parents=True)
    (package / source_tree.core_file_name()).write_bytes(b"")
    (package / "__init__.py").write_text("source = 1\n")
    return tree / name


@pytest.fixture
def tree(tmp_path: Path) -> Path:
    """A tree with ``python/src/rcspp`` and no build."""
    package = tmp_path / "python" / "src" / "rcspp"
    package.mkdir(parents=True)
    (package / "__init__.py").write_text("source = 1\n")
    return tmp_path


# MARK: Choosing the build


def test_the_only_build_is_chosen(tree: Path):
    build = make_build(tree, "build-release")
    (tree / "build-empty").mkdir()
    (tree / "other" / "python" / "rcspp").mkdir(parents=True)

    assert source_tree.find_builds(tree) == [build]
    assert source_tree.choose_build(tree=tree) == build.resolve()


def test_no_build_is_refused(tree: Path):
    with pytest.raises(SourceTreeError, match="no build of rcspp"):
        source_tree.choose_build(tree=tree)


def test_several_builds_need_a_choice(tree: Path):
    make_build(tree, "build")
    chosen = make_build(tree, "build-python")

    with pytest.raises(SourceTreeError, match="several builds"):
        source_tree.choose_build(tree=tree)

    assert source_tree.choose_build("build-python", tree) == chosen.resolve()


def test_a_chosen_folder_without_core_is_refused(tree: Path):
    (tree / "build").mkdir()

    with pytest.raises(SourceTreeError, match="has no python/rcspp"):
        source_tree.choose_build("build", tree)


# MARK: The copy of the sources


def test_a_copy_equal_to_the_sources_is_not_stale(tree: Path):
    build = make_build(tree, "build")

    assert source_tree.stale_python_files(build, tree) == []


def test_a_changed_or_missing_copy_is_stale(tree: Path):
    build = make_build(tree, "build")
    source = tree / "python" / "src" / "rcspp"
    (source / "__init__.py").write_text("source = 2\n")
    (source / "column_generation").mkdir()
    (source / "column_generation" / "loop.py").write_text("")

    assert source_tree.stale_python_files(build, tree) == [
        "__init__.py",
        "column_generation/loop.py",
    ]

    with pytest.raises(SourceTreeError, match="older copy"):
        source_tree.use_tree(build, tree)


# MARK: This tree


def test_rcspp_comes_from_the_build(build: Path):
    import rcspp
    import rcspp._core

    assert source_tree.is_inside(rcspp.__file__ or "", build)
    assert source_tree.is_inside(rcspp._core.__file__ or "", build)
    assert source_tree.is_inside(build, source_tree.TREE)


def test_rcspp_from_elsewhere_is_refused(build: Path, tmp_path: Path):
    with pytest.raises(SourceTreeError, match="not from the build"):
        source_tree.check_rcspp(tmp_path)


def test_the_example_comes_from_this_tree(build: Path):
    try:
        source_tree.use_example()
    except SourceTreeError:
        pytest.skip("the build has no column generation example")

    import cpp_column_generation

    example = source_tree.TREE / "examples" / "column_generation" / "python"
    assert source_tree.is_inside(cpp_column_generation.__file__ or "", example)
