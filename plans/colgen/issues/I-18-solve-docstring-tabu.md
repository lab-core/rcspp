# docs(python): ResourceGraph.solve's docstring omits the tabu search

<!-- markdownlint-disable MD013 -->

## Summary

`ResourceGraph.solve` accepts `algorithm="tabu"` (`python/src/rcspp/graph.py:26`, `Algorithm.Tabu`), but its docstring lists only `Algorithm.Simple`, `Algorithm.Pushing`, `Algorithm.Pulling`, `Algorithm.Greedy`, `Algorithm.AStar` and `Algorithm.Bidirectional`, and the matching strings. A reader of the API reference does not learn that tabu is available from Python.

## Proposed change

Add `Algorithm.Tabu` and `'tabu'` to the list, with a note that the tabu search needs a finite `params.max_iterations` (see #95).

Checked on `main` @ `6367ee56`.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
