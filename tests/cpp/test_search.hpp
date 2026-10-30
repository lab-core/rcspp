// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The labelling kernel's search, driven by hand: how one extension ends, what it keeps and
// releases, what the take rule's verdicts do, and a backward search with buckets. Also the two
// Algorithm members the kernel calls: the extension count and a backward path's recording.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/labelling.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace search_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;
using Buckets = LabelBuckets<RealResource, RealResource, Composed>;
using parts::ExtensionOutcome;
using Stored = parts::StoredLabel<Composed>;

/// @brief The line 0 -> 1 -> 2 (arcs 0 and 1), costing 1 an arc, with a cost of at most
///        @p max_cost; sorted, with its adjacency built.
inline std::unique_ptr<ResourceGraph<RealResource>> line(double max_cost) {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    if (max_cost == std::numeric_limits<double>::infinity()) {
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
    } else {
        graph->add_resource<RealResource>(
            std::make_unique<AdditionExtensionFunction<RealResource>>(),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, max_cost),
            std::make_unique<ValueCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
    }
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>({1.0}, 0, 1, 1.0);
    graph->add_arc<RealResource>({1.0}, 1, 2, 1.0);
    graph->sort_nodes();
    graph->build_csr();
    return graph;
}

/// @brief Records every outcome it is told of.
struct RecordingObserver {
        std::vector<ExtensionOutcome> outcomes;

        template <typename ResourceType>
        void on_extension(const Label<ResourceType>& /*label*/, const Arc<ResourceType>& /*arc*/,
                          ExtensionOutcome outcome) {
            outcomes.push_back(outcome);
        }
};

/// @brief A schedule that records every label pushed to it, and hands none out.
struct RecordingSchedule {
        std::vector<Stored> pushed;

        void initialize(detail::SearchContext<Composed>& /*ctx*/) {}
        void push(const Stored& stored) { pushed.push_back(stored); }
        [[nodiscard]] size_t size() const { return 0; }
        Stored next(detail::SearchContext<Composed>& /*ctx*/) { return {}; }
        void prepare_next_phase(detail::SearchContext<Composed>& /*ctx*/) {}
        void trim(detail::SearchContext<Composed>& /*ctx*/, size_t /*limit*/) {}
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<Composed>& /*ctx*/,
                                  const EraseCallable& /*erase*/) {}
        void clear() {}
};

static_assert(parts::LabelSchedule<RecordingSchedule, Composed>);
static_assert(parts::Observer<RecordingObserver, Composed>);

/// @brief A graph, a pool and a search of @p Direction on @p ContainerType, with the pushed labels
///        and the outcomes recorded.
template <typename Direction, typename ContainerType>
class Fixture {
    public:
        using SearchType =
            detail::Search<Composed, Direction, ContainerType,
                           detail::Preset<RecordingSchedule, parts::DominanceAdmission,
                                          parts::KeepAtNodes, RecordingObserver>>;

        explicit Fixture(double max_cost, const ContainerType& prototype)
            : graph_(line(max_cost)),
              pool_(std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory())) {
            ctx_.pool = &pool_;
            search_.reset(graph_->get_number_of_nodes(), prototype);
            search_.create_start_labels(*graph_, &pool_);
        }

        /// @brief Extends @p label along arc @p arc_id.
        ExtensionOutcome extend(Label<Composed>* label, size_t arc_id) {
            return search_.extend_along(label, graph_->get_arc(arc_id), ctx_);
        }

        /// @brief The number of labels stored at node @p node_id.
        size_t stored_at(size_t node_id) const {
            return search_.labels.labels_at(graph_->get_node(node_id)->pos()).size();
        }

        /// @brief The labels the pool has handed out and not had back.
        [[nodiscard]] size_t in_use() const {
            return pool_.get_nb_total_labels() - pool_.get_nb_available_labels();
        }

        [[nodiscard]] SearchType& search() { return search_; }
        [[nodiscard]] LabelPool<Composed>& pool() { return pool_; }
        [[nodiscard]] const std::vector<Stored>& pushed() const { return search_.schedule.pushed; }
        [[nodiscard]] const std::vector<ExtensionOutcome>& outcomes() const {
            return search_.observer.outcomes;
        }
        [[nodiscard]] const Graph<Composed>& graph() const { return *graph_; }

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        LabelPool<Composed> pool_;
        detail::SearchContext<Composed> ctx_;
        SearchType search_;
};

/// @brief Simple, with the two Algorithm members the kernel calls made public, and a way in to
///        set up a solve.
class Exposed : public test_util::Labelling<strategy::Simple, Composed, List> {
        using Base = test_util::Labelling<strategy::Simple, Composed, List>;

    public:
        using Base::Base;
        using Base::count_extension;
        using Base::record_backward_solution;

        void start(const Graph<Composed>* graph) {
            Algorithm<Composed, List>::initialize(graph, kInfinity);
        }
        [[nodiscard]] size_t solution_count() const { return solutions_.size(); }
        [[nodiscard]] const Solution& only_solution() const { return *solutions_.begin(); }

    private:
        static constexpr double kInfinity = std::numeric_limits<double>::infinity();
};

}  // namespace search_test

/// @brief Each outcome of an extension, what it stores and pushes, and what it releases at once.
TEST(Search, ExtendAlongReportsEachOutcomeAndReleasesWhatItRejects) {
    namespace st = search_test;
    st::Fixture<ForwardDirection, st::List> fixture(1.5, st::List{});
    ASSERT_EQ(fixture.pushed().size(), 1U);
    auto* start = fixture.pushed().front().label;
    EXPECT_EQ(fixture.stored_at(0), 1U);

    // 0 -> 1 costs 1: kept, stored, pushed and pinned to the start label.
    EXPECT_EQ(fixture.extend(start, 0), st::ExtensionOutcome::Accepted);
    ASSERT_EQ(fixture.pushed().size(), 2U);
    auto* at_one = fixture.pushed().back().label;
    EXPECT_EQ(at_one->prev_label, start);
    EXPECT_EQ(start->ref_count, 1U);
    EXPECT_EQ(fixture.stored_at(1), 1U);

    // The same extension again is dominated by the label just stored: released, not pushed.
    const size_t in_use = fixture.in_use();
    EXPECT_EQ(fixture.extend(start, 0), st::ExtensionOutcome::Dominated);
    EXPECT_EQ(fixture.pushed().size(), 2U);
    EXPECT_EQ(fixture.in_use(), in_use);
    EXPECT_EQ(start->ref_count, 1U);
    EXPECT_EQ(fixture.stored_at(1), 1U);

    // 1 -> 2 brings the cost to 2, above 1.5: infeasible, released.
    EXPECT_EQ(fixture.extend(at_one, 1), st::ExtensionOutcome::Infeasible);
    EXPECT_EQ(fixture.pushed().size(), 2U);
    EXPECT_EQ(fixture.in_use(), in_use);
    EXPECT_EQ(fixture.stored_at(2), 0U);

    EXPECT_EQ(fixture.outcomes(),
              (std::vector<st::ExtensionOutcome>{st::ExtensionOutcome::Accepted,
                                                 st::ExtensionOutcome::Dominated,
                                                 st::ExtensionOutcome::Infeasible}));
    EXPECT_TRUE(fixture.pool().check_ref_count_consistency());
}

/// @brief What the search does with a label the take rule does not extend.
TEST(Search, ApplyCarriesOutEachVerdict) {
    namespace st = search_test;
    using detail::Verdict;
    st::Fixture<ForwardDirection, st::List> fixture(std::numeric_limits<double>::infinity(),
                                                    st::List{});
    auto* start = fixture.pushed().front().label;
    ASSERT_EQ(fixture.extend(start, 0), st::ExtensionOutcome::Accepted);
    const auto kept = fixture.pushed().back();
    const size_t available = fixture.pool().get_nb_available_labels();

    for (const Verdict verdict : {Verdict::Extend, Verdict::Keep}) {
        fixture.search().apply(kept, verdict, &fixture.pool());
        EXPECT_EQ(fixture.stored_at(1), 1U);
        EXPECT_EQ(fixture.pool().get_nb_available_labels(), available);
    }

    fixture.search().apply(kept, Verdict::EraseAndRelease, &fixture.pool());
    EXPECT_EQ(fixture.stored_at(1), 0U);
    EXPECT_EQ(fixture.pool().get_nb_available_labels(), available + 1);
    EXPECT_EQ(start->ref_count, 0U);  // the predecessor is unpinned

    // Release: a label already out of its container (evicted as dominated) is released only.
    ASSERT_EQ(fixture.extend(start, 0), st::ExtensionOutcome::Accepted);
    const auto evicted = fixture.pushed().back();
    fixture.search().erase(evicted.position);
    const size_t before = fixture.pool().get_nb_available_labels();
    fixture.search().apply(evicted, Verdict::Release, &fixture.pool());
    EXPECT_EQ(fixture.pool().get_nb_available_labels(), before + 1);
    EXPECT_EQ(start->ref_count, 0U);
    EXPECT_TRUE(fixture.pool().check_ref_count_consistency());
}

/// @brief A backward search starts at the sinks, keeps its buckets turned backward, and its path
///        reads from its node to the sink.
TEST(Search, ABackwardSearchStartsAtTheSinksWithBucketsTurnedBackward) {
    namespace st = search_test;
    using BackwardBuckets = detail::directed_container_t<st::Buckets, BackwardDirection>;
    static_assert(
        std::is_same_v<BackwardBuckets,
                       LabelBuckets<RealResource, RealResource, st::Composed, BackwardDirection>>);
    const auto prototype =
        detail::make_directed_container<BackwardDirection>(st::Buckets(10, 0, 0));
    st::Fixture<BackwardDirection, BackwardBuckets> fixture(std::numeric_limits<double>::infinity(),
                                                            prototype);
    ASSERT_EQ(fixture.pushed().size(), 1U);
    auto* at_sink = fixture.pushed().front().label;
    EXPECT_EQ(at_sink->get_end_node()->id, 2U);
    EXPECT_TRUE(fixture.search().labels_at_end_nodes(fixture.graph()).empty());  // the sources

    ASSERT_EQ(fixture.extend(at_sink, 1), st::ExtensionOutcome::Accepted);  // head: node 1
    auto* at_one = fixture.pushed().back().label;
    ASSERT_EQ(fixture.extend(at_one, 0), st::ExtensionOutcome::Accepted);  // head: the source
    auto* at_source = fixture.pushed().back().label;
    EXPECT_EQ(at_source->get_end_node()->id, 0U);
    EXPECT_EQ(fixture.search().labels_at_end_nodes(fixture.graph()).size(), 1U);

    std::vector<size_t> arc_ids;
    append_suffix_arc_ids(*at_source, &arc_ids);
    EXPECT_EQ(arc_ids, (std::vector<size_t>{0, 1}));
    EXPECT_TRUE(fixture.pool().check_ref_count_consistency());
}

/// @brief Every extension that drew a label counts; an unreachable one does not.
TEST(Search, CountExtensionCountsTheExtensionsThatDrewALabel) {
    namespace st = search_test;
    auto graph = st::line(std::numeric_limits<double>::infinity());
    st::Exposed algorithm(&graph->get_resource_factory(), AlgorithmParams<st::List>());
    for (const auto outcome : {st::ExtensionOutcome::Unreachable,
                               st::ExtensionOutcome::Infeasible,
                               st::ExtensionOutcome::Dominated,
                               st::ExtensionOutcome::Accepted}) {
        algorithm.count_extension(outcome);
    }
    EXPECT_EQ(algorithm.get_number_of_extended_labels(), 3U);
}

/// @brief A backward label at a source records the path from the source to its sink.
TEST(Search, RecordBackwardSolutionRecordsTheWholeBackwardPath) {
    namespace st = search_test;
    st::Fixture<BackwardDirection, detail::directed_container_t<st::List, BackwardDirection>>
        fixture(std::numeric_limits<double>::infinity(),
                detail::make_directed_container<BackwardDirection>(st::List{}));
    auto* at_sink = fixture.pushed().front().label;
    ASSERT_EQ(fixture.extend(at_sink, 1), st::ExtensionOutcome::Accepted);
    ASSERT_EQ(fixture.extend(fixture.pushed().back().label, 0), st::ExtensionOutcome::Accepted);
    const auto* at_source = fixture.pushed().back().label;

    auto graph = st::line(std::numeric_limits<double>::infinity());
    st::Exposed algorithm(&graph->get_resource_factory(), AlgorithmParams<st::List>());
    algorithm.start(graph.get());
    algorithm.record_backward_solution(*at_source);
    ASSERT_EQ(algorithm.solution_count(), 1U);
    EXPECT_EQ(algorithm.only_solution().path_arc_ids, (std::vector<size_t>{0, 1}));
    EXPECT_EQ(algorithm.only_solution().path_node_ids, (std::vector<size_t>{0, 1, 2}));
    EXPECT_DOUBLE_EQ(algorithm.only_solution().cost, 2.0);
}
