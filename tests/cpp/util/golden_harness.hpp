// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The golden test's instances, settings and sweep.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/backward_only_algorithm.hpp"

namespace golden_labelling {

using namespace rcspp;  // NOLINT(google-build-using-namespace)
using Composed = ResourceTypeComposition<RealResource>;
using ForwardList = LabelList<Composed>;
using BackwardList = LabelList<Composed, BackwardDirection>;
constexpr double kInf = std::numeric_limits<double>::infinity();

/// One instance: a seed, a size, and one or two sources and sinks.
struct Spec {
        uint32_t seed;
        size_t n;
        size_t terminals;
};
/// @brief The instances every sweep runs: 8- and 11-node graphs with one source and sink,
///        11-node ones with two of each, and two 16-node ones.
inline std::vector<Spec> specs() {
    std::vector<Spec> all;
    for (uint32_t seed = 1; seed <= 8; ++seed) {
        for (const size_t n : {size_t{8}, size_t{11}}) {
            all.push_back({seed, n, 1});
        }
    }
    for (uint32_t seed = 9; seed <= 12; ++seed) {
        all.push_back({seed, 11, 2});  // sources {0, 1}, sinks {9, 10}
    }
    for (uint32_t seed = 13; seed <= 14; ++seed) {
        all.push_back({seed, 16, 1});
    }
    return all;
}

/// @brief A generated instance: arcs (cost, time, load), time windows and a capacity. The first
///        @c terminals nodes are sources, the last @c terminals sinks.
struct Instance {
        size_t n = 0;
        size_t terminals = 1;
        // origin, destination, cost, time, load
        std::vector<std::tuple<size_t, size_t, double, double, double>> arcs;
        std::map<size_t, std::pair<double, double>> windows;
        double capacity = 15.0;
        [[nodiscard]] bool is_source(size_t v) const { return v < terminals; }
        [[nodiscard]] bool is_sink(size_t v) const { return v + terminals >= n; }
};

/// @brief Draws the instance for @p spec.
///
/// The engine is portable across standard libraries; distributions are not, so none is used.
/// Every value is an integer-valued double, so sums are exact on every compiler.
inline Instance generate(const Spec& spec) {
    std::mt19937 rng(spec.seed);
    auto pick = [&rng](uint32_t m) { return static_cast<uint32_t>(rng() % m); };
    Instance in;
    in.n = spec.n;
    in.terminals = spec.terminals;
    for (size_t v = 0; v < in.n; ++v) {
        if (in.is_source(v)) {
            in.windows[v] = {0.0, 0.0};
        } else if (in.is_sink(v)) {
            in.windows[v] = {0.0, 200.0};
        } else {
            const double open = pick(50);
            in.windows[v] = {open, open + 20.0 + pick(60)};
        }
    }
    for (size_t i = 0; i < in.n; ++i) {
        if (in.is_sink(i)) {
            continue;  // never out of a sink
        }
        for (size_t j = 0; j < in.n; ++j) {
            if (i == j || in.is_source(j) || pick(100) >= 30) {
                continue;  // never into a source
            }
            const double cost = static_cast<double>(pick(21)) - 12.0;
            const double time = 1.0 + pick(10);
            const double load = 1.0 + pick(5);
            in.arcs.emplace_back(i, j, cost, time, load);
        }
    }
    return in;
}

/// @brief The graph for @p in, with three resources: an additive cost, a time window and a
///        capacity.
inline std::unique_ptr<ResourceGraph<RealResource>> build(const Instance& in) {
    auto g = std::make_unique<ResourceGraph<RealResource>>();
    g->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                  std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                  std::make_unique<ValueCostFunction<RealResource>>(),
                                  std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(in.windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(in.windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    g->add_resource<RealResource>(
        std::make_unique<CapacityExtensionFunction<RealResource>>(in.capacity),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, in.capacity),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    for (size_t v = 0; v < in.n; ++v) {
        g->add_node(v, in.is_source(v), in.is_sink(v));
    }
    for (const auto& [o, d, c, t, l] : in.arcs) {
        g->add_arc<RealResource, RealResource, RealResource>({c, t, l}, o, d, c);
    }
    return g;
}

/// One setting of the params a sweep runs every search under.
struct Variant {
        const char* name;
        size_t quota = kNoLimit;
        size_t phases = 1;
        bool dominated = false;
        bool preprocess = false;
        double upper_bound = kInf;
        bool prune = false;
        size_t max_iterations = kNoLimit;
        bool join_after_early_stop = true;
};
/// @brief The settings every search is swept under.
inline std::vector<Variant> variants() {
    return {{.name = "exact"},
            {.name = "pre", .preprocess = true},
            {.name = "quota", .quota = 2},
            {.name = "phases", .quota = 2, .phases = 3},
            {.name = "dominated", .dominated = true},
            {.name = "prune", .upper_bound = 0.0, .prune = true}};
}
/// @brief @ref variants, and the early stops a bidirectional search joins after.
inline std::vector<Variant> bidirectional_variants() {
    auto v = variants();
    v.push_back({.name = "early", .max_iterations = 30});
    v.push_back({.name = "early_nojoin", .max_iterations = 30, .join_after_early_stop = false});
    return v;
}

/// @brief The params of type @p ParamsType for setting @p v.
template <typename ParamsType>
ParamsType params_for(const Variant& v) {
    ParamsType p;
    p.max_extended_labels_per_node = v.quota;
    p.max_phases = v.phases;
    // Phases and dominated solutions only take effect with a finite stop
    // (AlgorithmBaseParams::check).
    if (v.phases > 1 || v.dominated) {
        p.max_solutions = 1000;
    }
    p.return_dominated_solutions = v.dominated;
    p.prune_by_incumbent = v.prune;
    p.max_iterations = v.max_iterations;
    p.join_after_early_stop = v.join_after_early_stop;
    return p;
}

/// What a search did: the labels it extended and the solutions it returned, and for a
/// bidirectional search what its join and its diagnostics report.
struct Outcome {
        size_t extended = 0;
        size_t solutions = 0;
        long long best_milli = 0;
        unsigned long long hash = 0;
        size_t joined = 0;
        size_t bounded = 0;
        size_t status = 0;
};

/// @brief Folds the 8 bytes of @p x into the FNV-1a hash @p h.
inline unsigned long long fnv(unsigned long long h, unsigned long long x) {
    for (int i = 0; i < 8; ++i) {
        h ^= (x >> (8 * i)) & 0xffULL;
        h *= 1099511628211ULL;
    }
    return h;
}
/// @brief @p cost in thousandths, rounded, so costs compare exactly.
inline long long milli(double cost) {
    return std::llround(cost * 1000.0);
}

/// @brief Summarises @p r and its @p extended label count: solution count, best cost, and a hash
///        over every (cost, path).
///
/// Solutions are sorted by (cost, path) first: their returned order among equal costs depends on
/// the standard library (unordered_set iteration, unstable sort).
inline Outcome summarise(const SolveResult& r, size_t extended) {
    Outcome o;
    o.extended = extended;
    o.solutions = r.solutions.size();
    std::vector<const Solution*> sorted;
    for (const auto& s : r.solutions) {
        sorted.push_back(&s);
    }
    std::ranges::sort(sorted, [](const Solution* a, const Solution* b) {
        if (milli(a->cost) != milli(b->cost)) {
            return milli(a->cost) < milli(b->cost);
        }
        return a->path_arc_ids < b->path_arc_ids;
    });
    o.best_milli = sorted.empty() ? 0 : milli(sorted.front()->cost);
    o.hash = 1469598103934665603ULL;
    for (const auto* s : sorted) {
        o.hash = fnv(o.hash, static_cast<unsigned long long>(milli(s->cost)));
        for (const size_t a : s->path_arc_ids) {
            o.hash = fnv(o.hash, a);
        }
        o.hash = fnv(o.hash, ~0ULL);
    }
    return o;
}

enum class Kind {
    Simple,
    Pushing,
    Pulling,
    AStar,
    Backward,
    Bidirectional,
};
/// @brief The name of @p k in a row key.
inline const char* kind_name(Kind k) {
    switch (k) {
        case Kind::Simple:
            return "simple";
        case Kind::Pushing:
            return "pushing";
        case Kind::Pulling:
            return "pulling";
        case Kind::AStar:
            return "astar";
        case Kind::Backward:
            return "backward";
        case Kind::Bidirectional:
            return "bidirectional";
    }
    return "?";
}

/// @brief Solves @p g with @p a under @p v and summarises the result.
template <typename AlgorithmPointer>
Outcome finish(AlgorithmPointer& a, ResourceGraph<RealResource>* g, const Variant& v) {
    const auto r = g->solve(a.get(), v.upper_bound, v.preprocess);
    return summarise(r, a->get_number_of_extended_labels());
}

/// @brief A bidirectional search by @p Strategy, clocked on time with `H` inside the paths'
///        durations, so the join has pairs to test.
template <template <typename, typename> class Strategy>
Outcome bidirectional_outcome(AlgorithmParams<ForwardList> p, const Variant& v,
                              ResourceGraph<RealResource>* g) {
    p.critical_resource_index = 1;
    p.half_way_point = 25.0;
    p.direction = SearchDirection::Bidirectional;
    auto a = g->create_algorithm<Strategy>(p);
    const auto r = g->solve(a.get(), v.upper_bound, v.preprocess);
    Outcome o = summarise(r, a->get_number_of_extended_labels());
    o.joined = r.bidirectional->joined_paths;
    o.bounded = r.bidirectional->half_way_used ? 1 : 0;
    o.status = static_cast<size_t>(r.status);
    return o;
}

/// @brief Runs search @p k under setting @p v on @p g.
inline Outcome run(Kind k, const Variant& v, ResourceGraph<RealResource>* g) {
    auto p = params_for<AlgorithmParams<ForwardList>>(v);
    switch (k) {
        case Kind::Simple: {
            auto a = g->create_algorithm<SimpleDominanceAlgorithm>(p);
            return finish(a, g, v);
        }
        case Kind::Pushing: {
            auto a = g->create_algorithm<PushingDominanceAlgorithm>(p);
            return finish(a, g, v);
        }
        case Kind::Pulling: {
            auto a = g->create_algorithm<PullingDominanceAlgorithm>(p);
            return finish(a, g, v);
        }
        case Kind::AStar: {
            auto a = g->create_algorithm<AStarDominanceAlgorithm>(p);
            return finish(a, g, v);
        }
        case Kind::Backward: {
            auto a = test_util::make_backward_only(g, params_for<AlgorithmParams<BackwardList>>(v));
            return finish(a, g, v);
        }
        case Kind::Bidirectional:
            return bidirectional_outcome<SimpleDominanceAlgorithm>(p, v, g);
    }
    return {};
}

/// @brief A recorded outcome, keyed by seed, size, terminals, setting and search.
struct Row {
        const char* key;
        Outcome outcome;
};

/// @brief Whether RCSPP_GOLDEN_CAPTURE is set: print rows instead of comparing.
inline bool capturing() {
    return std::getenv("RCSPP_GOLDEN_CAPTURE") != nullptr;
}

/// @brief Runs every search in @p kinds under every setting in @p settings on every instance, and
///        compares each outcome with its row in @p golden (or prints the rows when capturing).
///
/// @tparam KindType The enumeration naming the searches.
/// @param run_kind  Callable `(KindType, const Variant&, ResourceGraph<RealResource>*) ->
///                  Outcome`.
/// @param name_of   Callable `(KindType) -> const char*`, the search's name in a row key.
template <typename KindType, typename RunnerType, typename NamerType>
void sweep_with(const std::vector<KindType>& kinds, const std::vector<Variant>& settings,
                const std::vector<Row>& golden, const RunnerType& run_kind,
                const NamerType& name_of) {
    size_t compared = 0;
    for (const Spec& spec : specs()) {
        const Instance in = generate(spec);
        for (const auto& v : settings) {
            for (const KindType k : kinds) {
                auto g = build(in);  // a fresh graph: feasibility preprocessing is persistent
                const Outcome o = run_kind(k, v, g.get());
                const std::string key =
                    "s" + std::to_string(spec.seed) + "_n" + std::to_string(spec.n) + "_t" +
                    std::to_string(spec.terminals) + "_" + v.name + "_" + name_of(k);
                if (capturing()) {
                    std::printf("GOLDEN {\"%s\", {%zuu, %zuu, %lldLL, 0x%016llxULL",
                                key.c_str(),
                                o.extended,
                                o.solutions,
                                o.best_milli,
                                o.hash);
                    std::printf(", %zuu, %zuu, %zuu", o.joined, o.bounded, o.status);
                    std::printf("}},\n");
                    continue;
                }
                const Row* row = nullptr;
                for (const auto& r : golden) {
                    if (key == r.key) {
                        row = &r;
                        break;
                    }
                }
                ASSERT_NE(row, nullptr) << key << " has no golden row";
                const Outcome& e = row->outcome;
                EXPECT_EQ(o.extended, e.extended) << key;
                EXPECT_EQ(o.solutions, e.solutions) << key;
                EXPECT_EQ(o.best_milli, e.best_milli) << key;
                EXPECT_EQ(o.hash, e.hash) << key;
                EXPECT_EQ(o.joined, e.joined) << key;
                EXPECT_EQ(o.bounded, e.bounded) << key;
                EXPECT_EQ(o.status, e.status) << key;
                ++compared;
            }
        }
    }
    if (!capturing()) {
        EXPECT_EQ(compared, golden.size());
    }
}

/// @brief @ref sweep_with over the searches of @ref Kind.
inline void sweep(const std::vector<Kind>& kinds, const std::vector<Variant>& settings,
                  const std::vector<Row>& golden) {
    sweep_with(kinds, settings, golden, run, kind_name);
}

}  // namespace golden_labelling
