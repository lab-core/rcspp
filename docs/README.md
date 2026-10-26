# Building the documentation locally

The documentation site is built from this folder with [Sphinx](https://www.sphinx-doc.org/), and the C++ API
pages come from [Doxygen](https://www.doxygen.nl/). GitHub Pages builds it with
[`.github/workflows/pages.yml`](../.github/workflows/pages.yml). The steps below run the same build on Windows,
Linux or macOS, so the local site looks like the published one.

## 1. Install the tools (once)

- **Python 3.11 or newer** (the workflow uses 3.13).
- **Doxygen**, for the C++ API pages:

  | System | Command |
  |---|---|
  | Windows | `winget install --id DimitriVanHeesch.Doxygen`, then open a new terminal so `doxygen` is on the `PATH` |
  | macOS | `brew install doxygen` |
  | Ubuntu, Debian | `sudo apt-get install doxygen` |
  | Fedora | `sudo dnf install doxygen` |

  Check it with `doxygen --version`.

## 2. Create a Python environment (once)

From the repository root. The `.venv-docs` folder is ignored by git.

Linux, macOS:

```sh
python3 -m venv .venv-docs
source .venv-docs/bin/activate
pip install -r docs/requirements.txt sphinx-autobuild
```

Windows (PowerShell):

```powershell
python -m venv .venv-docs
.venv-docs\Scripts\Activate.ps1
pip install -r docs/requirements.txt sphinx-autobuild
```

If PowerShell refuses to run `Activate.ps1`, allow local scripts once with
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.

In a new terminal, activate the environment again (the `source` or `Activate.ps1` line) before the steps below.

## 3. Generate the C++ API data

From the `docs` folder:

```sh
cd docs
doxygen Doxyfile
```

This writes XML to `docs/_doxygen/xml`, which Sphinx reads. Run it again after changing doc comments in
`cpp/rcspp`. Without it, Sphinx stops with a configuration error from `exhale`.

## 4. View the site

Still in the `docs` folder, either serve it with live reload:

```sh
sphinx-autobuild . _site --ignore cpp/api --open-browser
```

The site opens at <http://127.0.0.1:8000>. Saving any file under `docs/` rebuilds it and reloads the page. Stop the
server with `Ctrl+C`. (`--ignore cpp/api` is needed: the build regenerates that folder every time, which would
otherwise trigger a new build forever.)

Or build it once, as the workflow does:

```sh
sphinx-build -b html . _site --keep-going
python -m http.server --directory _site 8000
```

and open <http://127.0.0.1:8000>.

`_site`, `_doxygen` and `cpp/api` are generated, and ignored by git.

## Troubleshooting

- **The Python API pages are empty**, and the build log shows `autodoc: failed to import` with an `ImportError`
  from `rcspp._core`. The `rcspp` package loads a compiled module from a local build folder (`build/`, `out/`,
  `cmake-build-*`) when one exists for the same Python version. If that build is older than the sources, the import
  fails. Rebuild the Python bindings (see the main [README](../README.md#building-the-python-bindings)), or delete
  that build folder. Without a compiled module, the documentation build uses placeholders, as on GitHub.
- **A page or an image does not update** in the live server: some changes, such as a new page in a table of
  contents, are only picked up by a full rebuild. Stop the server, delete `_site`, and start it again.
