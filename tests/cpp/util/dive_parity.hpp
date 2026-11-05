// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Runs a frozen copy of a heuristic and the library's heuristic of the same name on the same
// instance, and compares what each did: every label drawn, in order, the solutions, in the order
// returned, and the status.
//
// The dives have no per-extension hook, and the pool's counts are not theirs to keep: a dive on
// the labelling kernel keeps its labels chained, so it draws and releases them differently. What
// is compared is what a dive is: the tree of labels it builds. A fourth resource, the draw log,
// records it. Its value on a label is the label's place in the log (0 at a source), and its
// extension appends (the parent's place, the arc) to the log, so every label drawn and extended
// along an arc leaves one entry, whatever the search does with it afterwards.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/golden_harness.hpp"
#include "util/parity.hpp"

namespace dive_parity {

using parity::Composed;
using parity::List;
using GraphType = rcspp::ResourceGraph<rcspp::RealResource>;

/// @brief One label drawn and extended: the place in the log of the label it extends (0 for a
///        source label), and the arc.
struct Draw {
        size_t parent = 0;
        size_t arc_id = 0;
        bool operator==(const Draw&) const = default;
};

/// @brief The draw log's extension: appends (the parent's place, the arc) and sets the new label's
///        value to its own place.
///
/// The arc's consumption of this resource is the arc's id. Every copy of the function made for an
/// arc, or for a cloned graph, appends to the same log.
class DrawLogExtension : public rcspp::ExtensionFunction<rcspp::RealResource> {
    public:
        explicit DrawLogExtension(std::shared_ptr<std::vector<Draw>> log) : log_(std::move(log)) {}

        void extend(const rcspp::RealResource& resource, const rcspp::RealResource& extender_value,
                    rcspp::RealResource* extended_resource) override {
            log_->push_back({static_cast<size_t>(resource.get_value()),
                             static_cast<size_t>(extender_value.get_value())});
            extended_resource->set_value(static_cast<double>(log_->size()));
        }

        [[nodiscard]] auto clone() const
            -> std::unique_ptr<rcspp::ExtensionFunction<rcspp::RealResource>> override {
            return std::make_unique<DrawLogExtension>(*this);
        }

    private:
        std::shared_ptr<std::vector<Draw>> log_;
};

/// @brief The golden graph for @p in, with the draw log as a fourth resource, appending to @p log.
inline std::unique_ptr<GraphType> build(const golden_labelling::Instance& in,
                                        const std::shared_ptr<std::vector<Draw>>& log) {
    using rcspp::RealResource;
    auto g = std::make_unique<GraphType>();
    g->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    g->add_resource<RealResource>(
        std::make_unique<rcspp::TimeWindowExtensionFunction<RealResource>>(in.windows),
        std::make_unique<rcspp::TimeWindowFeasibilityFunction<RealResource>>(in.windows),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    g->add_resource<RealResource>(
        std::make_unique<rcspp::CapacityExtensionFunction<RealResource>>(in.capacity),
        std::make_unique<rcspp::MinMaxFeasibilityFunction<RealResource>>(0.0, in.capacity),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    g->add_resource<RealResource>(
        std::make_unique<DrawLogExtension>(log),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialDominanceFunction<RealResource>>());
    for (size_t v = 0; v < in.n; ++v) {
        g->add_node(v, in.is_source(v), in.is_sink(v));
    }
    for (size_t k = 0; k < in.arcs.size(); ++k) {
        const auto& [o, d, c, t, l] = in.arcs[k];
        const auto& arc = g->add_arc<RealResource, RealResource, RealResource, RealResource>(
            {c, t, l, static_cast<double>(k)},
            o,
            d,
            c);
        if (arc.id != k) {
            throw std::logic_error("the draw log reads an arc's id from its place in the instance");
        }
    }
    return g;
}

/// @brief An instance the dives are compared on, and the iterations a search with no budget
///        runs on it.
struct Case {
        std::string name;
        golden_labelling::Instance instance;
        size_t iteration_cap = rcspp::kNoLimit;
};

/// @brief The iterations a search with no budget runs on a 40-node instance: Greedy with none
///        draws tens of millions of labels there.
inline constexpr size_t kLargeIterationCap = 1000;

/// @brief The golden instances: seeds 1–4 at 8 and 11 nodes, seeds 9 and 10 (two sources and two
///        sinks) and seed 13 (16 nodes); with RCSPP_PARITY_FULL, all of them and seeds 15–18 at
///        25 and 40 nodes, the 40-node ones with @ref kLargeIterationCap.
inline std::vector<Case> cases() {
    namespace gl = golden_labelling;
    const bool full = parity::full_sweep();
    std::vector<gl::Spec> specs = gl::specs();
    if (full) {
        for (uint32_t seed = 15; seed <= 18; ++seed) {
            specs.push_back({seed, 25, 1});
            specs.push_back({seed, 40, 1});
        }
    }
    std::vector<Case> all;
    for (const gl::Spec& spec : specs) {
        const bool in_default =
            spec.seed <= 4 || spec.seed == 9 || spec.seed == 10 || spec.seed == 13;
        if (!full && !in_default) {
            continue;
        }
        all.push_back({"golden s" + std::to_string(spec.seed) + " n" + std::to_string(spec.n) +
                           " t" + std::to_string(spec.terminals),
                       gl::generate(spec),
                       spec.n >= 40 ? kLargeIterationCap : rcspp::kNoLimit});
    }
    return all;
}

/// @brief One setting the dives are compared under: a heuristics setting, with or without the
///        tabu tenure's jitter, and the number of solves on one object.
struct Setting {
        golden_labelling::Variant variant;
        bool jitter = false;
        size_t solves = 1;
        [[nodiscard]] std::string name() const {
            return std::string(variant.name) + (jitter ? " jitter" : "") +
                   (solves > 1 ? " x" + std::to_string(solves) : "");
        }
};

/// @brief Every heuristics setting without and with jitter (seed 3), and three exact solves on
///        one object, each with the previous best cost + 5 as its upper bound.
inline std::vector<Setting> settings() {
    std::vector<Setting> all;
    for (const auto& v : golden_labelling::heuristic_variants()) {
        all.push_back({v});
        all.push_back({v, true});
    }
    all.push_back({golden_labelling::heuristic_variants().front(), true, 3});
    return all;
}

/// @brief @p draws without a label's second extension, its later places renumbered.
///
/// The frozen Greedy extends the last label of a dive a second time when it is a dead end: the
/// next iteration starts from it, and draws its children again, all infeasible as before. A search
/// on the labelling kernel extends each label once. The repeat comes right after the first
/// extension, from the same parent along the same arcs, and draws no label that is extended in
/// turn, so without it the log is the tree the search built. One extension draws along each arc
/// once, so a (parent, arc) entry that repeats within a run of entries from the same parent is the
/// second extension.
inline std::vector<Draw> without_repeated_extensions(const std::vector<Draw>& draws) {
    std::vector<Draw> kept;
    std::vector<size_t> place(draws.size() + 1, 0);  // a draw's place in the log, renumbered
    std::vector<size_t> run_arcs;                    // the arcs drawn along in the current run
    for (size_t i = 0; i < draws.size(); ++i) {
        const Draw& draw = draws[i];
        if (i == 0 || draws[i - 1].parent != draw.parent) {
            run_arcs.clear();
        }
        if (std::ranges::find(run_arcs, draw.arc_id) != run_arcs.end()) {
            continue;  // the second extension: its labels are never extended
        }
        run_arcs.push_back(draw.arc_id);
        kept.push_back({place[draw.parent], draw.arc_id});
        place[i + 1] = kept.size();
    }
    return kept;
}

/// @brief Everything one solve of a dive did that a port must reproduce.
struct Observation {
        std::vector<Draw> draws;
        rcspp::AlgorithmStatus status = rcspp::AlgorithmStatus::Complete;
        std::vector<parity::SolutionRecord> solutions;
};

/// @brief Solves @p c under @p s with @p Strategy, releasing the labels after each solve when
///        @p release, and returns one observation per solve.
///
/// @param finite_budget Whether @p Strategy needs a finite @c max_iterations.
template <typename Strategy>
std::vector<Observation> observe(const Case& c, const Setting& s, bool finite_budget,
                                 bool release) {
    auto log = std::make_shared<std::vector<Draw>>();
    auto graph = build(c.instance, log);
    auto params = golden_labelling::heuristic_params<rcspp::AlgorithmParams<List>>(s.variant,
                                                                                   finite_budget,
                                                                                   *graph);
    params.release_after_solve = release;
    params.max_iterations = std::min(params.max_iterations, c.iteration_cap);
    if (s.jitter) {
        params.tabu_random_noise = true;
        params.seed = 3;
    }
    auto algorithm = graph->create_algorithm<Strategy>(params);
    std::vector<Observation> runs(s.solves);
    double upper_bound = s.variant.upper_bound;
    for (size_t k = 0; k < s.solves; ++k) {
        log->clear();
        const rcspp::SolveResult result = graph->solve(algorithm.get(), upper_bound, false);
        auto& o = runs[k];
        o.draws = without_repeated_extensions(*log);
        o.status = result.status;
        for (const auto& solution : result.solutions) {
            o.solutions.push_back(
                {parity::bits(solution.cost), solution.path_arc_ids, solution.path_node_ids});
        }
        if (!result.solutions.empty()) {
            upper_bound = result.solutions.front().cost + 5.0;
        }
    }
    return runs;
}

/// @brief Whether @p library's status is @p reference's, or the one a search on the labelling
///        kernel reports for the same run.
///
/// When the solution that reaches @c max_solutions is found on the last path, a frozen dive
/// reports @c MaxSolutions, its path still holding the label, and a search on the kernel
/// @c Complete, no label waiting, as every labelling search does. Their draws being the same, both
/// tried every path.
[[nodiscard]] inline bool same_status(rcspp::AlgorithmStatus reference,
                                      rcspp::AlgorithmStatus library) {
    return reference == library || (reference == rcspp::AlgorithmStatus::MaxSolutions &&
                                    library == rcspp::AlgorithmStatus::Complete);
}

/// @brief Whether two observations agree; if not, where they first part: the draws, then the
///        status (@ref same_status), then the solutions.
inline ::testing::AssertionResult same(const Observation& a, const Observation& b) {
    const size_t common = std::min(a.draws.size(), b.draws.size());
    for (size_t i = 0; i < common; ++i) {
        if (!(a.draws[i] == b.draws[i])) {
            std::ostringstream out;
            out << "the draws part at draw " << i + 1 << ": reference (from " << a.draws[i].parent
                << ", arc " << a.draws[i].arc_id << "), library (from " << b.draws[i].parent
                << ", arc " << b.draws[i].arc_id << ")";
            return ::testing::AssertionFailure() << out.str();
        }
    }
    if (a.draws.size() != b.draws.size()) {
        return ::testing::AssertionFailure() << "the draws agree for " << common
                                             << " labels, then one search stops: " << a.draws.size()
                                             << " against " << b.draws.size();
    }
    if (!same_status(a.status, b.status)) {
        return ::testing::AssertionFailure() << "status: reference " << static_cast<int>(a.status)
                                             << ", library " << static_cast<int>(b.status);
    }
    if (a.solutions.size() != b.solutions.size()) {
        return ::testing::AssertionFailure() << "solutions: reference " << a.solutions.size()
                                             << ", library " << b.solutions.size();
    }
    for (size_t i = 0; i < a.solutions.size(); ++i) {
        if (!(a.solutions[i] == b.solutions[i])) {
            return ::testing::AssertionFailure() << "solution " << i << " differs";
        }
    }
    return ::testing::AssertionSuccess();
}

/// @brief @ref same, solve by solve.
inline ::testing::AssertionResult same(const std::vector<Observation>& reference,
                                       const std::vector<Observation>& library) {
    if (reference.size() != library.size()) {
        return ::testing::AssertionFailure() << "the solve counts differ";
    }
    for (size_t k = 0; k < reference.size(); ++k) {
        if (auto r = same(reference[k], library[k]); !r) {
            return ::testing::AssertionFailure() << "solve " << k << ": " << r.message();
        }
    }
    return ::testing::AssertionSuccess();
}

/// @brief Whether a run of @p c under @p s has a finite @c max_iterations.
///
/// @param finite_budget Whether the search needs a finite @c max_iterations.
[[nodiscard]] inline bool has_budget(const Case& c, const Setting& s, bool finite_budget) {
    return finite_budget || s.variant.max_iterations != rcspp::kNoLimit ||
           c.iteration_cap != rcspp::kNoLimit;
}

/// @brief Runs @p ReferenceStrategy and @p LibraryStrategy over every case and setting, in both
///        release modes, and expects them to agree.
///
/// A search on the labelling kernel counts its iterations in labels taken, where a frozen dive
/// counted dives: @p compare_budgets false leaves out every run with a finite
/// @c max_iterations, which the two spend differently.
///
/// @param finite_budget   Whether the two need a finite @c max_iterations.
/// @param compare_budgets Whether runs with a finite @c max_iterations are compared.
/// @param comparable      Called after each run of @p ReferenceStrategy: whether that run is
///                        compared (by default, every run is).
/// @return The number of (case, setting, release mode) runs compared.
template <typename ReferenceStrategy, typename LibraryStrategy>
size_t sweep(bool finite_budget, bool compare_budgets = true,
             const std::function<bool()>& comparable = {}) {
    size_t compared = 0;
    for (const Case& c : cases()) {
        for (const Setting& s : settings()) {
            if (!compare_budgets && has_budget(c, s, finite_budget)) {
                continue;
            }
            for (const bool release : {false, true}) {
                const auto expected = observe<ReferenceStrategy>(c, s, finite_budget, release);
                if (comparable && !comparable()) {
                    continue;
                }
                const auto actual = observe<LibraryStrategy>(c, s, finite_budget, release);
                EXPECT_TRUE(same(expected, actual))
                    << c.name << ", setting " << s.name() << (release ? ", released" : ", kept");
                ++compared;
            }
        }
    }
    EXPECT_GT(compared, 0U);
    return compared;
}

}  // namespace dive_parity
