"""Sphinx configuration for rcspp documentation."""

import os
import sys

# Python source path so autodoc can import rcspp.*
sys.path.insert(0, os.path.abspath("../python/src"))

# ---------------------------------------------------------------------------
# Project metadata
# ---------------------------------------------------------------------------
project = "rcspp"
copyright = "2025, Laboratory for Combinatorial Optimization in Real-time Environment (LCORE)"
author = "LCORE"
release = "latest"

# ---------------------------------------------------------------------------
# Extensions
# ---------------------------------------------------------------------------
extensions = [
    "breathe",
    "exhale",
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",
    "myst_parser",
    "sphinx.ext.viewcode",
    "sphinx_copybutton",
    "sphinx.ext.intersphinx",
]

# ---------------------------------------------------------------------------
# MyST-Parser
# ---------------------------------------------------------------------------
myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "tasklist",
    "attrs_inline",
]
myst_heading_anchors = 3

# ---------------------------------------------------------------------------
# Autodoc / Napoleon
# ---------------------------------------------------------------------------
# The compiled C extension is not available at doc-build time; mock it so
# imports of rcspp.graph, rcspp.resource etc. succeed.
autodoc_mock_imports = ["rcspp._core", "networkx", "numpy"]
autodoc_default_options = {
    "members": True,
    "undoc-members": False,
    "show-inheritance": True,
    "special-members": "__init__",
}
napoleon_google_docstring = True
napoleon_numpy_docstring = False

# ---------------------------------------------------------------------------
# Breathe (Doxygen XML → Sphinx)
# ---------------------------------------------------------------------------
breathe_projects = {"rcspp": "./_doxygen/xml"}
breathe_default_project = "rcspp"
breathe_default_members = ("members", "undoc-members")

# ---------------------------------------------------------------------------
# Exhale (auto-generates per-class API pages from Doxygen XML)
# ---------------------------------------------------------------------------
exhale_args = {
    "containmentFolder": "./cpp/api",
    "rootFileName": "library_root.rst",
    "rootFileTitle": "Full C++ API Reference",
    "doxygenStripFromPath": "../",
    "createTreeView": True,
    "exhaleExecutesDoxygen": False,
    # Don't treat file/dir nodes as orphans in the full tree
    "unabridgedOrphanKinds": {"file", "dir", "page"},
}

# ---------------------------------------------------------------------------
# HTML / Furo theme
# ---------------------------------------------------------------------------
html_theme = "furo"
html_title = "rcspp"
html_static_path = ["_static"]
html_css_files = ["custom.css"]

html_theme_options = {
    # Light mode — navy blue brand
    "light_css_variables": {
        "color-brand-primary": "#1a6b9a",
        "color-brand-content": "#1a6b9a",
        "color-admonition-background": "#eef6fc",
        "font-stack": "Inter, system-ui, -apple-system, sans-serif",
        "font-stack--monospace": "'JetBrains Mono', 'Fira Code', monospace",
    },
    # Dark mode — lighter blue
    "dark_css_variables": {
        "color-brand-primary": "#4db8e8",
        "color-brand-content": "#4db8e8",
        "color-admonition-background": "#0d2030",
    },
    "source_repository": "https://github.com/lab-core/rcspp/",
    "source_branch": "main",
    "source_directory": "docs/",
    "navigation_with_keys": True,
    "top_of_page_buttons": ["view", "edit"],
}

# ---------------------------------------------------------------------------
# General
# ---------------------------------------------------------------------------
templates_path = ["_templates"]
exclude_patterns = [
    "_build",
    "_doxygen",
    "_site",
    "Thumbs.db",
    ".DS_Store",
    "Gemfile*",
    "_config.yml",
]

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# Intersphinx: link to Python standard library docs
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable", None),
}
