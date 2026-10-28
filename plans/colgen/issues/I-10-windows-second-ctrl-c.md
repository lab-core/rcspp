# fix(python): on Windows, the second Ctrl-C kills Python instead of raising KeyboardInterrupt

<!-- markdownlint-disable MD013 -->

## Summary

On Windows, the first Ctrl-C during a `solve` stops the search and raises `KeyboardInterrupt`, as intended. A second Ctrl-C later in the same process ends Python at once with exit code 3, as if no handler were installed.

The cause is in `python/bindings/graph.cpp`. On Windows, `init_sigint_handler` installs `py_sigint_handler` with `signal()`. The Microsoft C runtime resets a signal's handler to `SIG_DFL` before calling it, so the handler runs once. The next SIGINT gets the default action, which terminates the process. On POSIX the handler is installed with `sigaction` and no `SA_RESETHAND`, so it stays.

## How to reproduce

From Python on Windows, run a long solve, press Ctrl-C (`KeyboardInterrupt`), run another long solve and press Ctrl-C again: the process should exit with code 3.

Found while measuring how column generation runs react to Ctrl-C, on a debug build: SIGINT raised twice in one process through the C runtime the extension links (`raise(SIGINT)` from `ucrtbased.dll`), each during a long search. The first raised `KeyboardInterrupt` within milliseconds; the second ended the process with exit code 3. Not yet reproduced with a keyboard and a release build, where the C runtime's documented behavior gives the same result.

## Proposed change

- In `py_sigint_handler`, on Windows only, install the handler again first: `signal(SIGINT, py_sigint_handler);`.
- Add a test that raises SIGINT twice in one process, each during a long solve, and expects `KeyboardInterrupt` both times. On Windows it must raise the signal through the C runtime the extension links (the debug runtime in a debug build), not through `signal.raise_signal`, which uses Python's runtime and so reaches Python's handler instead.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
