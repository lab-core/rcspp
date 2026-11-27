// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// CostOrderedLabelList: the container itself (the order, the erasures, the scans the promise
// lets it skip), and the searches on it against the same searches on LabelList, with and without a
// dominance function that declares a cost order: the same extensions, labels and solutions, fewer
// dominance tests, and a wrong promise caught by RCSPP_VERIFY_COST_ORDER (the Debug builds). Then
// the promise CompositionDominanceFunction infers from the components and the cost function, and
// the default container.
//
// Its own translation unit: the searches on the new container instantiate what nothing else needs.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/golden_harness.hpp"
#include "util/parity.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace cost_order_test {
/// @brief The members a search calls, for labels of one resource type, whose resources make no
///        composition promise: they compile (never called).
template <typename Direction>
[[maybe_unused]] void store_single_type_label(CostOrderedLabelList<RealResource, Direction>& labels,
                                              Label<RealResource>& label) {
    if (!labels.is_dominated(label)) {
        (void)labels.remove_dominated_labels(label);
        labels.add_label(&label);
    }
}
template void store_single_type_label(CostOrderedLabelList<RealResource>&, Label<RealResource>&);
template void store_single_type_label(CostOrderedLabelList<RealResource, BackwardDirection>&,
                                      Label<RealResource>&);
}  // namespace cost_order_test

namespace cost_order_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;
using CostOrdered = CostOrderedLabelList<Composed>;
using Stored = parts::StoredLabel<Composed>;
constexpr double kInf = std::numeric_limits<double>::infinity();

/// The tolerance the tests declare: the default model's cost component compares within the
/// machine epsilon (value_leq), so any larger tolerance is a true promise.
constexpr double kTolerance = 1e-9;

/// @brief What a counting dominance function saw: every test, and those whose first label costs
///        more than the second's cost + the declared tolerance (which a promise lets a cost-ordered
///        container skip).
struct Counts {
        size_t tests = 0;
        size_t outside = 0;
};

/// @brief Component-wise dominance (@c CompositionDominanceFunction), declaring a cost order
///        (the same forward and backward, or one each), and counting its tests into @p counts if
///        given, with those outside the declared tolerance of their direction. A derived class,
///        so it infers no promise unless told to (@ref infer_from_components).
template <typename... ResourceTypes>
class DeclaringDominance
    : public Clonable<DeclaringDominance<ResourceTypes...>,
                      CompositionDominanceFunction<ResourceTypes...>,
                      DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> {
        using Base = CompositionDominanceFunction<ResourceTypes...>;
        using ResourceClass = Resource<ResourceTypeComposition<ResourceTypes...>>;

    public:
        explicit DeclaringDominance(std::optional<double> tolerance,
                                    std::shared_ptr<Counts> counts = nullptr)
            : tolerance_(tolerance), back_tolerance_(tolerance), counts_(std::move(counts)) {}

        DeclaringDominance(std::optional<double> tolerance, std::optional<double> back_tolerance,
                           std::shared_ptr<Counts> counts)
            : tolerance_(tolerance), back_tolerance_(back_tolerance), counts_(std::move(counts)) {}

        [[nodiscard]] bool check_dominance(const ResourceClass& lhs,
                                           const ResourceClass& rhs) override {
            count(lhs, rhs, tolerance_);
            return Base::check_dominance(lhs, rhs);
        }

        [[nodiscard]] bool check_back_dominance(const ResourceClass& lhs,
                                                const ResourceClass& rhs) override {
            count(lhs, rhs, back_tolerance_);
            return Base::check_back_dominance(lhs, rhs);
        }

        [[nodiscard]] std::optional<double> cost_order_tolerance() const override {
            return tolerance_;
        }
        [[nodiscard]] std::optional<double> back_cost_order_tolerance() const override {
            return back_tolerance_;
        }

        /// @brief Declares requires_component_dominance (it tests as the base does), so that,
        ///        declaring no tolerance, it has one inferred.
        void infer_from_components() { infers_ = true; }
        [[nodiscard]] bool requires_component_dominance() const override { return infers_; }

    private:
        void count(const ResourceClass& lhs, const ResourceClass& rhs,
                   std::optional<double> tolerance) const {
            if (counts_ == nullptr) {
                return;
            }
            ++counts_->tests;
            if (tolerance.has_value() && lhs.get_cost() > rhs.get_cost() + *tolerance) {
                ++counts_->outside;
            }
        }

        std::optional<double> tolerance_;
        std::optional<double> back_tolerance_;
        std::shared_ptr<Counts> counts_;
        bool infers_ = false;
};

/// @brief A dominance that lets a label dominate one costing up to @p slack less (the costs
///        within @p slack, the other components as usual), declaring the tolerance @p declared:
///        a wrong promise when it is below @p slack (by default 1 and 0), a true one otherwise.
template <typename... ResourceTypes>
class LooseDominance
    : public Clonable<LooseDominance<ResourceTypes...>,
                      CompositionDominanceFunction<ResourceTypes...>,
                      DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> {
        using ResourceClass = Resource<ResourceTypeComposition<ResourceTypes...>>;

    public:
        explicit LooseDominance(double slack = 1.0, double declared = 0.0)
            : slack_(slack), declared_(declared) {}

        [[nodiscard]] bool check_dominance(const ResourceClass& lhs,
                                           const ResourceClass& rhs) override {
            size_t component = 0;
            return lhs.for_each_component_and(
                rhs,
                [&component, slack = slack_](const auto& lhs_res, const auto& rhs_res) {
                    if (component++ == 0) {
                        return lhs_res.get_value().get_value() <=
                               rhs_res.get_value().get_value() + slack;
                    }
                    return lhs_res <= rhs_res;
                });
        }
        [[nodiscard]] std::optional<double> cost_order_tolerance() const override {
            return declared_;
        }

    private:
        double slack_;
        double declared_;
};

/// @brief An empty graph of @p ResourceTypes with the default functions, but @p dominance as its
///        composition dominance function.
template <typename... ResourceTypes>
std::unique_ptr<ResourceGraph<ResourceTypes...>> graph_with(
    std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> dominance) {
    return std::make_unique<ResourceGraph<ResourceTypes...>>(
        std::make_unique<CompositionExtensionFunction<ResourceTypes...>>(),
        std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
        std::make_unique<ComponentCostFunction<0, ResourceTypes...>>(0),
        std::move(dominance));
}

/// @brief An empty graph whose dominance declares @p tolerance (none: no promise), counting into
///        @p counts.
template <typename... ResourceTypes>
std::unique_ptr<ResourceGraph<ResourceTypes...>> declaring_graph(
    std::optional<double> tolerance, std::shared_ptr<Counts> counts = nullptr) {
    // Converted here: a pack is deduced from the exact type, without the derived-to-base step.
    std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> dominance =
        std::make_unique<DeclaringDominance<ResourceTypes...>>(tolerance, std::move(counts));
    return graph_with<ResourceTypes...>(std::move(dominance));
}

/// @brief The random instances of the parity cases: 12 nodes, a time window, a binding capacity,
///        costs of both signs, one or two sinks.
inline std::vector<test_util::InstanceConfig> random_configs() {
    std::vector<test_util::InstanceConfig> all;
    for (uint32_t seed = 1; seed <= 2; ++seed) {
        for (const size_t sinks : {size_t{1}, size_t{2}}) {
            test_util::InstanceConfig config;
            config.num_nodes = 12;
            config.with_time_window = true;
            config.with_capacity = true;
            config.capacity_binds = true;
            config.mixed_sign_costs = true;
            config.num_sinks = sinks;
            config.seed = seed;
            all.push_back(config);
        }
    }
    return all;
}

/// @brief The parity cases (golden and random instances), their graphs built by @p empty_graph.
template <typename EmptyGraphCallable>
std::vector<parity::Case> cases(const EmptyGraphCallable& empty_graph) {
    namespace gl = golden_labelling;
    std::vector<parity::Case> all;
    for (const gl::Spec& spec : gl::specs()) {
        if (spec.seed > 4 && spec.seed != 9 && spec.seed != 10 && spec.seed != 13) {
            continue;
        }
        all.push_back({"golden s" + std::to_string(spec.seed) + " n" + std::to_string(spec.n),
                       [spec, empty_graph] { return gl::build(gl::generate(spec), empty_graph()); },
                       1,
                       25.0});
    }
    for (const auto& config : random_configs()) {
        const auto probe = test_util::build_instance(config);
        all.push_back({"random " + test_util::describe(config),
                       [config, empty_graph] {
                           return test_util::build_instance(config, empty_graph()).graph;
                       },
                       probe.clock_index,
                       probe.clock_is_usable ? probe.clock_upper_bound / 2.0 : 0.0});
    }
    return all;
}

/// @brief @p observation with its solutions and labels sorted: the containers keep and record them
///        in different orders (a list in storing order, a cost-ordered container in cost order).
inline parity::Observation canonical(parity::Observation observation) {
    std::ranges::sort(observation.solutions, [](const auto& a, const auto& b) {
        return std::tie(a.cost_bits, a.arcs) < std::tie(b.cost_bits, b.arcs);
    });
    const auto by_label = [](const auto& a, const auto& b) {
        return std::tie(a.node_pos, a.label_id) < std::tie(b.node_pos, b.label_id);
    };
    std::ranges::sort(observation.labels, by_label);
    std::ranges::sort(observation.backward_labels, by_label);
    return observation;
}

/// @brief Pulling's order backward, as a one-parameter schedule template.
template <typename ResourceType>
using BackwardPull = parts::PullSchedule<ResourceType, BackwardDirection>;

/// @brief The search on @p Schedule in @p Direction, under every parity setting, on @p c: the same
///        extensions, counts, labels and solutions (as sets) on both containers.
template <template <typename> class Schedule, typename Direction>
void expect_same_search(const parity::Case& c) {
    const SearchDirection direction =
        Direction::backward ? SearchDirection::Backward : SearchDirection::Forward;
    const auto with_list = parity::settings(direction, List{});
    const auto with_order = parity::settings(direction, CostOrdered{});
    ASSERT_EQ(with_list.size(), with_order.size());
    for (size_t i = 0; i < with_list.size(); ++i) {
        SCOPED_TRACE(c.name + ", " + with_list[i].name);
        auto expected =
            parity::observe<parity::TracedLabellingAlgorithm<Schedule, List, Direction>>(
                c,
                with_list[i],
                /*release=*/false);
        auto actual =
            parity::observe<parity::TracedLabellingAlgorithm<Schedule, CostOrdered, Direction>>(
                c,
                with_order[i],
                /*release=*/false);
        ASSERT_EQ(expected.size(), actual.size());
        for (size_t k = 0; k < expected.size(); ++k) {
            EXPECT_TRUE(parity::same(canonical(expected[k]), canonical(actual[k])))
                << "solve " << k;
        }
    }
}

/// @brief Every single-direction search on every case built by @p empty_graph.
template <typename EmptyGraphCallable>
void expect_same_searches(const EmptyGraphCallable& empty_graph) {
    for (const auto& c : cases(empty_graph)) {
        {
            SCOPED_TRACE("Simple forward");
            expect_same_search<parts::FifoSchedule, ForwardDirection>(c);
        }
        {
            SCOPED_TRACE("Simple backward");
            expect_same_search<parts::FifoSchedule, BackwardDirection>(c);
        }
        {
            SCOPED_TRACE("A*");
            expect_same_search<parity::CompletionOrder, ForwardDirection>(c);
        }
        {
            SCOPED_TRACE("Pushing");
            expect_same_search<parity::ForwardSweep, ForwardDirection>(c);
        }
        {
            SCOPED_TRACE("Pulling forward");
            expect_same_search<parity::ForwardPull, ForwardDirection>(c);
        }
        {
            SCOPED_TRACE("Pulling backward");
            expect_same_search<BackwardPull, BackwardDirection>(c);
        }
    }
}

/// @brief The solutions of @p result as a sorted list of (cost, path).
inline std::vector<std::pair<double, std::vector<size_t>>> solution_set(const SolveResult& result) {
    std::vector<std::pair<double, std::vector<size_t>>> all;
    for (const auto& solution : result.solutions) {
        all.emplace_back(solution.cost, solution.path_arc_ids);
    }
    std::ranges::sort(all);
    return all;
}

/// @brief @p graph solved by @p Strategy on both containers with @p params' settings: the same
///        status and the same solutions, as sets.
template <typename Strategy, typename ParamsCallable, typename... ResourceTypes>
void expect_same_solve(ResourceGraph<ResourceTypes...>& graph, const ParamsCallable& set) {
    using GraphComposed = ResourceTypeComposition<ResourceTypes...>;
    AlgorithmParams<LabelList<GraphComposed>> list_params;
    AlgorithmParams<CostOrderedLabelList<GraphComposed>> order_params;
    set(list_params);
    set(order_params);
    const auto expected =
        graph.template solve<Strategy, LabelList<GraphComposed>>(list_params, /*preprocess=*/false);
    const auto actual =
        graph.template solve<Strategy, CostOrderedLabelList<GraphComposed>>(order_params,
                                                                            /*preprocess=*/false);
    EXPECT_EQ(actual.status, expected.status);
    EXPECT_EQ(solution_set(actual), solution_set(expected));
}

/// @brief A label of a two-component composition (cost, time) whose dominance is
///        @p dominance, made by @p graph's factory so that it carries the graph's functions.
inline std::unique_ptr<Label<Composed>> make_label(ResourceGraph<RealResource>& graph, size_t id,
                                                   double cost, double time) {
    auto resource = graph.get_resource_factory().create_resource(/*node_id=*/0);
    resource->template get_component<0>(0).set_value(cost);
    resource->template get_component<0>(1).set_value(time);  // mutable accesses: the cost is read
    return std::make_unique<Label<Composed>>(id, std::move(resource));
}

/// @brief An empty graph of two real components, cost and time, both added and compared by
///        value, whose dominance declares @p tolerance and counts into @p counts.
inline std::unique_ptr<ResourceGraph<RealResource>> two_component_graph(
    std::unique_ptr<DominanceFunction<Composed>> dominance) {
    auto graph = graph_with<RealResource>(std::move(dominance));
    for (int component = 0; component < 2; ++component) {
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            component == 0 ? std::unique_ptr<CostFunction<RealResource>>(
                                 std::make_unique<ValueCostFunction<RealResource>>())
                           : std::unique_ptr<CostFunction<RealResource>>(
                                 std::make_unique<TrivialCostFunction<RealResource>>()),
            std::make_unique<ValueDominanceFunction<RealResource>>());
    }
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    return graph;
}

/// @brief The same, with a dominance declaring @p tolerance (none: no promise), counting into
///        @p counts.
inline std::unique_ptr<ResourceGraph<RealResource>> two_component_graph(
    std::optional<double> tolerance, std::shared_ptr<Counts> counts = nullptr) {
    return two_component_graph(
        std::make_unique<DeclaringDominance<RealResource>>(tolerance, std::move(counts)));
}

/// @brief The labels' ids, in the container's order.
inline std::vector<size_t> ids(const CostOrdered& container) {
    std::vector<size_t> all;
    for (const auto* label : container.get_labels()) {
        all.push_back(label->id);
    }
    return all;
}

/// @brief How many dominance tests @p container makes to tell whether a label of cost 2.5 and time
///        0 is dominated, after storing five labels of @p graph, costs 1 to 5 and times 5 to 1:
///        two (the labels of cost 1 and 2) with a promise in its direction, five without one.
template <typename ContainerType>
size_t tests_to_check(ContainerType& container, ResourceGraph<RealResource>& graph, Counts& counts,
                      std::vector<std::unique_ptr<Label<Composed>>>* labels) {
    for (size_t id = 0; id < 5; ++id) {
        labels->push_back(
            make_label(graph, id, static_cast<double>(id + 1), static_cast<double>(5 - id)));
        container.add_label(labels->back().get());
    }
    labels->push_back(make_label(graph, 9, 2.5, 0.0));
    counts.tests = 0;
    EXPECT_FALSE(container.is_dominated(*labels->back()));
    return counts.tests;
}

/// @brief A cost that is not the value (half of it): @c CostKind::Unspecified, so its component
///        tells nothing of the cost order.
class HalfCost : public Clonable<HalfCost, CostFunction<RealResource>> {
    public:
        [[nodiscard]] double get_cost(const RealResource& resource) const override {
            return resource.get_value() / 2.0;
        }
};

/// @brief Value dominance within @p slack (`a <= b + slack`), promising the value order
///        @p declared: a wrong promise when @p declared is below @p slack.
class SlackValueDominance : public Clonable<SlackValueDominance, DominanceFunction<RealResource>> {
    public:
        explicit SlackValueDominance(double slack = 0.0, std::optional<double> declared = 0.0)
            : slack_(slack), declared_(declared) {}
        [[nodiscard]] bool check_dominance(const RealResource& lhs,
                                           const RealResource& rhs) override {
            return lhs.get_value() <= rhs.get_value() + slack_;
        }
        bool fast_check_dominance(const RealResource& lhs, const RealResource& rhs,
                                  double delta) override {
            return lhs.get_value() <= rhs.get_value() + slack_ + delta;
        }
        [[nodiscard]] std::optional<double> value_order_tolerance() const override {
            return declared_;
        }

    private:
        double slack_;
        std::optional<double> declared_;
};

/// @brief Adds a real component to @p graph: added along arcs, with @p cost and @p dominance.
inline void add_component(ResourceGraph<RealResource>& graph,
                          std::unique_ptr<CostFunction<RealResource>> cost,
                          std::unique_ptr<DominanceFunction<RealResource>> dominance) {
    graph.add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                     std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                     std::move(cost),
                                     std::move(dominance));
}

/// @brief A value cost under value dominance.
inline void add_value_cost(ResourceGraph<RealResource>& graph) {
    add_component(graph,
                  std::make_unique<ValueCostFunction<RealResource>>(),
                  std::make_unique<ValueDominanceFunction<RealResource>>());
}

/// @brief A zero cost under value dominance (a clock, a load).
inline void add_zero_cost(ResourceGraph<RealResource>& graph) {
    add_component(graph,
                  std::make_unique<TrivialCostFunction<RealResource>>(),
                  std::make_unique<ValueDominanceFunction<RealResource>>());
}

/// @brief An empty graph whose cost sums every component's (@c CompositionCostFunction), with
///        the default dominance.
inline std::unique_ptr<ResourceGraph<RealResource>> summing_graph() {
    return std::make_unique<ResourceGraph<RealResource>>(
        std::make_unique<CompositionExtensionFunction<RealResource>>(),
        std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
        std::make_unique<CompositionCostFunction<RealResource>>(),
        std::make_unique<CompositionDominanceFunction<RealResource>>());
}

/// @brief A promise forward and one backward.
using Promises = std::pair<std::optional<double>, std::optional<double>>;

/// @brief @p graph's promises, read off a node's resource as the container reads them.
inline Promises promises(ResourceGraph<RealResource>& graph) {
    const auto resource = graph.get_resource_factory().create_resource(/*node_id=*/0);
    return {resource->cost_order_tolerance(), resource->back_cost_order_tolerance()};
}

/// @brief A promise of @p tolerance in both directions.
inline Promises both(std::optional<double> tolerance) {
    return {tolerance, tolerance};
}

/// @brief Machine epsilon, the tolerance of value dominance on a real value (value_leq).
constexpr double kEpsilon = std::numeric_limits<double>::epsilon();

}  // namespace cost_order_test

/// @brief The labels are kept cheapest first, labels of equal cost in the order they were stored;
///        a label is erased by the label, among those of its cost; the costliest is removed above
///        a cost, the last stored of equal cost first; clear empties the container.
TEST(CostOrderedLabelList, KeepsItsLabelsInCostOrderAndErasesByTheLabel) {
    namespace cot = cost_order_test;
    auto graph = cot::two_component_graph(cot::kTolerance);
    std::vector<std::unique_ptr<Label<cot::Composed>>> labels;
    // Costs 3, 1, 2, 1, 3; times decreasing with the cost so that none dominates another.
    const std::vector<std::pair<double, double>> values = {{3.0, 1.0},
                                                           {1.0, 5.0},
                                                           {2.0, 3.0},
                                                           {1.0, 6.0},
                                                           {3.0, 2.0}};
    cot::CostOrdered container;
    static_assert(cot::CostOrdered::orders_by_cost);
    static_assert(parts::declares_own_clear_v<cot::CostOrdered>);
    for (size_t id = 0; id < values.size(); ++id) {
        labels.push_back(cot::make_label(*graph, id, values[id].first, values[id].second));
        container.add_label(labels.back().get());
    }
    EXPECT_EQ(cot::ids(container), (std::vector<size_t>{1, 3, 2, 0, 4}));
    EXPECT_EQ(container.get_labels().size(), 5U);
    EXPECT_EQ(container.get_labels().front()->id, 1U);

    container.erase_label(cot::Stored{.label = labels[3].get(), .position = {}});
    EXPECT_EQ(cot::ids(container), (std::vector<size_t>{1, 2, 0, 4}));

    EXPECT_FALSE(container.remove_costliest_above(3.0));
    EXPECT_TRUE(container.remove_costliest_above(2.5));
    EXPECT_TRUE(labels[4]->dominated);
    EXPECT_EQ(cot::ids(container), (std::vector<size_t>{1, 2, 0}));

    container.clear();
    EXPECT_TRUE(container.get_labels().empty());
    EXPECT_TRUE(cot::CostOrdered::LabelView().empty());
}

/// @brief With a promise, a label is tested only against the stored labels whose costs allow a
///        dominance; without one, against every stored label, as LabelList does.
TEST(CostOrderedLabelList, TestsOnlyThePairsTheCostsAllow) {
    namespace cot = cost_order_test;
    for (const bool promise : {true, false}) {
        SCOPED_TRACE(promise ? "a promise" : "no promise");
        auto counts = std::make_shared<cot::Counts>();
        auto graph = cot::two_component_graph(
            promise ? std::optional<double>(cot::kTolerance) : std::nullopt,
            counts);
        std::vector<std::unique_ptr<Label<cot::Composed>>> labels;
        cot::CostOrdered container;
        // Costs 1 to 5, times 5 to 1: none dominates another.
        for (size_t id = 0; id < 5; ++id) {
            labels.push_back(cot::make_label(*graph,
                                             id,
                                             static_cast<double>(id + 1),
                                             static_cast<double>(5 - id)));
            container.add_label(labels.back().get());
        }
        // Cost 2.5, time 0: dominates the labels of cost 3 to 5; dominated by none.
        auto probe = cot::make_label(*graph, 9, 2.5, 0.0);
        counts->tests = 0;
        EXPECT_FALSE(container.is_dominated(*probe));
        const size_t tested_by_is_dominated = counts->tests;
        counts->tests = 0;
        EXPECT_EQ(container.remove_dominated_labels(*probe), 3U);
        const size_t tested_by_remove = counts->tests;
        EXPECT_EQ(cot::ids(container), (std::vector<size_t>{0, 1}));
        if constexpr (!kVerifyCostOrder) {
            // A promise: only the labels of cost 1 and 2 can dominate it, only those of cost 3 to
            // 5 can be dominated. No promise: every label, both times.
            EXPECT_EQ(tested_by_is_dominated, promise ? 2U : 5U);
            EXPECT_EQ(tested_by_remove, promise ? 3U : 5U);
            EXPECT_EQ(counts->outside, 0U);
        }
    }
}

/// @brief A negative tolerance is refused when the first label is stored, naming the dominance
///        function.
TEST(CostOrderedLabelList, ANegativeToleranceIsRefused) {
    namespace cot = cost_order_test;
    auto graph = cot::two_component_graph(-1.0);
    auto label = cot::make_label(*graph, 0, 1.0, 1.0);
    cot::CostOrdered container;
    try {
        container.add_label(label.get());
        ADD_FAILURE() << "a negative tolerance must be refused";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string(e.what()).find("DeclaringDominance"), std::string::npos) << e.what();
    }
}

/// @brief With a dominance function that declares the cost order, every single-direction search
///        on CostOrderedLabelList does what it does on LabelList: the same extensions in the same
///        order, the same pool counts and status, and the same labels and solutions, under every
///        parity setting (quota, phases, memory pressure, which erases the labels stored aside,
///        dominated solutions, early stops, repeat solves).
TEST(CostOrderedLabelList, SearchesMatchLabelListWithAPromise) {
    namespace cot = cost_order_test;
    cot::expect_same_searches([] { return cot::declaring_graph<RealResource>(cot::kTolerance); });
}

/// @brief Without a promise (a dominance function that declares none and infers none), the same.
TEST(CostOrderedLabelList, SearchesMatchLabelListWithoutAPromise) {
    namespace cot = cost_order_test;
    cot::expect_same_searches([] { return cot::declaring_graph<RealResource>(std::nullopt); });
}

/// @brief With a promise, a search on CostOrderedLabelList makes fewer dominance tests than on
///        LabelList, and none outside the tolerance; with or without one, the same solutions.
///
/// Without a promise both containers test every stored label, but a scan stops at the first label
/// that dominates, and they scan in different orders (by cost, in storing order), so the counts
/// differ a little either way: the unit test above checks that every label is a candidate.
TEST(CostOrderedLabelList, APromiseSavesDominanceTests) {
    namespace cot = cost_order_test;
    size_t saved = 0;
    for (const auto& config : cot::random_configs()) {
        SCOPED_TRACE(test_util::describe(config));
        for (const bool promise : {true, false}) {
            const std::optional<double> tolerance =
                promise ? std::optional<double>(cot::kTolerance) : std::nullopt;
            auto on_list = std::make_shared<cot::Counts>();
            auto on_order = std::make_shared<cot::Counts>();
            auto list_graph =
                test_util::build_instance(config,
                                          cot::declaring_graph<RealResource>(tolerance, on_list))
                    .graph;
            auto order_graph =
                test_util::build_instance(config,
                                          cot::declaring_graph<RealResource>(tolerance, on_order))
                    .graph;
            const auto expected =
                list_graph->solve<strategy::Pulling, cot::List>(AlgorithmParams<cot::List>(),
                                                                /*preprocess=*/false);
            const auto actual = order_graph->solve<strategy::Pulling, cot::CostOrdered>(
                AlgorithmParams<cot::CostOrdered>(),
                /*preprocess=*/false);
            EXPECT_EQ(cot::solution_set(actual), cot::solution_set(expected));
            if constexpr (!kVerifyCostOrder) {  // the verification tests the skipped pairs too
                if (promise) {
                    EXPECT_LE(on_order->tests, on_list->tests);
                    EXPECT_EQ(on_order->outside, 0U);
                    saved += on_list->tests - on_order->tests;
                }
            }
        }
    }
    if constexpr (!kVerifyCostOrder) {
        EXPECT_GT(saved, 0U) << "a promise should save tests on these instances";
    }
}

/// @brief Bidirectional searches, and the ng model in every direction, find the same solutions on
///        both containers, with a promise.
TEST(CostOrderedLabelList, BidirectionalAndNgSearchesMatchLabelList) {
    namespace cot = cost_order_test;
    for (const auto& config : cot::random_configs()) {
        SCOPED_TRACE(test_util::describe(config));
        const auto built =
            test_util::build_instance(config, cot::declaring_graph<RealResource>(cot::kTolerance));
        for (const bool use_half_way : {true, false}) {
            const auto set = [&](auto& params) {
                params.direction = SearchDirection::Bidirectional;
                params.critical_resource_index = built.clock_index;
                params.half_way_point =
                    use_half_way && built.clock_is_usable ? built.clock_upper_bound / 2.0 : 0.0;
            };
            cot::expect_same_solve<strategy::Simple>(*built.graph, set);
            cot::expect_same_solve<strategy::Pulling>(*built.graph, set);
        }

        auto ng_config = config;
        ng_config.with_ng_path = true;
        const auto ng = test_util::build_ng_instance(
            ng_config,
            cot::declaring_graph<RealResource, SizeTBitsetResource>(cot::kTolerance));
        for (const auto direction : {SearchDirection::Forward,
                                     SearchDirection::Backward,
                                     SearchDirection::Bidirectional}) {
            const auto set = [&](auto& params) {
                params.direction = direction;
                params.critical_resource_index = ng.clock_index;
                params.half_way_point = ng.clock_is_usable ? ng.clock_upper_bound / 2.0 : 0.0;
            };
            cot::expect_same_solve<strategy::Simple>(*ng.graph, set);
        }
    }
}

/// @brief A dominance function that declares a cost order it does not keep: the verification
///        (RCSPP_VERIFY_COST_ORDER, the Debug builds) throws, naming it; without it, the search
///        runs.
TEST(CostOrderedLabelList, AWrongPromiseIsCaughtByTheVerification) {
    namespace cot = cost_order_test;
    size_t thrown = 0;
    for (const auto& config : cot::random_configs()) {
        auto graph =
            test_util::build_instance(
                config,
                cot::graph_with<RealResource>(std::unique_ptr<DominanceFunction<cot::Composed>>(
                    std::make_unique<cot::LooseDominance<RealResource>>())))
                .graph;
        try {
            (void)graph->solve<strategy::Simple, cot::CostOrdered>(
                AlgorithmParams<cot::CostOrdered>(),
                /*preprocess=*/false);
        } catch (const std::logic_error& e) {
            ++thrown;
            EXPECT_NE(std::string(e.what()).find("LooseDominance"), std::string::npos) << e.what();
        }
    }
    if constexpr (kVerifyCostOrder) {
        EXPECT_GT(thrown, 0U) << "the verification should catch the wrong promise";
    } else {
        EXPECT_EQ(thrown, 0U);
    }
}

/// @brief The promise is read in the container's direction (cost_order_tolerance forward,
///        back_cost_order_tolerance backward), and again from the first label stored after clear.
TEST(CostOrderedLabelList, ThePromiseIsReadPerDirectionAndAgainAfterClear) {
    namespace cot = cost_order_test;
    using Backward = CostOrderedLabelList<cot::Composed, BackwardDirection>;
    const auto graph_declaring = [](std::optional<double> forward,
                                    std::optional<double>
                                        backward,
                                    const std::shared_ptr<cot::Counts>& counts) {
        return cot::two_component_graph(
            std::make_unique<cot::DeclaringDominance<RealResource>>(forward, backward, counts));
    };
    // The verification tests the skipped pairs too: the counts are those of a Release build.
    const size_t with = kVerifyCostOrder ? 5U : 2U;
    for (const bool forward_declares : {true, false}) {
        SCOPED_TRACE(forward_declares ? "forward declares" : "backward declares");
        auto counts = std::make_shared<cot::Counts>();
        const std::optional<double> declared(cot::kTolerance);
        auto graph = forward_declares ? graph_declaring(declared, std::nullopt, counts)
                                      : graph_declaring(std::nullopt, declared, counts);
        std::vector<std::unique_ptr<Label<cot::Composed>>> labels;  // after its graph
        cot::CostOrdered forward;
        Backward backward;
        EXPECT_EQ(cot::tests_to_check(forward, *graph, *counts, &labels),
                  forward_declares ? with : 5U);
        EXPECT_EQ(cot::tests_to_check(backward, *graph, *counts, &labels),
                  forward_declares ? 5U : with);
    }

    auto counts = std::make_shared<cot::Counts>();
    auto declaring = cot::two_component_graph(cot::kTolerance, counts);
    auto silent = cot::two_component_graph(std::nullopt, counts);
    std::vector<std::unique_ptr<Label<cot::Composed>>> labels;  // after their graphs
    cot::CostOrdered container;
    EXPECT_EQ(cot::tests_to_check(container, *declaring, *counts, &labels), with);
    container.clear();
    EXPECT_EQ(cot::tests_to_check(container, *silent, *counts, &labels), 5U);
    container.clear();
    EXPECT_EQ(cot::tests_to_check(container, *declaring, *counts, &labels), with);
}

/// @brief The scans include the labels exactly at cost + eps and cost - eps, and the labels of
///        equal cost when eps is 0.
TEST(CostOrderedLabelList, TheBoundsAreInclusive) {
    namespace cot = cost_order_test;
    // Costs within 0.5 may dominate, and the promise says so: a true one, at its bound.
    auto loose = cot::two_component_graph(
        std::make_unique<cot::LooseDominance<RealResource>>(/*slack=*/0.5, /*declared=*/0.5));
    {
        cot::CostOrdered container;
        auto stored = cot::make_label(*loose, 0, 2.5, 0.0);
        auto probe = cot::make_label(*loose, 1, 2.0, 1.0);
        container.add_label(stored.get());
        EXPECT_TRUE(container.is_dominated(*probe)) << "a label at cost + eps can dominate";
    }
    {
        cot::CostOrdered container;
        auto stored = cot::make_label(*loose, 0, 1.5, 1.0);
        auto probe = cot::make_label(*loose, 1, 2.0, 0.0);
        container.add_label(stored.get());
        EXPECT_EQ(container.remove_dominated_labels(*probe), 1U)
            << "a label at cost - eps can be dominated";
    }
    auto exact = cot::two_component_graph(0.0);
    {
        cot::CostOrdered container;
        auto stored = cot::make_label(*exact, 0, 2.0, 1.0);
        auto later = cot::make_label(*exact, 1, 2.0, 2.0);
        auto earlier = cot::make_label(*exact, 2, 2.0, 0.0);
        container.add_label(stored.get());
        EXPECT_TRUE(container.is_dominated(*later)) << "eps 0: a label of equal cost dominates";
        EXPECT_EQ(container.remove_dominated_labels(*earlier), 1U)
            << "eps 0: a label of equal cost is dominated";
    }
}

/// @brief With a promise, the capped (KBest) and relaxed (Relaxed) admission rules, which reach
///        remove_costliest_above and the masked tests, and the heuristics, which store nothing at
///        the nodes, find on CostOrderedLabelList what they find on LabelList.
TEST(CostOrderedLabelList, CappedRelaxedAndHeuristicSearchesMatchLabelList) {
    namespace cot = cost_order_test;
    for (const auto& config : cot::random_configs()) {
        SCOPED_TRACE(test_util::describe(config));
        const auto built =
            test_util::build_instance(config, cot::declaring_graph<RealResource>(cot::kTolerance));
        const auto capped = [](auto& params) {
            params.admission_rule = parts::AdmissionRule::KBest;
            params.max_admitted_labels_per_node = 2;
        };
        const auto relaxed = [](auto& params) {
            params.admission_rule = parts::AdmissionRule::Relaxed;
            params.dominance_ignored_components = {1};
        };
        const auto bounded = [](auto& params) { params.max_iterations = 50; };
        cot::expect_same_solve<strategy::Simple>(*built.graph, capped);
        cot::expect_same_solve<strategy::Pulling>(*built.graph, capped);
        cot::expect_same_solve<strategy::Simple>(*built.graph, relaxed);
        cot::expect_same_solve<strategy::Pulling>(*built.graph, relaxed);
        cot::expect_same_solve<strategy::Greedy>(*built.graph, bounded);
        cot::expect_same_solve<strategy::BoundGuidedGreedy>(*built.graph, bounded);
        cot::expect_same_solve<strategy::Tabu>(*built.graph, bounded);
        cot::expect_same_solve<strategy::ImprovingTabu>(*built.graph, bounded);
        cot::expect_same_solve<strategy::Diversification<strategy::Tabu>>(
            *built.graph,
            [](auto& params) { params.max_iterations = 5; });
    }
}

/// @brief The default model (the graph's default cost, component 0's value, and component-wise
///        dominance) has its promise inferred: machine epsilon for a real cost, 0 for an integer
///        one, in both directions; a model whose costs are all zero, 0.
TEST(CostOrderInference, TheDefaultModelInfersItsCostOrder) {
    namespace cot = cost_order_test;
    {
        ResourceGraph<RealResource> graph;
        presets::add_cost_resource<RealResource>(graph);
        presets::add_window_resource<RealResource>(graph, {{0, {0.0, 10.0}}});
        EXPECT_EQ(cot::promises(graph), cot::both(cot::kEpsilon));
        const auto resource = graph.get_resource_factory().create_resource(/*node_id=*/0);
        EXPECT_NE(resource->cost_order_source(false).find("inferred"), std::string::npos)
            << resource->cost_order_source(false);
    }
    {
        ResourceGraph<IntResource> graph;
        presets::add_cost_resource<IntResource>(graph);
        const auto resource = graph.get_resource_factory().create_resource(/*node_id=*/0);
        EXPECT_EQ(resource->cost_order_tolerance(), std::optional<double>(0.0));
        EXPECT_EQ(resource->back_cost_order_tolerance(), std::optional<double>(0.0));
    }
    {
        ResourceGraph<UIntResource> graph;
        presets::add_cost_resource<UIntResource>(graph);
        const auto resource = graph.get_resource_factory().create_resource(/*node_id=*/0);
        EXPECT_EQ(resource->cost_order_tolerance(), std::optional<double>(0.0));
        EXPECT_EQ(resource->back_cost_order_tolerance(), std::optional<double>(0.0));
    }
    {
        ResourceGraph<RealResource> graph;
        cot::add_zero_cost(graph);
        cot::add_zero_cost(graph);
        EXPECT_EQ(cot::promises(graph), cot::both(0.0));
    }
}

/// @brief The cost component tells the order only when its cost is its value and its dominance
///        orders values: a cost of another kind, or a dominance that declares no value order,
///        gives none; a dominance that swaps backward (a limit) gives none backward.
TEST(CostOrderInference, TheCostComponentMustBeAValueUnderAValueOrder) {
    namespace cot = cost_order_test;
    {
        ResourceGraph<RealResource> graph;
        cot::add_component(graph,
                           std::make_unique<cot::HalfCost>(),
                           std::make_unique<ValueDominanceFunction<RealResource>>());
        EXPECT_EQ(cot::promises(graph), cot::both(std::nullopt));
    }
    {
        ResourceGraph<RealResource> graph;
        cot::add_component(graph,
                           std::make_unique<ValueCostFunction<RealResource>>(),
                           std::make_unique<cot::SlackValueDominance>(0.0, std::nullopt));
        EXPECT_EQ(cot::promises(graph), cot::both(std::nullopt));
    }
    {
        ValueDominanceFunction<RealResource> dominance;
        EXPECT_EQ(dominance.value_order_tolerance(), std::optional<double>(cot::kEpsilon));
        EXPECT_EQ(dominance.back_value_order_tolerance(), std::optional<double>(cot::kEpsilon));
        dominance.set_back_dominance_reversed(true);
        EXPECT_EQ(dominance.value_order_tolerance(), std::optional<double>(cot::kEpsilon));
        EXPECT_EQ(dominance.back_value_order_tolerance(), std::nullopt);
        EXPECT_EQ(TrivialDominanceFunction<RealResource>().value_order_tolerance(), std::nullopt);
    }
    {
        // A value cost whose backward value is a limit (a time window's: add_resource swaps the
        // backward comparison): the promise holds forward only.
        ResourceGraph<RealResource> graph;
        const std::map<size_t, std::pair<double, double>> windows = {{0, {0.0, 10.0}}};
        graph.add_resource<RealResource>(
            std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        EXPECT_EQ(cot::promises(graph), cot::Promises(cot::kEpsilon, std::nullopt));
    }
    {
        // A class derived from ValueDominanceFunction may test otherwise: it promises nothing.
        class Derived : public Clonable<Derived,
                                        ValueDominanceFunction<RealResource>,
                                        DominanceFunction<RealResource>> {};
        EXPECT_EQ(Derived().value_order_tolerance(), std::nullopt);
    }
}

/// @brief A cost that sums the components' (@c CompositionCostFunction) keeps the tolerance of
///        its one nonzero cost among zero costs, and promises nothing once two are nonzero (the
///        sum's rounding could break the promise) or one tells nothing.
TEST(CostOrderInference, ASumKeepsTheToleranceOfItsOneNonzeroCost) {
    namespace cot = cost_order_test;
    auto graph = cot::summing_graph();
    cot::add_zero_cost(*graph);
    EXPECT_EQ(cot::promises(*graph), cot::both(0.0));
    cot::add_value_cost(*graph);
    EXPECT_EQ(cot::promises(*graph), cot::both(cot::kEpsilon));
    cot::add_value_cost(*graph);
    EXPECT_EQ(cot::promises(*graph), cot::both(std::nullopt));

    auto untold = cot::summing_graph();
    cot::add_value_cost(*untold);
    // A zero cost needs no order: whatever its dominance, it adds 0 to both labels.
    cot::add_component(*untold,
                       std::make_unique<TrivialCostFunction<RealResource>>(),
                       std::make_unique<cot::SlackValueDominance>(0.0, std::nullopt));
    EXPECT_EQ(cot::promises(*untold), cot::both(cot::kEpsilon));
    cot::add_component(*untold,
                       std::make_unique<cot::HalfCost>(),
                       std::make_unique<ValueDominanceFunction<RealResource>>());
    EXPECT_EQ(cot::promises(*untold), cot::both(std::nullopt));

    // A class derived from either cost function may compute its cost otherwise (a weight, a sign):
    // it gives nothing.
    class DerivedSum : public Clonable<DerivedSum,
                                       CompositionCostFunction<RealResource>,
                                       CostFunction<cot::Composed>> {};
    class DerivedComponent : public ComponentCostFunction<0, RealResource> {
        public:
            DerivedComponent() : ComponentCostFunction<0, RealResource>(0) {}
            [[nodiscard]] auto clone() const
                -> std::unique_ptr<CostFunction<cot::Composed>> override {
                return std::make_unique<DerivedComponent>(*this);
            }
    };
    const auto promises_with = [](std::unique_ptr<CostFunction<cot::Composed>> cost) {
        ResourceGraph<RealResource> derived(
            std::make_unique<CompositionExtensionFunction<RealResource>>(),
            std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
            std::move(cost),
            std::make_unique<CompositionDominanceFunction<RealResource>>());
        cot::add_value_cost(derived);
        return cot::promises(derived);
    };
    EXPECT_EQ(promises_with(std::make_unique<DerivedSum>()), cot::both(std::nullopt));
    EXPECT_EQ(promises_with(std::make_unique<DerivedComponent>()), cot::both(std::nullopt));
}

/// @brief A declaration comes first; a function derived from CompositionDominanceFunction infers
///        nothing (it may test otherwise) unless it declares requires_component_dominance; the
///        messages say whether the promise was declared or inferred.
TEST(CostOrderInference, ADeclarationComesFirstAndADerivedFunctionInfersOnlyIfItSays) {
    namespace cot = cost_order_test;
    auto declared = cot::two_component_graph(cot::kTolerance);
    EXPECT_EQ(cot::promises(*declared), cot::both(cot::kTolerance));
    const auto source =
        declared->get_resource_factory().create_resource(/*node_id=*/0)->cost_order_source(false);
    EXPECT_NE(source.find("DeclaringDominance"), std::string::npos) << source;
    EXPECT_EQ(source.find("inferred"), std::string::npos) << source;

    auto silent = cot::two_component_graph(std::nullopt);
    EXPECT_EQ(cot::promises(*silent), cot::both(std::nullopt));

    auto inferring = std::make_unique<cot::DeclaringDominance<RealResource>>(std::nullopt);
    inferring->infer_from_components();
    auto told = cot::two_component_graph(
        std::unique_ptr<DominanceFunction<cot::Composed>>(std::move(inferring)));
    EXPECT_EQ(cot::promises(*told), cot::both(cot::kEpsilon));
}

/// @brief With the default model's inferred promise, every single-direction search does on
///        CostOrderedLabelList what it does on LabelList (as with a declared promise), and the
///        verification (Debug) finds no pair the inference wrongly ruled out.
TEST(CostOrderInference, SearchesMatchLabelListWithAnInferredPromise) {
    namespace cot = cost_order_test;
    cot::expect_same_searches([] { return std::make_unique<ResourceGraph<RealResource>>(); });
}

/// @brief The inferred promise saves dominance tests, with the same solutions.
TEST(CostOrderInference, AnInferredPromiseSavesDominanceTests) {
    namespace cot = cost_order_test;
    const auto inferring_graph = [](std::shared_ptr<cot::Counts> counts) {
        auto dominance = std::make_unique<cot::DeclaringDominance<RealResource>>(std::nullopt,
                                                                                 std::move(counts));
        dominance->infer_from_components();
        return cot::graph_with<RealResource>(
            std::unique_ptr<DominanceFunction<cot::Composed>>(std::move(dominance)));
    };
    size_t saved = 0;
    for (const auto& config : cot::random_configs()) {
        SCOPED_TRACE(test_util::describe(config));
        auto on_list = std::make_shared<cot::Counts>();
        auto on_order = std::make_shared<cot::Counts>();
        auto list_graph = test_util::build_instance(config, inferring_graph(on_list)).graph;
        auto order_graph = test_util::build_instance(config, inferring_graph(on_order)).graph;
        EXPECT_EQ(cot::promises(*order_graph), cot::both(cot::kEpsilon));
        const auto expected =
            list_graph->solve<strategy::Pulling, cot::List>(AlgorithmParams<cot::List>(),
                                                            /*preprocess=*/false);
        const auto actual = order_graph->solve<strategy::Pulling, cot::CostOrdered>(
            AlgorithmParams<cot::CostOrdered>(),
            /*preprocess=*/false);
        EXPECT_EQ(cot::solution_set(actual), cot::solution_set(expected));
        if constexpr (!kVerifyCostOrder) {  // the verification tests the skipped pairs too
            EXPECT_LE(on_order->tests, on_list->tests);
            saved += on_list->tests - on_order->tests;
        }
    }
    if constexpr (!kVerifyCostOrder) {
        EXPECT_GT(saved, 0U) << "the inferred promise should save tests on these instances";
    }
}

/// @brief A cost component that promises a wrong value order makes a wrong inferred promise,
///        which the verification catches, naming the inference, forward and backward. Without the
///        verification (Release) the search runs and finds the cheapest path.
TEST(CostOrderInference, AWrongComponentPromiseIsCaughtByTheVerification) {
    namespace cot = cost_order_test;
    // Component 0, the graph's cost, dominates within 1 but promises 0; component 1, a time, by
    // value. Two paths reach the sink 3: through 1, cost 1.5 and time 0; through 2, cost 1 and
    // time 5. The first dominates the second (1.5 <= 1 + 1, 0 <= 5) although it costs more, a
    // pair the inferred promise rules out, whichever label is stored first.
    ResourceGraph<RealResource> graph;
    cot::add_component(graph,
                       std::make_unique<ValueCostFunction<RealResource>>(),
                       std::make_unique<cot::SlackValueDominance>(1.0, 0.0));
    cot::add_zero_cost(graph);
    graph.add_node(0, /*source=*/true, /*sink=*/false);
    graph.add_node(1);
    graph.add_node(2);
    graph.add_node(3, /*source=*/false, /*sink=*/true);
    graph.add_arc<RealResource, RealResource>({1.5, 0.0}, 0, 1);
    graph.add_arc<RealResource, RealResource>({0.0, 0.0}, 1, 3);
    graph.add_arc<RealResource, RealResource>({1.0, 5.0}, 0, 2);
    graph.add_arc<RealResource, RealResource>({0.0, 0.0}, 2, 3);
    EXPECT_EQ(cot::promises(graph), cot::both(0.0));
    for (const auto direction : {SearchDirection::Forward, SearchDirection::Backward}) {
        SCOPED_TRACE(to_string(direction));
        AlgorithmParams<cot::CostOrdered> params;
        params.direction = direction;
        if constexpr (kVerifyCostOrder) {
            try {
                (void)graph.solve<strategy::Simple, cot::CostOrdered>(params,
                                                                      /*preprocess=*/false);
                ADD_FAILURE() << "the verification should catch the wrong component promise";
            } catch (const std::logic_error& e) {
                EXPECT_NE(std::string(e.what()).find("inferred"), std::string::npos) << e.what();
                EXPECT_NE(std::string(e.what()).find("CompositionDominanceFunction"),
                          std::string::npos)
                    << e.what();
            }
        } else {
            // Without the verification the search runs, keeping a label dominance would remove.
            const auto result =
                graph.solve<strategy::Simple, cot::CostOrdered>(params, /*preprocess=*/false);
            ASSERT_FALSE(result.solutions.empty());
            EXPECT_DOUBLE_EQ(result.solutions.front().cost, 1.0);
        }
    }
}

/// @brief CostOrderedLabelList is the default container, of the algorithms a graph builds and of
///        its solves, which find what a LabelList search finds.
TEST(CostOrderInference, TheDefaultContainerIsCostOrdered) {
    namespace cot = cost_order_test;
    static_assert(std::is_same_v<DefaultLabelContainer<cot::Composed>, cot::CostOrdered>);
    static_assert(std::is_same_v<DefaultLabelContainer<cot::Composed, BackwardDirection>,
                                 CostOrderedLabelList<cot::Composed, BackwardDirection>>);
    static_assert(
        std::is_same_v<Algorithm<cot::Composed>, Algorithm<cot::Composed, cot::CostOrdered>>);
    static_assert(
        std::is_same_v<decltype(std::declval<ResourceGraph<RealResource>&>().create_algorithm()),
                       std::unique_ptr<Algorithm<cot::Composed, cot::CostOrdered>>>);
    for (const auto& config : cot::random_configs()) {
        SCOPED_TRACE(test_util::describe(config));
        auto by_default = test_util::build_instance(config).graph;
        auto on_list = test_util::build_instance(config).graph;
        const auto actual =
            by_default->solve(AlgorithmBaseParams{}, cot::kInf, /*preprocess=*/false);
        const auto expected =
            on_list->solve<strategy::Simple, cot::List>(AlgorithmParams<cot::List>(),
                                                        /*preprocess=*/false);
        EXPECT_EQ(actual.status, expected.status);
        EXPECT_EQ(cot::solution_set(actual), cot::solution_set(expected));
    }
}

/// @brief With the inferred promise, bidirectional searches, and the ng model in every direction,
///        find on CostOrderedLabelList what they find on LabelList.
TEST(CostOrderInference, BidirectionalAndNgSearchesMatchLabelListWithAnInferredPromise) {
    namespace cot = cost_order_test;
    for (const auto& config : cot::random_configs()) {
        SCOPED_TRACE(test_util::describe(config));
        const auto built = test_util::build_instance(config);
        ASSERT_EQ(cot::promises(*built.graph), cot::both(cot::kEpsilon));
        const auto set = [&](auto& params) {
            params.direction = SearchDirection::Bidirectional;
            params.critical_resource_index = built.clock_index;
            params.half_way_point = built.clock_is_usable ? built.clock_upper_bound / 2.0 : 0.0;
        };
        cot::expect_same_solve<strategy::Simple>(*built.graph, set);
        cot::expect_same_solve<strategy::Pulling>(*built.graph, set);

        auto ng_config = config;
        ng_config.with_ng_path = true;
        const auto ng = test_util::build_ng_instance(ng_config);
        for (const auto direction : {SearchDirection::Forward,
                                     SearchDirection::Backward,
                                     SearchDirection::Bidirectional}) {
            const auto ng_set = [&](auto& params) {
                params.direction = direction;
                params.critical_resource_index = ng.clock_index;
                params.half_way_point = ng.clock_is_usable ? ng.clock_upper_bound / 2.0 : 0.0;
            };
            cot::expect_same_solve<strategy::Simple>(*ng.graph, ng_set);
        }
    }
}
