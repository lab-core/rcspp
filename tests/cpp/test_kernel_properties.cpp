// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// What every exact labelling search must agree on, whatever its order of work: the optimum of
// forward Simple, paths that replay through the model, and, within one direction, the labels it
// ends with; and what the relaxed and k-best admission rules keep of it. Its own translation
// unit, as each row instantiates a search.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "util/equivalence_helpers.hpp"
#include "util/labelling.hpp"
#include "util/parity.hpp"

namespace kernel_properties {

using parity::Composed;
using parity::List;
using rcspp::SearchDirection;

/// @brief What one row did: its solutions, the label sets it ended with, and whether its result
///        is a proof.
struct Outcome {
        std::vector<rcspp::Solution> solutions;
        std::multiset<std::string> labels;
        std::multiset<std::string> backward_labels;
        std::map<size_t, size_t> labels_at;           ///< per node position, the labels stored
        std::map<size_t, size_t> backward_labels_at;  ///< per node position, the labels stored
        bool proven = false;
        std::string proof_lost_by;
};

/// @brief What a row sets in its params before it solves: the admission rule, say.
using Adjust = std::function<void(rcspp::AlgorithmBaseParams&)>;

/// @brief Solves @p c with an @p AlgorithmType in @p direction, with no bound and the labels
///        kept, with @p mask when one is given, and the params @p adjust sets; expects every
///        returned path to replay, and returns what it found.
///
/// The label sets hold `"<node position> | <resource>"` for every stored label of finite cost.
template <typename AlgorithmType>
Outcome solve_row(const parity::Case& c, SearchDirection direction,
                  const rcspp::ArcMask* mask = nullptr, const Adjust& adjust = {}) {
    auto graph = c.build();
    rcspp::AlgorithmParams<List> params;
    params.direction = direction;
    params.release_after_solve = false;
    if (direction == SearchDirection::Bidirectional) {
        params.critical_resource_index = c.clock_index;
        params.half_way_point = c.half_way_point;
    }
    if (adjust) {
        adjust(params);
    }
    parity::Observed<AlgorithmType> algorithm(&graph->get_resource_factory(), params);
    algorithm.set_arc_mask(mask);
    const auto result = graph->solve(&algorithm, parity::kInf, /*preprocess=*/false);
    Outcome out;
    out.solutions = result.solutions;
    out.proven = result.proven;
    out.proof_lost_by = result.proof_lost_by;
    algorithm.visit_labels([&out](bool backward, size_t pos, const rcspp::Label<Composed>& label) {
        ++(backward ? out.backward_labels_at : out.labels_at)[pos];
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
        std::function<Outcome(const parity::Case&, const rcspp::ArcMask*, const Adjust&)> run;

        /// @brief Solves @p c, with @p mask when one is given, and the params @p adjust sets.
        [[nodiscard]] Outcome solve(const parity::Case& c, const rcspp::ArcMask* mask = nullptr,
                                    const Adjust& adjust = {}) const {
            return run(c, mask, adjust);
        }
};

/// @brief @p Strategy's search in @p direction, as a row named @p name.
template <typename Strategy>
Row row(const std::string& name, SearchDirection direction) {
    return {
        name + " " + std::string(rcspp::to_string(direction)),
        direction,
        [direction](const parity::Case& c, const rcspp::ArcMask* mask, const Adjust& adjust) {
            switch (direction) {
                case SearchDirection::Forward:
                    return solve_row<test_util::Labelling<Strategy, Composed, List>>(c,
                                                                                     direction,
                                                                                     mask,
                                                                                     adjust);
                case SearchDirection::Backward:
                    return solve_row<
                        test_util::Labelling<Strategy, Composed, List, rcspp::BackwardDirection>>(
                        c,
                        direction,
                        mask,
                        adjust);
                case SearchDirection::Bidirectional:
                    break;
            }
            return solve_row<test_util::BidirectionalLabelling<Strategy, Composed, List>>(c,
                                                                                          direction,
                                                                                          mask,
                                                                                          adjust);
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

/// @brief The random cases with a capacity, built with the capacity's dominance as
///        @p capacity_in_dominance says; and the capacity's component index.
struct CapacityCases {
        std::vector<parity::Case> cases;
        std::vector<size_t> capacity_components;
};

/// @brief The random property cases (12 nodes, a time window and a binding capacity, seeds 1–4,
///        one and two sinks), with the capacity in dominance or left out of it.
inline CapacityCases capacity_cases(bool capacity_in_dominance) {
    CapacityCases all;
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
            config.capacity_in_dominance = capacity_in_dominance;
            const auto probe = test_util::build_instance(config);
            const double h = probe.clock_is_usable ? probe.clock_upper_bound / 2.0 : 0.0;
            all.cases.push_back({"random " + test_util::describe(config),
                                 [config] { return test_util::build_instance(config).graph; },
                                 probe.clock_index,
                                 h});
            all.capacity_components.push_back(2);  // the cost, the time window, the capacity
        }
    }
    return all;
}

/// @brief How many components @p c's model has: all are reals.
inline size_t number_of_components(const parity::Case& c) {
    auto graph = c.build();
    const auto* node = graph->get_node(graph->get_node_ids().front());
    return node->resource->get_components<rcspp::RealResource>().size();
}

/// @brief Sets the relaxed rule, leaving @p components out of dominance.
inline Adjust relaxed(std::set<size_t> components) {
    return [components = std::move(components)](rcspp::AlgorithmBaseParams& params) {
        params.admission_rule = rcspp::parts::AdmissionRule::Relaxed;
        params.dominance_ignored_components = components;
    };
}

/// @brief Sets the k-best rule, with @p k.
inline Adjust k_best(size_t k) {
    return [k](rcspp::AlgorithmBaseParams& params) {
        params.admission_rule = rcspp::parts::AdmissionRule::KBest;
        params.max_admitted_labels_per_node = k;
    };
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

/// @brief Each admission rule at its limit (a relaxed rule that leaves nothing out, a k-best rule
///        with no limit, or with a limit no node reaches) ends every exact search with the exact
///        rule's labels and solutions; only a rule that is not exact loses the proof.
TEST(KernelProperties, EachAdmissionRuleAtItsLimitEndsWithTheExactLabels) {
    namespace kp = kernel_properties;
    const auto rows = kp::rows();
    for (const auto& c : kp::property_cases()) {
        for (const auto& row : rows) {
            const auto expected = row.solve(c);
            EXPECT_TRUE(expected.proven) << c.name << ", " << row.name;
            for (const auto& [name, adjust, proven] :
                 {std::tuple{"relaxed over nothing", kp::relaxed({}), true},
                  std::tuple{"k-best with no limit", kp::k_best(rcspp::kNoLimit), true},
                  std::tuple{"k-best with a large k", kp::k_best(100'000), false}}) {
                const auto out = row.solve(c, nullptr, adjust);
                EXPECT_EQ(out.labels, expected.labels)
                    << c.name << ", " << row.name << ", " << name;
                EXPECT_EQ(out.backward_labels, expected.backward_labels)
                    << c.name << ", " << row.name << ", " << name;
                ASSERT_EQ(out.solutions.size(), expected.solutions.size())
                    << c.name << ", " << row.name << ", " << name;
                for (size_t i = 0; i < out.solutions.size(); ++i) {
                    EXPECT_EQ(out.solutions[i].path_arc_ids, expected.solutions[i].path_arc_ids)
                        << c.name << ", " << row.name << ", " << name;
                }
                EXPECT_EQ(out.proven, proven) << c.name << ", " << row.name << ", " << name;
                if (!proven) {
                    EXPECT_EQ(out.proof_lost_by, "relaxed_admission")
                        << c.name << ", " << row.name << ", " << name;
                }
            }
        }
    }
}

/// @brief A relaxed or k-best solve returns paths that replay, none cheaper than the optimum, and
///        no proof; under k-best, no node ends with more than k labels.
TEST(KernelProperties, ARelaxedOrKBestSolveReturnsPathsThatReplayAndNoneBelowTheOptimum) {
    namespace kp = kernel_properties;
    const auto rows = kp::rows();
    size_t fewer_labels = 0;
    for (const auto& c : kp::property_cases()) {
        const auto optimum = rows.front().solve(c);
        struct Rule {
                std::string name;
                kp::Adjust adjust;
                size_t k = 0;  ///< k-best's; 0 for a relaxed rule
        };
        std::vector<Rule> rules;
        for (size_t component = 1; component < kp::number_of_components(c); ++component) {
            rules.push_back(
                {"relaxed over " + std::to_string(component), kp::relaxed({component})});
        }
        for (const size_t k : {1, 2, 3}) {
            rules.push_back({"k-best with k " + std::to_string(k), kp::k_best(k), k});
        }
        for (const auto& row : rows) {
            const auto exact = row.solve(c);
            for (const auto& [name, adjust, k] : rules) {
                const auto out = row.solve(c, nullptr, adjust);  // its paths replay
                const std::string where = c.name + ", " + row.name + ", " + name;
                EXPECT_FALSE(out.proven) << where;
                if (!out.solutions.empty()) {
                    ASSERT_FALSE(optimum.solutions.empty()) << where;
                    EXPECT_GE(out.solutions.front().cost, optimum.solutions.front().cost - 1e-6)
                        << where;
                }
                if (k > 0) {
                    for (const auto& per_node : {out.labels_at, out.backward_labels_at}) {
                        for (const auto& [pos, count] : per_node) {
                            EXPECT_LE(count, k) << where << ", node position " << pos;
                        }
                    }
                }
                if (out.labels.size() + out.backward_labels.size() <
                    exact.labels.size() + exact.backward_labels.size()) {
                    ++fewer_labels;
                }
            }
        }
    }
    EXPECT_GT(fewer_labels, 0U) << "no rule ever dropped a label";
}

/// @brief Leaving the capacity out of dominance with the relaxed rule ends every exact search with
///        the labels and solutions of an exact solve on the model whose capacity dominance is
///        trivial.
TEST(KernelProperties, RelaxedDominanceIsDominanceWithTheComponentLeftOut) {
    namespace kp = kernel_properties;
    const auto rows = kp::rows();
    const auto with = kp::capacity_cases(/*capacity_in_dominance=*/true);
    const auto without = kp::capacity_cases(/*capacity_in_dominance=*/false);
    size_t differs = 0;
    for (size_t i = 0; i < with.cases.size(); ++i) {
        const auto& c = with.cases[i];
        for (const auto& row : rows) {
            const auto expected = row.solve(without.cases[i]);
            const auto exact = row.solve(c);
            const auto out = row.solve(c, nullptr, kp::relaxed({with.capacity_components[i]}));
            EXPECT_EQ(out.labels, expected.labels) << c.name << ", " << row.name;
            EXPECT_EQ(out.backward_labels, expected.backward_labels) << c.name << ", " << row.name;
            ASSERT_EQ(out.solutions.size(), expected.solutions.size())
                << c.name << ", " << row.name;
            for (size_t s = 0; s < out.solutions.size(); ++s) {
                EXPECT_EQ(out.solutions[s].path_arc_ids, expected.solutions[s].path_arc_ids)
                    << c.name << ", " << row.name;
            }
            if (out.labels != exact.labels || out.backward_labels != exact.backward_labels) {
                ++differs;
            }
        }
    }
    EXPECT_GT(differs, 0U) << "leaving the capacity out never changed a label";
}

/// @brief In a bidirectional solve, leaving the clock out of dominance turns the half-way point
///        off, and says why; leaving another component out keeps it on.
TEST(KernelProperties, LeavingTheClockOutOfDominanceTurnsTheHalfWayPointOff) {
    namespace kp = kernel_properties;
    const auto cases = kp::capacity_cases(/*capacity_in_dominance=*/true);
    const auto& c = cases.cases.front();
    const auto solve = [&c](const std::set<size_t>& ignored) {
        auto graph = c.build();
        rcspp::AlgorithmParams<kp::List> params;
        params.direction = SearchDirection::Bidirectional;
        params.critical_resource_index = c.clock_index;
        params.half_way_point = c.half_way_point;
        params.admission_rule = rcspp::parts::AdmissionRule::Relaxed;
        params.dominance_ignored_components = ignored;
        return graph->solve<rcspp::strategy::Simple>(params, /*preprocess=*/false);
    };
    ASSERT_EQ(c.clock_index, 1U) << "the time window, the first real component after the cost";
    const auto clock_left_out = solve({1});
    ASSERT_TRUE(clock_left_out.bidirectional.has_value());
    EXPECT_FALSE(clock_left_out.bidirectional->half_way_used);
    EXPECT_NE(clock_left_out.bidirectional->half_way_off_reason.find("left out of dominance"),
              std::string::npos)
        << clock_left_out.bidirectional->half_way_off_reason;
    const auto capacity_left_out = solve({2});
    ASSERT_TRUE(capacity_left_out.bidirectional.has_value());
    EXPECT_TRUE(capacity_left_out.bidirectional->half_way_used)
        << capacity_left_out.bidirectional->half_way_off_reason;
}

/// @brief A relaxed rule that names a component the model does not have, and a k-best rule that
///        would keep no label, are refused when the solve starts.
TEST(KernelProperties, AnAdmissionRuleTheModelCannotHonourIsRefused) {
    namespace kp = kernel_properties;
    const auto c = kp::property_cases().front();
    auto graph = c.build();
    rcspp::AlgorithmParams<kp::List> params;
    params.admission_rule = rcspp::parts::AdmissionRule::Relaxed;
    params.dominance_ignored_components = {99};
    EXPECT_THROW((void)graph->solve<rcspp::strategy::Simple>(params, /*preprocess=*/false),
                 std::invalid_argument);
    params.admission_rule = rcspp::parts::AdmissionRule::KBest;
    params.max_admitted_labels_per_node = 0;
    EXPECT_THROW((void)graph->solve<rcspp::strategy::Simple>(params, /*preprocess=*/false),
                 std::invalid_argument);
}
