# docs: pages that disagree with the code, and features with no docs

<!-- markdownlint-disable MD013 -->

## Summary

A pass over every engine feature and where Python reaches it found docs that contradict the code, and C++ features the docs never mention. Each item below was checked against the code.

## Docs that contradict the code

| where | what it says | what the code does |
|---|---|---|
| `docs/advanced/algorithms.md:414-415` (presets table) | ng-path and elementary presets: *not available* in Python | both exist: `presets.add_ng_path_resource` and `presets.add_elementary_resource` (`python/src/rcspp/presets.py:138`, `:175`) |
| `docs/advanced/algorithms.md:457-461` | Python has no ng-path preset; `NgPathExtensionFunction` and `IntersectionFeasibilityFunction` are not exposed, so an ng-path model cannot be built from Python | both functions are bound (`python/bindings/resource.cpp:136-145`); `docs/python/quickstart.md` already builds ng-path models |
| `docs/advanced/algorithms.md:566-580` | a `Diversification` section with a Python example | the example runs `algorithm="greedy"`; `strategy::Diversification` is C++ only (not among Python's algorithm names) |
| `docs/python/quickstart.md:125-138` | "Multiple solutions (diversification)" | same: plain greedy, no diversification |
| `docs/index.md:103-104` | `uint` among the resource types, Diversification among the algorithms | both are C++ only; the page does not say so |
| `docs/cpp/concepts.md:199-202` | `LabelBuckets<R>`, which "requires a `BucketAlgorithmParams`" | the template takes four parameters (bucket resource, sort resource, resource composition, direction; `cpp/rcspp/label/label_buckets.hpp:63-65`); `BucketAlgorithmParams` is the Python API |
| `docs/python/api.rst` | modules `graph`, `resource`, `pricing_pool`, `logger` | `rcspp.presets` has no API page |
| `release_after_solve`'s docstring (`python/bindings/graph.cpp:227-229`, `graph.pyi:396`) | set it to false when the same algorithm is called repeatedly | from Python every `solve` builds a new algorithm, so the flag has no effect there |

## Features with no docs

- `strategy::ImprovingTabu` (`cpp/rcspp/algorithm/strategies.hpp:92`, `improving_tabu_search.hpp:44`) and its parameter `non_improving_episodes_before_growth` (`algorithm_params.hpp:138`): only a file tree in `docs/cpp/index.md:39,46` names it.
- `AdditionExtensionFunction`'s `min_value` floor (`addition_extension_function.hpp:45`).
- `ResourceGraph::sort_nodes_by_connectivity` (`resource_graph.hpp:279`).
- `UIntSetResource`, `SizeTSetResource`, `SizeTBitsetResource` (`resource_traits.hpp:32-44`): missing from the type table of `docs/cpp/concepts.md:51-58`.

## Proposed change

One docs PR: fix the eight places above, mark Diversification, `ImprovingTabu` and `uint` as C++ only where they are listed, drop or rewrite the two greedy "diversification" examples, add `rcspp.presets` to `api.rst`, and document the four undocumented features.

Checked on `main` @ `ef4532d7`.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
