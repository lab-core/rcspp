# docs(cpp): the "Choosing an algorithm" example of the C++ quickstart does not compile

<!-- markdownlint-disable MD013 -->

## Summary

The "Choosing an algorithm" block of `docs/cpp/quickstart.md` passes the class template `LabelList` where `ResourceGraph::solve` expects a type:

```cpp
auto result = graph.solve<GreedyAlgorithm, RealResource, LabelList>(ub, params);
auto result = graph.solve<DiversificationSearch, RealResource, LabelList>(ub, params);
```

The third template parameter of `solve` is `LabelContainerType`, a type such as `LabelList<ResourceTypeComposition<RealResource, IntResource>>` (`cpp/rcspp/resource/resource_graph.hpp`, `solve`). `LabelList` alone is a template (`cpp/rcspp/algorithm/label_buckets.hpp`), so both lines fail to compile.

The same block also uses `params` before declaring it, and declares `auto result` three times in one scope, so it cannot be pasted as it is.

## Proposed change

- Leave out the label container when it is the default: `graph.solve<GreedyAlgorithm>(ub, params)`, or spell it out as `LabelList<Comp>`, with `Comp` defined in that block (today it is defined only in the column generation section above).
- Declare `params` before its first use, and give each example its own scope or result name.
- Compile the snippet once (for instance as a scratch test) before closing.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
