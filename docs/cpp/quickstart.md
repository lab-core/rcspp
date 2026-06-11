---
title: C++ Quick Start
---

# C++ Quick Start

## Minimal example — single resource

```cpp
#include "rcspp/rcspp.hpp"
using namespace rcspp;

// Graph with one real resource (distance, max = 50)
using RG = ResourceGraph<RealResource>;

RG graph;
graph.add_resource<RealResource>(
    std::make_unique<AdditionExtensionFunction<RealResource>>(),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 50.0),
    std::make_unique<ValueCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());

graph.add_node(0, /*source=*/true);
graph.add_node(1);
graph.add_node(2);
graph.add_node(3, false, /*sink=*/true);

graph.add_arc({10.0}, 0, 1, /*cost=*/10.0);
graph.add_arc({15.0}, 1, 3, /*cost=*/15.0);
graph.add_arc({20.0}, 0, 2, /*cost=*/20.0);
graph.add_arc({10.0}, 2, 3, /*cost=*/10.0);
// path 0→2→3 has lower cost (30) but identical resource usage to 0→1→3 (25)

SolveResult result = graph.solve();
std::cout << "Cost: " << result.solutions[0].cost << "\n"; // 25.0
```

---

## Two resources — time window + capacity

```cpp
#include "rcspp/rcspp.hpp"
using namespace rcspp;

using RG = ResourceGraph<RealResource, IntResource>;

// time windows per node: {node_id: {ready, due}}
std::unordered_map<size_t, std::pair<double,double>> tw = {
    {0, {0.0, 0.0}},
    {1, {5.0, 20.0}},
    {2, {0.0, 100.0}},
};

RG graph;

// Resource 0: time (drives objective via ValueCostFunction)
graph.add_resource<RealResource>(
    std::make_unique<TimeWindowExtensionFunction<RealResource>>(tw),
    std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(tw),
    std::make_unique<ValueCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());

// Resource 1: demand (capacity ≤ 10, no cost)
graph.add_resource<IntResource>(
    std::make_unique<AdditionExtensionFunction<IntResource>>(),
    std::make_unique<MinMaxFeasibilityFunction<IntResource>>(0, 10),
    std::make_unique<TrivialCostFunction<IntResource>>(),
    std::make_unique<ValueDominanceFunction<IntResource>>());

graph.add_node(0, true);
graph.add_node(1);
graph.add_node(2, false, true);

// arc: {travel_time, demand}
graph.add_arc(std::make_tuple(std::vector{5.0}, std::vector{3}), 0, 1, 5.0);
graph.add_arc(std::make_tuple(std::vector{8.0}, std::vector{4}), 1, 2, 8.0);

auto result = graph.solve();
```

---

## Column generation loop

```cpp
#include "rcspp/rcspp.hpp"
using namespace rcspp;

using Comp = ResourceTypeComposition<RealResource, IntResource>;
using RG   = ResourceGraph<RealResource, IntResource>;

RG graph;
// … build graph with rows on arcs …

// arc with LP row: arc contributes 1 unit to constraint 5
graph.add_arc({10.0, 2}, 0, 1, 10.0, {Row{5, 1.0}});

// Solve loop
std::vector<double> duals(n_constraints, 0.0);

while (true) {
    graph.update_reduced_costs(duals);
    SolveResult result = graph.solve(/*upper_bound=*/-1e-9);

    if (result.solutions.empty()) break;          // no negative-RC column

    for (auto& sol : result.solutions) {
        master_problem.add_column(sol.column);    // hand column to LP solver
    }

    duals = master_problem.get_duals();           // solve LP, get new duals
}
```

---

## Choosing an algorithm

```cpp
// Default (optimal): SimpleDominanceAlgorithm
auto result = graph.solve();

// Greedy (fast, non-optimal)
auto result = graph.solve<GreedyAlgorithm, RealResource, LabelList>(ub, params);

// Diversification search — collect N diverse solutions
AlgorithmParams<LabelList<Comp>> params;
params.stop_after_X_solutions = 10;
params.max_iterations = 50;
auto result = graph.solve<DiversificationSearch, RealResource, LabelList>(ub, params);
```

---

## Memory-limited solve

```cpp
AlgorithmParams<LabelList<Comp>> params;
params.limit_to_available_ram         = true;   // auto-limit to system free RAM
params.memory_limit_fraction          = 0.8;    // use at most 80% of available RAM
params.memory_pressure_fraction       = 0.6;    // start pruning at 60%
params.memory_pressure_max_labels_per_node = 50; // prune to 50 labels/node under pressure
params.memory_check_interval          = 10000;  // check every 10k label extensions

auto result = graph.solve(ub, params);
if (result.status == AlgorithmStatus::MEMORY_LIMIT) {
    // solver hit the RSS limit; solutions may be incomplete
}
```
