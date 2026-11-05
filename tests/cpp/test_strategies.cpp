// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// A strategy of one's own: a descriptor written from the public parts (rcspp::parts) solves in
// every direction it searches, a part of one's own meets its concept, a malformed part is refused
// by the concept it fails, and rcspp::testing::expect_same_optimum checks an exact strategy
// against forward Simple.
//
// Its own translation unit: the user strategies instantiate searches nothing else needs.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <type_traits>

#include "rcspp/rcspp.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace strategies_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;

/// @brief Best first forward, first in, first out backward; the library's other parts.
struct BestFirstForward {
        template <typename R, typename Dir>
        using schedule =
            std::conditional_t<Dir::backward, parts::FifoSchedule<R>, parts::BestFirstSchedule<R>>;
};

/// @brief An admission rule of one's own: dominance, and at most three labels per node. Not
///        exact, and it says so.
struct AtMostThree {
        static bool exact(const parts::AdmissionContext& /*ctx*/) { return false; }

        template <typename ResourceType, typename NodeView>
        bool admit(const Label<ResourceType>& candidate, NodeView& node,
                   const parts::AdmissionContext& /*ctx*/) const {
            if (node.is_dominated(candidate) || node.size() >= 3) {
                return false;
            }
            node.evict_dominated_by(candidate);
            return true;
        }
};

/// @brief Simple's order with the rule of at most three labels per node; forward and backward
///        only.
struct SimpleAtMostThree {
        template <typename R, typename Dir>
        using schedule = parts::FifoSchedule<R>;
        template <typename R, typename Dir>
        using admission = AtMostThree;
        static constexpr bool bidirectional = false;
};

/// @brief A schedule that hands out nothing and cannot trim: not a schedule.
struct NotASchedule {
        void initialize(detail::SearchContext<Composed>& /*ctx*/) {}
        void push(const parts::StoredLabel<Composed>& /*stored*/) {}
        [[nodiscard]] size_t size() const { return 0; }
};

/// @brief An admission rule that does not say whether it is exact.
struct NoExact {
        template <typename ResourceType, typename NodeView>
        bool admit(const Label<ResourceType>& /*candidate*/, NodeView& /*node*/,
                   const parts::AdmissionContext& /*ctx*/) const {
            return true;
        }
};

/// @brief A model with a usable clock, and the params of a solve on it: the clock and its
///        half-way point set, for a bidirectional search.
struct Model {
        test_util::GeneratedInstance built;
        AlgorithmParams<List> params;
};

inline Model model(uint32_t seed) {
    test_util::InstanceConfig config;
    config.num_nodes = 12;
    config.with_time_window = true;
    config.with_capacity = true;
    config.capacity_binds = true;
    config.mixed_sign_costs = true;
    config.seed = seed;
    Model m{.built = test_util::build_instance(config), .params = {}};
    m.params.critical_resource_index = m.built.clock_index;
    m.params.half_way_point = m.built.clock_is_usable ? m.built.clock_upper_bound / 2.0 : 0.0;
    return m;
}

}  // namespace strategies_test

/// @brief The parts of the library meet their concepts; a malformed part fails the concept it
///        breaks, which is the check make_algorithm runs and names in its message.
TEST(Strategies, ThePartsMeetTheirConceptsAndAMalformedPartDoesNot) {
    namespace st = strategies_test;
    using R = st::Composed;
    static_assert(parts::LabelSchedule<parts::FifoSchedule<R>, R>);
    static_assert(parts::LabelSchedule<parts::BestFirstSchedule<R>, R>);
    static_assert(parts::LabelSchedule<parts::SweepSchedule<R, ForwardDirection>, R>);
    static_assert(parts::NodeSchedule<parts::PullSchedule<R, BackwardDirection>, R>);
    static_assert(parts::Admission<parts::DominanceAdmission, R>);
    static_assert(parts::Admission<st::AtMostThree, R>);
    static_assert(parts::Retention<parts::KeepAtNodes<st::List>, R>);
    static_assert(parts::Observer<parts::NoObserver, R>);

    static_assert(!parts::Schedule<st::NotASchedule, R>, "no trim, no next phase, no clear");
    static_assert(!parts::Admission<st::NoExact, R>, "no exact");

    static_assert(detail::searches<st::BestFirstForward>(SearchDirection::Bidirectional));
    static_assert(detail::searches<st::SimpleAtMostThree>(SearchDirection::Backward));
    static_assert(!detail::searches<st::SimpleAtMostThree>(SearchDirection::Bidirectional));
    static_assert(!detail::searches<strategy::Greedy>(SearchDirection::Backward));
    static_assert(std::is_same_v<detail::preset_t<strategy::Simple, R, ForwardDirection>,
                                 detail::Preset<parts::FifoSchedule<R>>>,
                  "a strategy that names only its schedule gets the exact search's parts");
}

/// @brief A strategy of one's own, from public parts only, solves in every direction, and runs a
///        best-first forward side and a first-in, first-out backward side bidirectionally.
TEST(Strategies, AStrategyOfOnesOwnSolvesInEveryDirection) {
    namespace st = strategies_test;
    auto m = st::model(1);
    for (const auto direction :
         {SearchDirection::Forward, SearchDirection::Backward, SearchDirection::Bidirectional}) {
        auto params = m.params;
        params.direction = direction;
        auto algorithm = m.built.graph->create_algorithm<st::BestFirstForward>(params);
        EXPECT_EQ(algorithm->direction(), direction);
        (void)m.built.graph->solve(algorithm.get());
    }
    auto params = m.params;
    params.direction = SearchDirection::Bidirectional;
    auto algorithm = m.built.graph->create_algorithm<st::BestFirstForward>(params);
    using Sides = detail::BidirectionalLabellingAlgorithm<
        st::Composed,
        st::List,
        RealResource,
        detail::Preset<parts::BestFirstSchedule<st::Composed>>,
        detail::Preset<parts::FifoSchedule<st::Composed>>>;
    EXPECT_NE(dynamic_cast<Sides*>(algorithm.get()), nullptr);
}

/// @brief The exact strategy of one's own finds forward Simple's optimum in every direction, along
///        paths that replay; so do the library's.
TEST(Strategies, ExpectSameOptimumPassesOnExactStrategies) {
    namespace st = strategies_test;
    for (const uint32_t seed : {1U, 2U, 3U}) {
        auto m = st::model(seed);
        EXPECT_EQ(
            rcspp::testing::expect_same_optimum<st::BestFirstForward>(*m.built.graph, m.params),
            "")
            << "seed " << seed;
        EXPECT_EQ(rcspp::testing::expect_same_optimum<strategy::AStar>(*m.built.graph, m.params),
                  "");
        EXPECT_EQ(rcspp::testing::expect_same_optimum<strategy::Pulling>(*m.built.graph, m.params),
                  "");
    }
}

/// @brief A strategy that refuses a direction is refused there, by name; one with an admission rule
///        of its own keeps at most what the rule admits.
TEST(Strategies, ARefusedDirectionAndAnAdmissionOfOnesOwn) {
    namespace st = strategies_test;
    auto m = st::model(2);
    auto params = m.params;
    params.direction = SearchDirection::Bidirectional;
    EXPECT_THROW((void)m.built.graph->create_algorithm<st::SimpleAtMostThree>(params),
                 std::invalid_argument);

    params.direction = SearchDirection::Forward;
    params.release_after_solve = false;
    auto algorithm = m.built.graph->create_algorithm<st::SimpleAtMostThree>(params);
    const auto result = m.built.graph->solve(algorithm.get());
    const auto exact = m.built.graph->solve<strategy::Simple>(m.params);
    ASSERT_FALSE(exact.solutions.empty());
    if (!result.solutions.empty()) {
        EXPECT_GE(result.solutions.front().cost, exact.solutions.front().cost - 1e-9)
            << "an inexact rule never beats the exact optimum";
    }
}
