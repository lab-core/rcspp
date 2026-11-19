#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The source guard: which build rcspp comes from, and that it measures this tree."""

import sys
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


def write_cache(build: Path, source: Path) -> None:
    """A ``CMakeCache.txt`` in `build`, configured from `source`, written as CMake
    does."""
    (build / "CMakeCache.txt").write_text(
        f"# This is the CMakeCache file.\nCMAKE_HOME_DIRECTORY:INTERNAL={source.as_posix()}\n"
    )


def test_a_build_of_another_tree_is_refused(tree: Path, tmp_path_factory: pytest.TempPathFactory):
    build = make_build(tree, "build")
    other = tmp_path_factory.mktemp("other")
    write_cache(build, other)

    with pytest.raises(SourceTreeError, match="configured from .*other.*, not from this tree"):
        source_tree.choose_build(tree=tree)

    with pytest.raises(SourceTreeError, match="not from this tree"):
        source_tree.choose_build("build", tree)


def test_a_build_of_this_tree_may_live_elsewhere(
    tree: Path, tmp_path_factory: pytest.TempPathFactory
):
    outside = make_build(tmp_path_factory.mktemp("builds"), "release")
    write_cache(outside, tree)
    inside = make_build(tree, "build")
    write_cache(inside, tree)

    assert source_tree.choose_build(outside, tree) == outside.resolve()
    assert source_tree.choose_build(tree=tree) == inside.resolve()


def test_a_relative_build_is_found_from_the_tree_root(tree: Path, monkeypatch: pytest.MonkeyPatch):
    build = make_build(tree, "build")
    (tree / "elsewhere").mkdir()
    monkeypatch.chdir(tree / "elsewhere")

    assert source_tree.choose_build("build", tree) == build.resolve()


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


def test_a_module_left_in_the_copy_is_stale(tree: Path):
    # Removed from the sources, but still importable from the build's copy.
    build = make_build(tree, "build")
    (build / "python" / "rcspp" / "old.py").write_text("")

    assert source_tree.stale_python_files(build, tree) == ["old.py (not in the sources)"]


# MARK: Paths


@pytest.mark.skipif(sys.platform != "win32", reason="drive letters are Windows's")
def test_a_path_on_another_drive_is_not_inside():
    assert not source_tree.is_inside(r"D:\elsewhere\rcspp\__init__.py", Path(r"C:\tree"))
    assert source_tree.is_inside(r"c:\TREE\build\x.py", Path(r"C:\tree"))


def test_a_folder_goes_first_on_the_path_even_when_already_there(monkeypatch: pytest.MonkeyPatch):
    monkeypatch.setattr(sys, "path", ["first", "folder", "last"])
    source_tree.put_first_on_path("folder")

    assert sys.path == ["folder", "first", "last"]

    source_tree.put_first_on_path("new")

    assert sys.path == ["new", "folder", "first", "last"]


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


def test_rcspp_without_a_file_is_refused(build: Path, monkeypatch: pytest.MonkeyPatch):
    # A namespace package has no __file__: it would pass as the current folder.
    import rcspp

    monkeypatch.setattr(rcspp, "__file__", None)
    monkeypatch.chdir(build)

    with pytest.raises(SourceTreeError, match="rcspp has no file"):
        source_tree.check_rcspp(build)


def test_the_example_comes_from_this_tree(build: Path):
    try:
        source_tree.use_example()
    except SourceTreeError:
        pytest.skip("the build has no column generation example")

    import cpp_column_generation

    example = source_tree.TREE / "examples" / "column_generation" / "python"
    assert source_tree.is_inside(cpp_column_generation.__file__ or "", example)
