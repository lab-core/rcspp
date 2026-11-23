# feat(python): a typed exception for a refused model; the resource functions Python cannot reach

<!-- markdownlint-disable MD013 -->

## Summary

Found while listing every engine feature and where Python reaches it. Two parts: a small gap with a clear fix, and a list of C++-only resource functions, low priority, to decide whether Python should have them.

## 1. A refused model reaches Python as a plain `RuntimeError`

A backward or bidirectional solve runs the model checks and throws `ModelRefused` (`cpp/rcspp/validation/model_report.hpp:29`), which carries the list of problems (`problems()`, `:39`). No exception translator is registered, so Python gets a `RuntimeError` whose message is the joined text (`docs/advanced/algorithms.md:674-675` says so). To act on the problems, Python code has to parse the message, or call `check_model` again (`python/src/rcspp/graph.py:614`, which does return the list).

**Proposed change:** register a Python `ModelRefused`, a subclass of `RuntimeError` (existing `except RuntimeError` code keeps working), with a `problems` attribute holding the list; export it from `rcspp`; update `algorithms.md:674`.

## 2. Low priority: resource functions only in C++

| feature | C++ | Python today |
|---|---|---|
| Per-node bounds: `NodeBounds`, per-node capacity, min-max and size bounds, `presets::add_capacity_resource(..., per_node)` | `cpp/rcspp/resource/functions/node_bounds.hpp:23`, `presets.hpp:167-171` | a single value only (`python/bindings/resource.cpp:51-53`, `:94-96`, `:132-135`; `presets.py:108`). The comment at `resource.cpp:88-89` says uniform only on purpose. Per-node time windows do work, through a dict |
| `AdditionExtensionFunction`'s `min_value` floor | `addition_extension_function.hpp:45` | no argument (`resource.cpp:39`) |
| `ReachableFeasibilityFunction` (a node is entered only after the nodes listed for it, e.g. pickup before delivery) | `reachable_feasibility_function.hpp:29` | not bound |
| `TrivialExtensionFunction`, `TrivialDominanceFunction` | `trivial_extension_function.hpp:22`, `trivial_dominance_function.hpp:19` | not bound (their siblings, trivial feasibility and trivial cost, are) |

**Question:** are any of these wanted from Python? If so, the per-node forms could take a dict per node, as the time windows already do.

Not proposed: functions written in Python (a Python call per label extension), composition functions, and costs of another type than `real`.

Checked on `main` @ `ef4532d7`.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
