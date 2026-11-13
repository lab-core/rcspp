# fix(algorithm): tabu search with default parameters logs an error and returns no path

<!-- markdownlint-disable MD013 -->

## Summary

`AlgorithmParams::max_iterations` defaults to `MAX_INT`, and the tabu search refuses that value: it logs `max_iterations must be set to a finite value for TabuSearchAlgorithm` and returns `MaxPhases` with no solution. Nothing fails, so a caller that does not read the log gets an empty result that looks like a finished search.

## Reproduce

```python
from rcspp import presets
from rcspp.graph import AlgorithmParams, ResourceGraph

graph = ResourceGraph()
presets.add_cost_resource(graph)
graph.add_node(0, source=True)
graph.add_node(1)
graph.add_node(2, sink=True)
graph.add_arc((-1.0,), 0, 1, -1.0, [])
graph.add_arc((-1.0,), 1, 2, -1.0, [])

result = graph.solve("tabu")
print(result.status, len(result.solutions))  # AlgorithmStatus.MaxPhases 0, and the error in the log

params = AlgorithmParams()
params.max_iterations = 1000
result = graph.solve("tabu", params=params)
print(result.status, len(result.solutions))  # AlgorithmStatus.MaxPhases 1
```

The check is in `cpp/rcspp/algorithm/improving_tabu_search.hpp` (`max_iterations >= MAX_INT`); `DiversificationSearch` has the same one.

## Options

1. Give the tabu searches a finite default number of iterations when none is set (the value to choose: the column generation runs of #88 used 1000).
2. Reject the parameters: raise (C++ exception, Python `ValueError`) instead of logging and returning an empty result.

Either way, a default call should not silently return nothing. Option 2 changes what existing callers see; option 1 does not.

## Found by

The column generation pricer (#88) runs a tabu stage before an exact one. With default parameters the tabu stage searched nothing and the exact stage did all the work; #88 will require a finite `max_iterations` for a tabu stage, but `ResourceGraph.solve("tabu")` itself still behaves as above.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
