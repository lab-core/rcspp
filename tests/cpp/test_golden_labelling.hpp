// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.
//
// Golden runs: on generated instances, each search must extend exactly the labels and return
// exactly the solutions it did when the rows below were recorded. Off unless RCSPP_GOLDEN=1;
// RCSPP_GOLDEN_CAPTURE=1 prints rows instead of comparing.
#pragma once

#define GOLDEN_LEVEL 1  // 0: G0 (#32); 1: + backward (#33 on); 2: + bidirectional (#35 on); 3: #43
#define GOLDEN_API 2    // 0: captures (old API); 1: rebuilt #33; 2: from the direction PR on

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
#if GOLDEN_LEVEL >= 1 && GOLDEN_API <= 1
#include "util/backward_only_algorithm.hpp"
#endif

namespace golden_labelling {

using namespace rcspp;  // NOLINT(google-build-using-namespace)
using Composed = ResourceTypeComposition<RealResource>;
using ForwardList = LabelList<Composed>;
#if GOLDEN_LEVEL >= 1
using BackwardList = LabelList<Composed, BackwardDirection>;  // directions arrive with #33
#endif
constexpr double kInf = std::numeric_limits<double>::infinity();

/// One instance: a seed, a size, and one or two sources and sinks.
struct Spec {
        uint32_t seed;
        size_t n;
        size_t terminals;
};
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

struct Variant {
        const char* name;
        size_t quota = MAX_INT;
        size_t phases = 1;
        bool dominated = false;
        bool preprocess = false;
        double upper_bound = kInf;
        bool prune = false;
        size_t max_iterations = MAX_INT;      // level 3
        size_t max_join_pairs = MAX_INT;      // level 3
        size_t join_column_budget = MAX_INT;  // level 3
        bool join_after_early_stop = true;    // level 3
};
inline std::vector<Variant> variants() {
    std::vector<Variant> v{{.name = "exact"},
                           {.name = "pre", .preprocess = true},
                           {.name = "quota", .quota = 2},
                           {.name = "phases", .quota = 2, .phases = 3},
                           {.name = "dominated", .dominated = true},
                           {.name = "prune", .upper_bound = 0.0, .prune = true}};
#if GOLDEN_LEVEL >= 3
    v.push_back({.name = "pairs", .max_join_pairs = 3});
    v.push_back({.name = "columns", .join_column_budget = 1});
    v.push_back({.name = "early", .max_iterations = 30});
    v.push_back({.name = "early_nojoin", .max_iterations = 30, .join_after_early_stop = false});
#endif
    return v;
}

template <typename Params>
Params params_for(const Variant& v) {
    Params p;
    p.num_labels_to_extend_by_node = v.quota;
    p.num_max_phases = v.phases;
    // Phases and dominated solutions only take effect with a finite stop
    // (AlgorithmBaseParams::check).
    if (v.phases > 1 || v.dominated) {
        p.stop_after_X_solutions = 1000;
    }
    p.return_dominated_solutions = v.dominated;
    p.prune_based_on_upper_bound_ = v.prune;
#if GOLDEN_LEVEL >= 3
    p.max_iterations = v.max_iterations;
    p.max_join_pairs = v.max_join_pairs;
    p.join_column_budget = v.join_column_budget;
    p.join_after_early_stop = v.join_after_early_stop;
#endif
    return p;
}

struct Outcome {
        size_t extended = 0;
        size_t solutions = 0;
        long long best_milli = 0;
        unsigned long long hash = 0;
        size_t joined = 0;
        size_t forward_labels = 0, backward_labels = 0, dominance_checks = 0;  // level 3
        size_t join_pairs_tested = 0, join_truncated = 0, bounded = 0, status = 0;
};

inline unsigned long long fnv(unsigned long long h, unsigned long long x) {
    for (int i = 0; i < 8; ++i) {
        h ^= (x >> (8 * i)) & 0xffULL;
        h *= 1099511628211ULL;
    }
    return h;
}
inline long long milli(double cost) {
    return std::llround(cost * 1000.0);
}

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

enum class Kind { Simple, Pushing, Pulling, AStar, Backward, Bidirectional };
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

template <typename Algo>
Outcome finish(Algo& a, ResourceGraph<RealResource>* g, const Variant& v) {
    const auto r = g->solve(a.get(), v.upper_bound, v.preprocess);
    return summarise(r, a->get_number_of_extended_labels());
}

// ── The adapter: the only part that changes between API versions ─────────────────────────────────
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
            auto a = g->create_algorithm<AStarAlgoBound<RealResource>::Algo>(p);
            return finish(a, g, v);
        }
        case Kind::Backward: {
#if GOLDEN_LEVEL >= 1
            // Backward rows compare `extended` and `best_milli` only: before #33 a backward
            // solve() returned no solutions, so the count and the hash are recorded as 0.
            Outcome o;
#if GOLDEN_API <= 1
            auto pb = params_for<AlgorithmParams<BackwardList>>(v);
            pb.release_after_solve = false;
            auto a = g->create_algorithm<test_util::BackwardOnlyAlgorithm, BackwardList>(pb);
            const auto r = g->solve(a.get(), v.upper_bound, v.preprocess);
#if GOLDEN_API == 0
            (void)r;
            const double best = a->best_terminal_cost();
#else
            const double best = r.solutions.empty() ? kInf : r.solutions.front().cost;
#endif
#else
            p.direction = SearchDirection::Backward;
            p.release_after_solve = false;
            auto a = g->create_algorithm<SimpleDominanceAlgorithm>(p);
            const auto r = g->solve(a.get(), v.upper_bound, v.preprocess);
            const double best = r.solutions.empty() ? kInf : r.solutions.front().cost;
#endif
            o.extended = a->get_number_of_extended_labels();
            o.best_milli = std::isinf(best) ? 0 : milli(best);
            return o;
#else
            break;
#endif
        }
        case Kind::Bidirectional: {
#if GOLDEN_LEVEL >= 2
            p.critical_resource_index = 1;
            p.half_way_point = 25.0;  // inside the paths' durations, so the join has pairs to test
#if GOLDEN_API == 0
            auto a = g->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(p);
#else
            p.direction = SearchDirection::Bidirectional;
            auto a = g->create_algorithm<SimpleDominanceAlgorithm>(p);
#endif
            const auto r = g->solve(a.get(), v.upper_bound, v.preprocess);
            Outcome o = summarise(r, a->get_number_of_extended_labels());
            o.joined = r.number_of_joined_paths;
#if GOLDEN_LEVEL >= 3
            o.forward_labels = r.forward_labels;
            o.backward_labels = r.backward_labels;
            o.dominance_checks = r.dominance_checks;
            o.join_pairs_tested = r.join_pairs_tested;
            o.join_truncated = r.join_truncated ? 1 : 0;
            o.bounded = r.bounded_by_half_way ? 1 : 0;
            o.status = static_cast<size_t>(r.status);
#endif
            return o;
#else
            break;
#endif
        }
    }
    return {};
}

struct Row {
        const char* key;
        Outcome outcome;
};

inline const std::vector<Row>& golden_g1() {  // forward and backward (from #33)
    static const std::vector<Row> rows{
        // clang-format off
        {"s1_n8_t1_exact_simple", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_exact_pushing", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_exact_pulling", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_exact_astar", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_exact_backward", {15u, 0u, -27000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_pre_simple", {13u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_pre_pushing", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_pre_pulling", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_pre_astar", {13u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_pre_backward", {12u, 0u, -27000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_quota_simple", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_quota_pushing", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_quota_pulling", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_quota_astar", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_quota_backward", {15u, 0u, -27000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_phases_simple", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_phases_pushing", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_phases_pulling", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_phases_astar", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_phases_backward", {15u, 0u, -27000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_dominated_simple", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_dominated_pushing", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_dominated_pulling", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_dominated_astar", {15u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_dominated_backward", {15u, 0u, -27000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_prune_pulling", {14u, 2u, -27000LL, 0x294d0cf11ac72771ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_exact_simple", {198u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_exact_pushing", {232u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_exact_pulling", {232u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_exact_astar", {240u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_exact_backward", {184u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_pre_simple", {179u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_pre_pushing", {179u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_pre_pulling", {179u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_pre_astar", {218u, 9u, -25000LL, 0x64536858836ca57fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_pre_backward", {163u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_quota_simple", {55u, 1u, -9000LL, 0x4ca20e1bb6f584ddULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_quota_pushing", {112u, 6u, -25000LL, 0x05e81eeb9d01446dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_quota_pulling", {112u, 6u, -25000LL, 0x05e81eeb9d01446dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_quota_astar", {60u, 4u, -25000LL, 0xe2b814672eacf330ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_quota_backward", {56u, 0u, -12000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_phases_simple", {139u, 5u, -25000LL, 0x8b47eee5f713dd49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_phases_pushing", {236u, 10u, -25000LL, 0x2d5f057de5ab1423ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_phases_pulling", {248u, 10u, -25000LL, 0x2d5f057de5ab1423ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_phases_astar", {134u, 6u, -25000LL, 0x2633d6a989e6e337ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_phases_backward", {147u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_dominated_simple", {198u, 10u, -25000LL, 0xb1fa7e25e121ac1aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_dominated_pushing", {232u, 10u, -25000LL, 0xb1fa7e25e121ac1aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_dominated_pulling", {232u, 10u, -25000LL, 0xb1fa7e25e121ac1aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_dominated_astar", {240u, 10u, -25000LL, 0x2d5f057de5ab1423ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_dominated_backward", {184u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_prune_pulling", {42u, 3u, -24000LL, 0xbd80b27ab87df15dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s1_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_exact_simple", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_exact_pushing", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_exact_pulling", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_exact_astar", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_exact_backward", {14u, 0u, 3000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_pre_simple", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_pre_pushing", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_pre_pulling", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_pre_astar", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_pre_backward", {14u, 0u, 3000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_quota_simple", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_quota_pushing", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_quota_pulling", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_quota_astar", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_quota_backward", {14u, 0u, 3000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_phases_simple", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_phases_pushing", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_phases_pulling", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_phases_astar", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_phases_backward", {15u, 0u, 3000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_dominated_simple", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_dominated_pushing", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_dominated_pulling", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_dominated_astar", {1u, 1u, 3000LL, 0x161452c261f33846ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_dominated_backward", {14u, 0u, 3000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_prune_pulling", {1u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_exact_simple", {144u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_exact_pushing", {144u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_exact_pulling", {144u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_exact_astar", {176u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_exact_backward", {15u, 0u, 15000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_pre_simple", {128u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_pre_pushing", {128u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_pre_pulling", {128u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_pre_astar", {159u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_pre_backward", {10u, 0u, 15000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_quota_simple", {40u, 1u, 16000LL, 0x2b262cdf1d46649aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_quota_pushing", {115u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_quota_pulling", {115u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_quota_astar", {43u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_quota_backward", {15u, 0u, 15000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_phases_simple", {97u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_phases_pushing", {140u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_phases_pulling", {154u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_phases_astar", {108u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_phases_backward", {17u, 0u, 15000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_dominated_simple", {144u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_dominated_pushing", {144u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_dominated_pulling", {144u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_dominated_astar", {176u, 2u, 15000LL, 0xa7891ecbfe2a55eeULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_dominated_backward", {15u, 0u, 15000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_prune_pulling", {4u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s2_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_exact_simple", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_exact_pushing", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_exact_pulling", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_exact_astar", {29u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_exact_backward", {26u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_pre_simple", {23u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_pre_pushing", {23u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_pre_pulling", {23u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_pre_astar", {23u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_pre_backward", {22u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_quota_simple", {26u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_quota_pushing", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_quota_pulling", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_quota_astar", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_quota_backward", {26u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_phases_simple", {29u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_phases_pushing", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_phases_pulling", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_phases_astar", {31u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_phases_backward", {26u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_dominated_simple", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_dominated_pushing", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_dominated_pulling", {28u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_dominated_astar", {29u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_dominated_backward", {26u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_prune_pulling", {8u, 1u, -13000LL, 0x4b1c53f478b3d7bdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_exact_simple", {121u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_exact_pushing", {103u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_exact_pulling", {103u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_exact_astar", {103u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_exact_backward", {122u, 0u, -23000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_pre_simple", {115u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_pre_pushing", {117u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_pre_pulling", {117u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_pre_astar", {99u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_pre_backward", {115u, 0u, -23000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_quota_simple", {51u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_quota_pushing", {64u, 2u, -23000LL, 0x38a5bda264be4397ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_quota_pulling", {64u, 2u, -23000LL, 0x38a5bda264be4397ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_quota_astar", {57u, 3u, -23000LL, 0x7277b0614129e13fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_quota_backward", {63u, 0u, -23000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_phases_simple", {126u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_phases_pushing", {103u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_phases_pulling", {106u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_phases_astar", {109u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_phases_backward", {129u, 0u, -23000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_dominated_simple", {121u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_dominated_pushing", {103u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_dominated_pulling", {103u, 4u, -23000LL, 0x66384dfd43515437ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_dominated_astar", {103u, 5u, -23000LL, 0x449c9360864fcc56ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_dominated_backward", {122u, 0u, -23000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_prune_pulling", {14u, 1u, -23000LL, 0xdfad01649b03773cULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s3_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_exact_simple", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_exact_pushing", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_exact_pulling", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_exact_astar", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_exact_backward", {8u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_pre_simple", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_pre_pushing", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_pre_pulling", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_pre_astar", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_pre_backward", {6u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_quota_simple", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_quota_pushing", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_quota_pulling", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_quota_astar", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_quota_backward", {8u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_phases_simple", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_phases_pushing", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_phases_pulling", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_phases_astar", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_phases_backward", {8u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_dominated_simple", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_dominated_pushing", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_dominated_pulling", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_dominated_astar", {7u, 2u, -13000LL, 0xd1ee824904a9941aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_dominated_backward", {8u, 0u, -13000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_prune_pulling", {7u, 1u, -13000LL, 0x1068778935bd2cd0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_exact_simple", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_exact_pushing", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_exact_pulling", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_exact_astar", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_exact_backward", {72u, 0u, -8000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_pre_simple", {72u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_pre_pushing", {72u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_pre_pulling", {72u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_pre_astar", {72u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_pre_backward", {58u, 0u, -8000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_quota_simple", {47u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_quota_pushing", {81u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_quota_pulling", {81u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_quota_astar", {45u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_quota_backward", {43u, 0u, -8000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_phases_simple", {85u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_phases_pushing", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_phases_pulling", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_phases_astar", {88u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_phases_backward", {77u, 0u, -8000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_dominated_simple", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_dominated_pushing", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_dominated_pulling", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_dominated_astar", {91u, 3u, -8000LL, 0x9ade3884b8e2faeaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_dominated_backward", {72u, 0u, -8000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_prune_pulling", {57u, 1u, -8000LL, 0xb9fe074c449d732fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s4_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_exact_simple", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_exact_pushing", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_exact_pulling", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_exact_astar", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_exact_backward", {11u, 0u, -22000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_pre_simple", {10u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_pre_pushing", {10u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_pre_pulling", {10u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_pre_astar", {10u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_pre_backward", {8u, 0u, -22000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_quota_simple", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_quota_pushing", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_quota_pulling", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_quota_astar", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_quota_backward", {11u, 0u, -22000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_phases_simple", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_phases_pushing", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_phases_pulling", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_phases_astar", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_phases_backward", {11u, 0u, -22000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_dominated_simple", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_dominated_pushing", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_dominated_pulling", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_dominated_astar", {11u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_dominated_backward", {11u, 0u, -22000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_prune_pulling", {10u, 3u, -22000LL, 0x5da3f98e0d6ba45fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_exact_simple", {93u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_exact_pushing", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_exact_pulling", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_exact_astar", {93u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_exact_backward", {36u, 0u, -12000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_pre_simple", {83u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_pre_pushing", {83u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_pre_pulling", {83u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_pre_astar", {83u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_pre_backward", {32u, 0u, -12000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_quota_simple", {43u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_quota_pushing", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_quota_pulling", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_quota_astar", {50u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_quota_backward", {32u, 0u, -12000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_phases_simple", {93u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_phases_pushing", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_phases_pulling", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_phases_astar", {103u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_phases_backward", {36u, 0u, -12000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_dominated_simple", {93u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_dominated_pushing", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_dominated_pulling", {97u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_dominated_astar", {93u, 2u, -12000LL, 0xb93d69275e2f6f42ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_dominated_backward", {36u, 0u, -12000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_prune_pulling", {3u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s5_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_exact_simple", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_exact_pushing", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_exact_pulling", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_exact_astar", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_exact_backward", {45u, 0u, -39000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_pre_simple", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_pre_pushing", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_pre_pulling", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_pre_astar", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_pre_backward", {45u, 0u, -39000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_quota_simple", {26u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_quota_pushing", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_quota_pulling", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_quota_astar", {21u, 4u, -37000LL, 0x51b475ea1bac6977ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_quota_backward", {27u, 0u, -14000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_phases_simple", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_phases_pushing", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_phases_pulling", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_phases_astar", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_phases_backward", {48u, 0u, -39000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_dominated_simple", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_dominated_pushing", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_dominated_pulling", {30u, 5u, -39000LL, 0x7fc63171a5612e92ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_dominated_astar", {30u, 6u, -39000LL, 0x624d7c0bf3c33b9aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_dominated_backward", {45u, 0u, -39000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_prune_pulling", {18u, 4u, -37000LL, 0x51b475ea1bac6977ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_exact_simple", {75u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_exact_pushing", {77u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_exact_pulling", {77u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_exact_astar", {75u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_exact_backward", {2u, 0u, 5000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_pre_simple", {67u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_pre_pushing", {67u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_pre_pulling", {67u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_pre_astar", {67u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_pre_backward", {1u, 0u, 5000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_quota_simple", {41u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_quota_pushing", {66u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_quota_pulling", {66u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_quota_astar", {41u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_quota_backward", {2u, 0u, 5000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_phases_simple", {74u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_phases_pushing", {76u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_phases_pulling", {80u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_phases_astar", {78u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_phases_backward", {2u, 0u, 5000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_dominated_simple", {75u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_dominated_pushing", {77u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_dominated_pulling", {77u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_dominated_astar", {75u, 1u, 5000LL, 0x7d259b7a076f694dULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_dominated_backward", {2u, 0u, 5000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_prune_pulling", {17u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s6_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_exact_simple", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_exact_pushing", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_exact_pulling", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_exact_astar", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_exact_backward", {13u, 0u, -10000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_pre_simple", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_pre_pushing", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_pre_pulling", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_pre_astar", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_pre_backward", {13u, 0u, -10000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_quota_simple", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_quota_pushing", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_quota_pulling", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_quota_astar", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_quota_backward", {13u, 0u, -10000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_phases_simple", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_phases_pushing", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_phases_pulling", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_phases_astar", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_phases_backward", {13u, 0u, -10000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_dominated_simple", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_dominated_pushing", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_dominated_pulling", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_dominated_astar", {18u, 2u, -10000LL, 0x5eafa7048f0dca99ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_dominated_backward", {13u, 0u, -10000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_prune_pulling", {3u, 1u, -10000LL, 0x11b56bf6e57032d7ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_exact_simple", {46u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_exact_pushing", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_exact_pulling", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_exact_astar", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_exact_backward", {75u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_pre_simple", {46u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_pre_pushing", {53u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_pre_pulling", {53u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_pre_astar", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_pre_backward", {64u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_quota_simple", {32u, 3u, -22000LL, 0x592269cffa351647ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_quota_pushing", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_quota_pulling", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_quota_astar", {20u, 3u, -25000LL, 0x11f8f47a35cf8689ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_quota_backward", {39u, 0u, -22000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_phases_simple", {47u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_phases_pushing", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_phases_pulling", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_phases_astar", {53u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_phases_backward", {82u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_dominated_simple", {46u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_dominated_pushing", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_dominated_pulling", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_dominated_astar", {52u, 5u, -25000LL, 0x287dea0a61a3be49ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_dominated_backward", {75u, 0u, -25000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_prune_pulling", {9u, 2u, -22000LL, 0x932b95a6d1571386ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s7_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_exact_simple", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_exact_pushing", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_exact_pulling", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_exact_astar", {24u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_exact_backward", {21u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_pre_simple", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_pre_pushing", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_pre_pulling", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_pre_astar", {24u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_pre_backward", {21u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_quota_simple", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_quota_pushing", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_quota_pulling", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_quota_astar", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_quota_backward", {19u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_phases_simple", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_phases_pushing", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_phases_pulling", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_phases_astar", {24u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_phases_backward", {21u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_dominated_simple", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_dominated_pushing", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_dominated_pulling", {22u, 4u, -20000LL, 0x1f5e0381bc4b4dfaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_dominated_astar", {24u, 5u, -20000LL, 0x6dbe89221a43a6f2ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_dominated_backward", {21u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_prune_pulling", {9u, 1u, -20000LL, 0xc43aada31e7f297eULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n8_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_exact_simple", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_exact_pushing", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_exact_pulling", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_exact_astar", {47u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_exact_backward", {39u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_pre_simple", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_pre_pushing", {43u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_pre_pulling", {43u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_pre_astar", {47u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_pre_backward", {35u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_quota_simple", {34u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_quota_pushing", {39u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_quota_pulling", {39u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_quota_astar", {38u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_quota_backward", {34u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_phases_simple", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_phases_pushing", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_phases_pulling", {41u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_phases_astar", {47u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_phases_backward", {39u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_dominated_simple", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_dominated_pushing", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_dominated_pulling", {40u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_dominated_astar", {47u, 2u, -20000LL, 0xd9635c6a791b0347ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_dominated_backward", {39u, 0u, -20000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_prune_pulling", {2u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s8_n11_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_exact_simple", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_exact_pushing", {55u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_exact_pulling", {55u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_exact_astar", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_exact_backward", {26u, 0u, -17000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_pre_simple", {49u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_pre_pushing", {50u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_pre_pulling", {50u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_pre_astar", {49u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_pre_backward", {25u, 0u, -17000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_quota_simple", {32u, 4u, -14000LL, 0xfc4c9d765d6764c9ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_quota_pushing", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_quota_pulling", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_quota_astar", {31u, 4u, -17000LL, 0xfb41366d569b4540ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_quota_backward", {26u, 0u, -17000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_phases_simple", {58u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_phases_pushing", {55u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_phases_pulling", {55u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_phases_astar", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_phases_backward", {29u, 0u, -17000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_dominated_simple", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_dominated_pushing", {55u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_dominated_pulling", {55u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_dominated_astar", {54u, 6u, -17000LL, 0x9996c7be97eaac38ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_dominated_backward", {26u, 0u, -17000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_prune_pulling", {19u, 1u, -12000LL, 0x03320db12ff9171eULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s9_n11_t2_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_exact_simple", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_exact_pushing", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_exact_pulling", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_exact_astar", {56u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_exact_backward", {46u, 0u, -18000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_pre_simple", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_pre_pushing", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_pre_pulling", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_pre_astar", {56u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_pre_backward", {46u, 0u, -18000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_quota_simple", {35u, 6u, -13000LL, 0x6749308c581cc533ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_quota_pushing", {45u, 7u, -18000LL, 0xdb4ccf368de27bc7ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_quota_pulling", {45u, 7u, -18000LL, 0xdb4ccf368de27bc7ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_quota_astar", {38u, 4u, -18000LL, 0x0840cec299a5615cULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_quota_backward", {44u, 0u, -18000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_phases_simple", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_phases_pushing", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_phases_pulling", {53u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_phases_astar", {56u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_phases_backward", {46u, 0u, -18000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_dominated_simple", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_dominated_pushing", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_dominated_pulling", {50u, 8u, -18000LL, 0x79a42ff058df1249ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_dominated_astar", {56u, 10u, -18000LL, 0xf1fe9187541e5aa0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_dominated_backward", {46u, 0u, -18000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_prune_pulling", {10u, 1u, -18000LL, 0x6084325f3c52b4e8ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s10_n11_t2_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_exact_simple", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_exact_pushing", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_exact_pulling", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_exact_astar", {79u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_exact_backward", {49u, 0u, -33000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_pre_simple", {65u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_pre_pushing", {65u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_pre_pulling", {65u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_pre_astar", {69u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_pre_backward", {46u, 0u, -33000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_quota_simple", {50u, 6u, -17000LL, 0x393cddcaa9d47410ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_quota_pushing", {61u, 8u, -33000LL, 0x47ce1cb70d7523b8ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_quota_pulling", {61u, 8u, -33000LL, 0x47ce1cb70d7523b8ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_quota_astar", {52u, 6u, -17000LL, 0xa24c140b76f624b3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_quota_backward", {46u, 0u, -33000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_phases_simple", {79u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_phases_pushing", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_phases_pulling", {86u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_phases_astar", {84u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_phases_backward", {53u, 0u, -33000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_dominated_simple", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_dominated_pushing", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_dominated_pulling", {75u, 9u, -33000LL, 0xfc59b160261878c3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_dominated_astar", {79u, 10u, -33000LL, 0x442e8ee4d2596bbcULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_dominated_backward", {49u, 0u, -33000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_prune_pulling", {18u, 2u, -17000LL, 0xf73b9d6c40ffc887ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s11_n11_t2_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_exact_simple", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_exact_pushing", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_exact_pulling", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_exact_astar", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_exact_backward", {12u, 0u, -1000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_pre_simple", {13u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_pre_pushing", {13u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_pre_pulling", {13u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_pre_astar", {13u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_pre_backward", {12u, 0u, -1000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_quota_simple", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_quota_pushing", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_quota_pulling", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_quota_astar", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_quota_backward", {12u, 0u, -1000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_phases_simple", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_phases_pushing", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_phases_pulling", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_phases_astar", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_phases_backward", {12u, 0u, -1000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_dominated_simple", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_dominated_pushing", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_dominated_pulling", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_dominated_astar", {15u, 3u, -1000LL, 0xc56e183eecef35e0ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_dominated_backward", {12u, 0u, -1000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_prune_pulling", {1u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s12_n11_t2_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_exact_simple", {367u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_exact_pushing", {388u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_exact_pulling", {388u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_exact_astar", {376u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_exact_backward", {887u, 0u, -32000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_pre_simple", {339u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_pre_pushing", {385u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_pre_pulling", {385u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_pre_astar", {345u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_pre_backward", {844u, 0u, -32000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_quota_simple", {135u, 4u, -10000LL, 0x82fd74d2a8291e96ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_quota_pushing", {265u, 7u, -32000LL, 0x5cb6071eab65d007ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_quota_pulling", {265u, 7u, -32000LL, 0x5cb6071eab65d007ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_quota_astar", {142u, 5u, -28000LL, 0xd6f70fd9c030731aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_quota_backward", {124u, 0u, -6000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_phases_simple", {302u, 9u, -28000LL, 0xc2993e9419b7b715ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_phases_pushing", {388u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_phases_pulling", {408u, 10u, -32000LL, 0x8d1f228482729a91ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_phases_astar", {294u, 7u, -32000LL, 0x5cb6071eab65d007ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_phases_backward", {374u, 0u, -28000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_dominated_simple", {367u, 11u, -32000LL, 0x62039085add9f6f3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_dominated_pushing", {388u, 14u, -32000LL, 0x5e1fd3085c21d56bULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_dominated_pulling", {388u, 14u, -32000LL, 0x5e1fd3085c21d56bULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_dominated_astar", {376u, 12u, -32000LL, 0x7a4581c9443646cdULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_dominated_backward", {887u, 0u, -32000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_prune_pulling", {3u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s13_n16_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_exact_simple", {259u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_exact_pushing", {268u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_exact_pulling", {268u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_exact_astar", {304u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_exact_backward", {391u, 0u, -35000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_pre_simple", {228u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_pre_pushing", {226u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_pre_pulling", {226u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_pre_astar", {267u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_pre_backward", {351u, 0u, -35000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_quota_simple", {108u, 4u, -21000LL, 0x14980f553faaec11ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_quota_pushing", {163u, 6u, -29000LL, 0x23b7ff6237f12daaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_quota_pulling", {163u, 6u, -29000LL, 0x23b7ff6237f12daaULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_quota_astar", {95u, 3u, -23000LL, 0x4b25de7853cca805ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_quota_backward", {125u, 0u, -29000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_phases_simple", {238u, 8u, -35000LL, 0x22cfe41447f633dcULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_phases_pushing", {298u, 9u, -35000LL, 0x98346b8a442d966fULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_phases_pulling", {296u, 8u, -29000LL, 0xee91006ecb02a62aULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_phases_astar", {226u, 6u, -35000LL, 0xd11d596738980aa3ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_phases_backward", {321u, 0u, -35000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_dominated_simple", {259u, 8u, -35000LL, 0x22cfe41447f633dcULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_dominated_pushing", {268u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_dominated_pulling", {268u, 7u, -35000LL, 0x876c8aad12cbc5abULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_dominated_astar", {304u, 11u, -35000LL, 0x44d30c7e5abef639ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_dominated_backward", {391u, 0u, -35000LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_prune_simple", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_prune_pushing", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_prune_pulling", {57u, 2u, -28000LL, 0x37587a3b24da9488ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_prune_astar", {0u, 0u, 0LL, 0x14650fb0739d0383ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        {"s14_n16_t1_prune_backward", {0u, 0u, 0LL, 0x0000000000000000ULL, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
        // clang-format on
    };
    return rows;
}
inline const std::vector<Row>& golden_g2() {  // bidirectional (from #35)
    static const std::vector<Row> rows{
        // recorded with the bidirectional search
    };
    return rows;
}
inline const std::vector<Row>& golden_g3() {  // bidirectional with the level-3 settings (#43)
    static const std::vector<Row> rows{
        // recorded with the join budgets
    };
    return rows;
}

inline bool capturing() {
    return std::getenv("RCSPP_GOLDEN_CAPTURE") != nullptr;
}
inline bool enabled() {
    return capturing() || std::getenv("RCSPP_GOLDEN") != nullptr;
}

inline void sweep(const std::vector<Kind>& kinds, const std::vector<Row>& golden) {
    size_t compared = 0;
    for (const Spec& spec : specs()) {
        const Instance in = generate(spec);
        for (const auto& v : variants()) {
            for (const Kind k : kinds) {
                auto g = build(in);  // a fresh graph: feasibility preprocessing is persistent
                const Outcome o = run(k, v, g.get());
                const std::string key =
                    "s" + std::to_string(spec.seed) + "_n" + std::to_string(spec.n) + "_t" +
                    std::to_string(spec.terminals) + "_" + v.name + "_" + kind_name(k);
                if (capturing()) {
                    std::printf(
                        "GOLDEN {\"%s\", {%zuu, %zuu, %lldLL, 0x%016llxULL, %zuu, %zuu, %zuu, "
                        "%zuu, %zuu, %zuu, %zuu, %zuu}},\n",
                        key.c_str(),
                        o.extended,
                        o.solutions,
                        o.best_milli,
                        o.hash,
                        o.joined,
                        o.forward_labels,
                        o.backward_labels,
                        o.dominance_checks,
                        o.join_pairs_tested,
                        o.join_truncated,
                        o.bounded,
                        o.status);
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
                EXPECT_EQ(o.forward_labels, e.forward_labels) << key;
                EXPECT_EQ(o.backward_labels, e.backward_labels) << key;
                EXPECT_EQ(o.dominance_checks, e.dominance_checks) << key;
                EXPECT_EQ(o.join_pairs_tested, e.join_pairs_tested) << key;
                EXPECT_EQ(o.join_truncated, e.join_truncated) << key;
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

}  // namespace golden_labelling

TEST(GoldenLabelling, ForwardAndBackwardMatchTheRecordedRuns) {
    namespace gl = golden_labelling;
    if (!gl::enabled()) {
        GTEST_SKIP() << "set RCSPP_GOLDEN=1";
    }
    std::vector<gl::Kind> kinds{gl::Kind::Simple,
                                gl::Kind::Pushing,
                                gl::Kind::Pulling,
                                gl::Kind::AStar};
#if GOLDEN_LEVEL >= 1
    kinds.push_back(gl::Kind::Backward);
#endif
    gl::sweep(kinds, gl::golden_g1());
}

#if GOLDEN_LEVEL == 2
TEST(GoldenLabelling, BidirectionalMatchesTheRecordedRuns) {
    namespace gl = golden_labelling;
    if (!gl::enabled()) {
        GTEST_SKIP() << "set RCSPP_GOLDEN=1";
    }
    gl::sweep({gl::Kind::Bidirectional}, gl::golden_g2());
}
#endif

#if GOLDEN_LEVEL >= 3
// G3's settings include G2's six, so this test also covers what G2 checked.
TEST(GoldenLabelling, JoinBudgetsMatchTheRecordedRuns) {
    namespace gl = golden_labelling;
    if (!gl::enabled()) {
        GTEST_SKIP() << "set RCSPP_GOLDEN=1";
    }
    gl::sweep({gl::Kind::Bidirectional}, gl::golden_g3());
}
#endif

TEST(GoldenLabelling, TimingIsPrintedWhenAsked) {
    namespace gl = golden_labelling;
    if (std::getenv("RCSPP_TIMING") == nullptr) {
        GTEST_SKIP();
    }
    std::vector<gl::Kind> kinds{gl::Kind::Simple,
                                gl::Kind::Pushing,
                                gl::Kind::Pulling,
                                gl::Kind::AStar};
#if GOLDEN_LEVEL >= 2
    kinds.push_back(gl::Kind::Bidirectional);
#endif
    const gl::Variant exact{.name = "exact"};
    for (const gl::Kind k : kinds) {
        for (const size_t n : {size_t{40}, size_t{60}}) {
            long long micros = 0;
            for (uint32_t seed = 101; seed <= 103; ++seed) {
                auto g = gl::build(gl::generate({seed, n, 1}));
                const auto start = std::chrono::steady_clock::now();
                (void)gl::run(k, exact, g.get());
                micros += std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();
            }
            std::printf("TIMING %s %zu %lld\n", gl::kind_name(k), n, micros);
        }
    }
}
