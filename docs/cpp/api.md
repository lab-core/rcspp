---
title: C++ API Reference
parent: C++ Library
nav_order: 3
---

# C++ API Reference

All classes are in the `rcspp` namespace.  Include `rcspp/rcspp.hpp`.

---

## `ResourceGraph<R₁, R₂, …>`

The main entry point.  Template parameters are the resource types in the
order they are registered.

```cpp
template<typename... ResourceTypes>
class ResourceGraph : public Graph<ResourceTypeComposition<ResourceTypes...>> { … };
```

### Construction

```cpp
ResourceGraph<RealResource, IntResource> graph;

// Reserve capacity upfront (optional — avoids rehashing)
graph.reserve(n_nodes, n_arcs);
```

### Adding resources

Resources must be added in the same order as the template parameters.

```cpp
graph.add_resource<RealResource>(
    std::make_unique<AdditionExtensionFunction<RealResource>>(),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, 100.0),
    std::make_unique<ValueCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());

graph.add_resource<IntResource>(
    std::make_unique<AdditionExtensionFunction<IntResource>>(),
    std::make_unique<MinMaxFeasibilityFunction<IntResource>>(0, 10),
    std::make_unique<TrivialCostFunction<IntResource>>(),
    std::make_unique<ValueDominanceFunction<IntResource>>());
```

### Adding nodes and arcs

```cpp
// Nodes
Node<Composition>& n = graph.add_node(node_id, /*source=*/false, /*sink=*/false);

// Arcs — single resource
graph.add_arc({10.0}, origin_id, dest_id, arc_cost);

// Arcs — two resources
graph.add_arc(
    std::make_tuple(std::vector{10.0}, std::vector{3}),
    origin_id, dest_id, arc_cost,
    {Row{constraint_index, coefficient}});  // optional LP rows
```

### Solving

```cpp
// Default: SimpleDominanceAlgorithm, LabelList
SolveResult result = graph.solve();
SolveResult result = graph.solve(upper_bound, params, /*preprocess=*/true, /*cost_index=*/0);

// Explicit algorithm and label container
SolveResult result = graph.solve<GreedyAlgorithm, RealResource, LabelList>();
SolveResult result = graph.solve<SimpleDominanceAlgorithm, RealResource, LabelBuckets>(
    upper_bound, bucket_params);
```

### Graph modification

```cpp
bool removed  = graph.remove_arc(arc_id);
bool restored = graph.restore_arc(arc_id);

// Force an arc — removes all other arcs between same origin–destination pair
std::vector<size_t> removed_ids = graph.force_arc(arc_id);

// Accessors
Node<C>*  node = graph.get_node(node_id);
Arc<C>*   arc  = graph.get_arc(arc_id);

// Clone (deep copy with independent arc-removal state)
auto copy = graph.clone(/*include_rows=*/true, /*clone_removed_arcs=*/false);
```

### Column generation

```cpp
// Recompute arc reduced costs from LP dual values
// reduced_cost = arc.cost - Σ (row.coefficient * duals[row.index])
graph.update_reduced_costs(duals, /*cost_index=*/0);
```

---

## `Graph<ResourceType>` — base class

Underlying directed graph structure.  Normally you work through
`ResourceGraph`, but the base is accessible.

```cpp
size_t number_of_nodes() const;
size_t number_of_arcs() const;
const std::vector<const Node<R>*>& get_sorted_nodes() const; // topological order
const std::vector<size_t>& get_source_node_ids() const;
const std::vector<size_t>& get_sink_node_ids() const;
void sort_nodes();   // topological sort
void build_csr();    // build compressed-row adjacency (needed after arc changes for some algos)
```

---

## `Node<ResourceType>` and `Arc<ResourceType>`

### Node

```cpp
struct Node {
    size_t id;
    bool source, sink;
    size_t pos;                             // position in sorted order
    Resource<ResourceType> resource;
    std::vector<Arc<ResourceType>*> in_arcs, out_arcs;
};
```

### Arc

```cpp
struct Arc {
    size_t id;
    double cost;
    Node<ResourceType>* origin;
    Node<ResourceType>* destination;
    std::unique_ptr<Extender<ResourceType>> extender;
    std::vector<Row> rows;                  // LP column coefficients
};
```

### Row

```cpp
struct Row {
    int index;          // constraint index in the LP master
    double coefficient;
};
```

---

## `AlgorithmParams<LabelContainerType>`

Controls solver behaviour.  Pass as the second argument to `solve()`.

```cpp
AlgorithmParams<LabelList<Composition>> params;

// Termination
params.stop_after_X_solutions = 1;      // stop after finding N solutions
params.max_iterations          = 100;   // max label extension iterations
params.timeout_s               = 60.0;  // wall-clock timeout
params.should_stop             = []{ return external_flag; }; // custom callback

// Optimality
params.return_dominated_solutions = false;  // include dominated solutions in output
params.num_max_phases             = 1;      // number of DP phases (bi-directional: 2)

// Column generation
params.use_pool           = true;
params.release_after_solve = true;          // shrink label pool after solve

// Memory limits
params.max_memory_gb                  = 8.0;
params.limit_to_available_ram         = false;
params.memory_limit_fraction          = 0.9;
params.memory_pressure_fraction       = 0.8;
params.memory_pressure_max_labels_per_node = 200;
params.memory_check_interval          = 50000;

// Greedy/tabu parameters
params.num_labels_to_extend_by_node = MAX_INT;
params.tabu_tenure                  = 5;
params.tabu_random_noise            = true;
params.forbidden_tabu               = {source_id, sink_id};
params.seed                         = 42;
```

### `BucketAlgorithmParams<LabelBuckets<…>>`

Additional parameters when using `LabelBuckets`:

```cpp
BucketAlgorithmParams<LabelBuckets<Composition>> bp;
bp.range_buckets          = 100;  // number of buckets
bp.bucket_resource_index  = 0;    // resource to partition on
bp.sort_resource_index    = 1;    // resource to sort within bucket
```

---

## `SolveResult`

```cpp
struct SolveResult {
    std::vector<Solution> solutions;      // sorted best-first
    AlgorithmStatus       status;
    size_t                num_extended_labels;
    std::string           status_string() const;
};

enum class AlgorithmStatus {
    COMPLETE,        // all non-dominated paths found
    TIMEOUT,
    MAX_SOLUTIONS,
    MAX_PHASES,
    INTERRUPTED,     // SIGINT
    MEMORY_LIMIT,
};
```

---

## `Solution`

```cpp
struct Solution {
    double              cost;
    std::vector<size_t> path_node_ids;
    std::vector<size_t> path_arc_ids;
    Column              column;           // for LP master
};

struct Column {
    double            cost;
    std::vector<Row>  rows;
};
```

---

## Algorithms

All algorithms inherit from `Algorithm<ResourceType, LabelContainerType>`.

| Class | Optimal | Notes |
|---|---|---|
| `SimpleDominanceAlgorithm` | Yes | Standard label-setting; default |
| `PushingDominanceAlgorithm` | Yes | Pushes dominated labels forward |
| `PullingDominanceAlgorithm` | Yes | Bi-directional with pull phase |
| `AStarDominanceAlgorithm` | Yes | A★ heuristic for priority |
| `GreedyAlgorithm` | No | Fast; good first solution |
| `TabuSearch` | No | Tabu-arc avoidance |
| `ImprovingTabuSearch` | No | Tabu + improving-move filter |
| `DiversificationSearch` | No | Wraps another algorithm; collects diverse solutions |

### Choosing an algorithm

```cpp
// Via template parameter on ResourceGraph::solve
graph.solve<GreedyAlgorithm, RealResource, LabelList>(ub, params);

// Via explicit construction
auto algo = std::make_unique<DiversificationSearch<Composition>>(
    &resource_factory, params, std::move(inner_algo));
auto result = algo->solve(&graph, upper_bound);
```

---

## `SolutionPool` (column generation)

Thread-safe column store with activity tracking.  Used in conjunction with
`FilteredSolutionPool` for per-subproblem views.

```cpp
SolutionPool pool;

// Create a view
auto& fp = pool.new_filter();           // FilteredSolutionPool&

// Add solutions
size_t col_id = fp.add(solution);
std::vector<size_t> ids = fp.add({sol1, sol2, sol3});

// Price columns (returns those with reduced_cost < threshold)
auto priced = fp.price(duals, threshold);

// Activity
auto& act = fp.get_activity(col_id);
// act.age, act.use_count, act.priced_count, act.usage_rate()

// Remove stale columns
fp.remove_stale(max_age);
fp.global_remove_if([](size_t id, const Solution& s, const Activity& a) {
    return a.usage_rate() < 0.01;
});

// Arc-based filters
fp.remove_if_arc_present(forbidden_arc_id);
fp.global_remove_if_arc_present(arc_id);
```
