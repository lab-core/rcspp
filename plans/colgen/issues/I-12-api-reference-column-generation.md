# docs: add rcspp.column_generation to the Python API reference

<!-- markdownlint-disable MD013 -->

The Python API reference (`docs/python/api.rst`) documents `rcspp.graph`, `rcspp.resource`, `rcspp.pricing_pool` and `rcspp.logger`, but not `rcspp.column_generation` (#88), the largest Python package. Its classes are described only on the column generation page and in their docstrings.

## Why it is missing

Sphinx cannot import the package with the mocks in `docs/conf.py` (`autodoc_mock_imports = ["rcspp._core", "networkx", "numpy"]`):

- with numpy mocked, `rcspp/column_generation/types.py` fails while building its numpy type aliases at import time;
- with numpy real, the mocked `rcspp._core` fails in `graph_pricer.py`, where the annotation `str | Algorithm` is evaluated when the class is created.

## Proposed change

1. Add `from __future__ import annotations` to the modules of `rcspp.column_generation`, so that annotations are no longer evaluated at import time.
2. Remove `numpy` from `autodoc_mock_imports` and add it to `docs/requirements.txt` (numpy is already a dependency of rcspp).
3. Add an `rcspp.column_generation` section to `docs/python/api.rst` (`automodule` with `:members:` and `:imported-members:`).

Tried locally with the GitHub Pages build (Doxygen 1.9.8, then `sphinx-build -b html . _site --keep-going`): the reference documents 42 classes and functions of the package, with no import failure.

## To check

- The Python tests and pyright (strict) pass with the future import in every module.
- The new section renders the docstrings well (Google style, through napoleon).

This depends on #88, so it comes after the column generation stack is merged.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
