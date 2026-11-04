// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// What every exact labelling search must agree on, whatever its order of work: the optimum of
// forward Simple, paths that replay through the model, and, within one direction, the labels it
// ends with. Its own translation unit, as each row instantiates a search.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "util/equivalence_helpers.hpp"
#include "util/labelling.hpp"
#include "util/parity.hpp"

namespace kernel_properties {

using parity::Composed;
using parity::List;
using rcspp::SearchDirection;

/// @brief What one row did: its solutions, and the label sets it ended with.
struct Outcome {
        std::vector<rcspp::Solution> solutions;
        std::multiset<std::string> labels;
        std::multiset<std::string> backward_labels;
};

/// @brief Solves @p c with an @p AlgorithmType in @p direction, with no bound and the labels
///        kept, and with @p mask when one is given; expects every returned path to replay, and
///        returns what it found.
///
/// The label sets hold `"<node position> | <resource>"` for every stored label of finite cost.
template <typename AlgorithmType>
Outcome solve_row(const parity::Case& c, SearchDirection direction,
                  const rcspp::ArcMask* mask = nullptr) {
    auto graph = c.build();
    rcspp::AlgorithmParams<List> params;
    params.direction = direction;
    params.release_after_solve = false;
    if (direction == SearchDirection::Bidirectional) {
        params.critical_resource_index = c.clock_index;
        params.half_way_point = c.half_way_point;
    }
    parity::Observed<AlgorithmType> algorithm(&graph->get_resource_factory(), params);
    algorithm.set_arc_mask(mask);
    const auto result = graph->solve(&algorithm, parity::kInf, /*preprocess=*/false);
    Outcome out;
    out.solutions = result.solutions;
    algorithm.visit_labels([&out](bool backward, size_t pos, const rcspp::Label<Composed>& label) {
        if (std::isinf(label.get_cost())) {
            return;
        }
        auto& set = backward ? out.backward_labels : out.labels;
        set.insert(std::to_string(pos) + " | " + label.get_resource().to_string());
    });
    for (const auto& solution : result.solutions) {
        EXPECT_EQ(equivalence_test::path_problem(*graph, solution), "") << c.name;
    }
    return out;
}

/// @brief One exact search: a strategy in a direction.
struct Row {
        std::string name;
        SearchDirection direction;
        std::function<Outcome(const parity::Case&, const rcspp::ArcMask*)> run;

        /// @brief Solves @p c, with @p mask when one is given.
        [[nodiscard]] Outcome solve(const parity::Case& c,
                                    const rcspp::ArcMask* mask = nullptr) const {
            return run(c, mask);
        }
};

/// @brief @p Strategy's search in @p direction, as a row named @p name.
template <typename Strategy>
Row row(const std::string& name, SearchDirection direction) {
    return {
        name + " " + std::string(rcspp::to_string(direction)),
        direction,
        [direction](const parity::Case& c, const rcspp::ArcMask* mask) {
            switch (direction) {
                case SearchDirection::Forward:
                    return solve_row<test_util::Labelling<Strategy, Composed, List>>(c,
                                                                                     direction,
                                                                                     mask);
                case SearchDirection::Backward:
                    return solve_row<
                        test_util::Labelling<Strategy, Composed, List, rcspp::BackwardDirection>>(
                        c,
                        direction,
                        mask);
                case SearchDirection::Bidirectional:
                    break;
            }
            return solve_row<test_util::BidirectionalLabelling<Strategy, Composed, List>>(c,
                                                                                          direction,
                                                                                          mask);
        }};
}

/// @brief The exact searches: forward Simple (first, the reference), A*, Pushing and Pulling;
///        backward Simple (first in its direction), A*, Pushing and Pulling; bidirectional
///        Simple (first in its direction), A*, Pushing and Pulling.
inline std::vector<Row> rows() {
    namespace strategy = rcspp::strategy;
    std::vector<Row> all;
    for (const auto direction :
         {SearchDirection::Forward, SearchDirection::Backward, SearchDirection::Bidirectional}) {
        all.push_back(row<strategy::Simple>("simple", direction));
        all.push_back(row<strategy::AStar>("astar", direction));
        all.push_back(row<strategy::Pushing>("pushing", direction));
        all.push_back(row<strategy::Pulling>("pulling", direction));
    }
    return all;
}

/// @brief The cases: golden seeds 1–8 at 8 and 11 nodes, and random instances of 12 nodes, seeds
///        1–4, with one and two sinks.
inline std::vector<parity::Case> property_cases() {
    namespace gl = golden_labelling;
    std::vector<parity::Case> all;
    for (const gl::Spec& spec : gl::specs()) {
        if (spec.seed > 8) {
            continue;
        }
        all.push_back({"golden s" + std::to_string(spec.seed) + " n" + std::to_string(spec.n),
                       [spec] { return gl::build(gl::generate(spec)); },
                       1,
                       25.0});
    }
    for (uint32_t seed = 1; seed <= 4; ++seed) {
        for (const size_t sinks : {size_t{1}, size_t{2}}) {
            test_util::InstanceConfig config;
            config.num_nodes = 12;
            config.with_time_window = true;
            config.with_capacity = true;
            config.capacity_binds = true;
            config.mixed_sign_costs = true;
            config.num_sinks = sinks;
            config.seed = seed;
            const auto probe = test_util::build_instance(config);
            const double h = probe.clock_is_usable ? probe.clock_upper_bound / 2.0 : 0.0;
            all.push_back({"random " + test_util::describe(config),
                           [config] { return test_util::build_instance(config).graph; },
                           probe.clock_index,
                           h});
        }
    }
    return all;
}

}  // namespace kernel_properties

/// @brief Every exact search returns forward Simple's optimum (or, like it, nothing), along paths
///        that replay through the model to their reported cost.
TEST(KernelProperties, EveryExactSearchReachesTheForwardOptimumAlongPathsThatReplay) {
    namespace kp = kernel_properties;
    const auto rows = kp::rows();
    for (const auto& c : kp::property_cases()) {
        const auto reference = rows.front().solve(c);
        for (const auto& row : rows) {
            const auto out = row.solve(c);
            ASSERT_EQ(out.solutions.empty(), reference.solutions.empty())
                << c.name << ", " << row.name;
            if (!out.solutions.empty()) {
                EXPECT_NEAR(out.solutions.front().cost, reference.solutions.front().cost, 1e-6)
                    << c.name << ", " << row.name;
            }
        }
    }
}

/// @brief Greedy with no budget tries every path: it returns forward Simple's optimum (or, like
///        it, nothing), along paths that replay through the model.
TEST(KernelProperties, GreedyWithNoBudgetReachesTheForwardOptimum) {
    namespace kp = kernel_properties;
    const auto reference = kp::rows().front();
    for (const auto& c : kp::property_cases()) {
        const auto expected = reference.solve(c);
        auto graph = c.build();
        const auto result =
            graph->solve<rcspp::strategy::Greedy>(rcspp::AlgorithmParams<kp::List>{}, false);
        ASSERT_EQ(result.solutions.empty(), expected.solutions.empty()) << c.name;
        EXPECT_EQ(result.status, rcspp::AlgorithmStatus::Complete) << c.name;
        if (!result.solutions.empty()) {
            EXPECT_NEAR(result.solutions.front().cost, expected.solutions.front().cost, 1e-6)
                << c.name;
        }
        for (const auto& solution : result.solutions) {
            EXPECT_EQ(equivalence_test::path_problem(*graph, solution), "") << c.name;
        }
    }
}

/// @brief A hard mask leaves its arcs out of every search, as if they were removed from the
///        graph: every exact search, and Greedy with no budget, returns the optimum of the graph
///        without them, along paths that use none of them and replay on the whole graph.
///
/// The arcs masked are those of forward Simple's optimal path, so each search must find another.
/// The bounds the searches compute on the whole graph stay lower bounds on the masked one.
TEST(KernelProperties, AHardMaskLeavesItsArcsOutOfEverySearch) {
    namespace kp = kernel_properties;
    const auto rows = kp::rows();
    size_t masked_cases = 0;
    for (const auto& c : kp::property_cases()) {
        const auto unmasked = rows.front().solve(c);
        if (unmasked.solutions.empty()) {
            continue;
        }
        // A path may use an arc more than once: each is masked, and removed, once.
        const auto& path = unmasked.solutions.front().path_arc_ids;
        const std::set<size_t> arcs(path.begin(), path.end());
        auto graph = c.build();
        rcspp::ArcMask mask(graph->get_arc_id_bound());
        auto removed = c.build();
        for (const size_t arc_id : arcs) {
            mask.set(arc_id, rcspp::ArcMask::State::Hard);
            ASSERT_TRUE(removed->remove_arc(arc_id));
        }
        const auto expected =
            removed->solve<rcspp::strategy::Simple>(rcspp::AlgorithmParams<kp::List>{},
                                                    /*preprocess=*/false);
        ++masked_cases;

        const auto check = [&](const std::vector<rcspp::Solution>& solutions,
                               const std::string& name) {
            ASSERT_EQ(solutions.empty(), expected.solutions.empty()) << c.name << ", " << name;
            if (!solutions.empty()) {
                EXPECT_NEAR(solutions.front().cost, expected.solutions.front().cost, 1e-6)
                    << c.name << ", " << name;
            }
            for (const auto& solution : solutions) {
                for (const size_t arc_id : solution.path_arc_ids) {
                    EXPECT_FALSE(mask.is_hard(arc_id)) << c.name << ", " << name;
                }
                EXPECT_EQ(equivalence_test::path_problem(*graph, solution), "")
                    << c.name << ", " << name;
            }
        };
        for (const auto& row : rows) {
            check(row.solve(c, &mask).solutions, row.name);
        }
        auto greedy =
            graph->create_algorithm<rcspp::strategy::Greedy>(rcspp::AlgorithmParams<kp::List>{});
        greedy->set_arc_mask(&mask);
        check(graph->solve(greedy.get(), parity::kInf, /*preprocess=*/false).solutions, "greedy");
    }
    EXPECT_GT(masked_cases, 0U);
}

/// @brief Within one direction, every exact search ends with the same labels of finite cost.
TEST(KernelProperties, WithinADirectionEveryExactSearchEndsWithTheSameLabels) {
    namespace kp = kernel_properties;
    const auto rows = kp::rows();
    for (const auto& c : kp::property_cases()) {
        for (const auto direction : {SearchDirection::Forward,
                                     SearchDirection::Backward,
                                     SearchDirection::Bidirectional}) {
            const kp::Row* first = nullptr;
            kp::Outcome expected;
            for (const auto& row : rows) {
                if (row.direction != direction) {
                    continue;
                }
                auto out = row.solve(c);
                if (first == nullptr) {
                    first = &row;
                    expected = std::move(out);
                    EXPECT_FALSE(expected.labels.empty()) << c.name << ", " << row.name;
                    continue;
                }
                EXPECT_EQ(out.labels, expected.labels)
                    << c.name << ", " << row.name << " against " << first->name;
                EXPECT_EQ(out.backward_labels, expected.backward_labels)
                    << c.name << ", " << row.name << " against " << first->name;
            }
        }
    }
}
