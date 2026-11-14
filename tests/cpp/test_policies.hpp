// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The parts that decide what a node keeps, driven by hand: the admission rule on a node view, the
// node view of both containers in both directions, and the labels kept at their nodes.

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace policies_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;

/// A retention with every member but clear(): it must not satisfy parts::Retention.
struct NoClear {
        static constexpr bool records_at_take = false;
        void reset(size_t /*num_nodes*/, const List& /*prototype*/) {}
        parts::NodeView<List> node(size_t /*pos*/) { return parts::NodeView<List>(&container_); }
        std::list<Label<Composed>*>::iterator store(size_t /*pos*/, Label<Composed>* /*label*/) {
            return {};
        }
        void erase(const std::list<Label<Composed>*>::iterator& /*position*/) {}
        [[nodiscard]] const std::list<Label<Composed>*>& labels_at(size_t /*pos*/) const {
            return none_;
        }
        [[nodiscard]] const std::vector<List>& containers() const { return containers_; }

    private:
        std::list<Label<Composed>*> none_;
        List container_;
        std::vector<List> containers_;
};
using BackwardList = LabelList<Composed, BackwardDirection>;
using Buckets = LabelBuckets<RealResource, RealResource, Composed>;

/// @brief A line 0 -> 1 -> 2 whose resource is the cost (component 0), and a load (component 1)
///        when asked for, and a pool for its labels.
class Fixture {
    public:
        explicit Fixture(bool with_load = false)
            : graph_(std::make_unique<ResourceGraph<RealResource>>()), with_load_(with_load) {
            graph_->add_resource<RealResource>(
                std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
            if (with_load) {
                graph_->add_resource<RealResource>(
                    std::make_unique<AdditionExtensionFunction<RealResource>>(),
                    std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                    std::make_unique<TrivialCostFunction<RealResource>>(),
                    std::make_unique<ValueDominanceFunction<RealResource>>());
            }
            graph_->add_node(0, /*source=*/true, /*sink=*/false);
            graph_->add_node(1);
            graph_->add_node(2, /*source=*/false, /*sink=*/true);
            if (with_load) {
                graph_->add_arc<RealResource, RealResource>({1.0, 1.0}, 0, 1, 1.0);
                graph_->add_arc<RealResource, RealResource>({1.0, 1.0}, 1, 2, 1.0);
            } else {
                graph_->add_arc<RealResource>({1.0}, 0, 1, 1.0);
                graph_->add_arc<RealResource>({1.0}, 1, 2, 1.0);
            }
            graph_->sort_nodes();
            pool_ = std::make_unique<LabelPool<Composed>>(
                std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory()));
        }

        /// @brief A label at node @p node_id whose cost is @p cost, and whose load is @p load
        ///        when the fixture has one.
        Label<Composed>* label(size_t node_id, double cost, double load = 0.0) {
            auto& made = pool_->get_next_label(graph_->get_node(node_id));
            made.get_resource().get_component<RealResource>(0).set_value(cost);
            if (with_load_) {
                made.get_resource().get_component<RealResource>(1).set_value(load);
            }
            made.dominated = false;
            return &made;
        }

        /// @brief The position of node @p node_id.
        [[nodiscard]] size_t pos(size_t node_id) const { return graph_->get_node(node_id)->pos(); }

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        bool with_load_;
        std::unique_ptr<LabelPool<Composed>> pool_;
};

}  // namespace policies_test

/// @brief Under the exact rule a candidate is kept unless a label at its node dominates it, and
///        evicts the labels it dominates; the rule says it is exact.
TEST(Policies, DominanceAdmissionKeepsRejectsAndEvicts) {
    namespace pt = policies_test;
    pt::Fixture f;
    pt::List node;
    parts::NodeView<pt::List> view(&node);
    const parts::DominanceAdmission admission;
    const parts::AdmissionContext context;
    EXPECT_TRUE(parts::DominanceAdmission::is_exact(context));

    auto* dear = f.label(1, 5.0);
    ASSERT_TRUE(admission.admit(*dear, view, context)) << "an empty node keeps it";
    node.add_label(dear);
    EXPECT_FALSE(admission.admit(*f.label(1, 6.0), view, context)) << "dominated by the 5";
    EXPECT_EQ(view.size(), 1U);

    auto* cheap = f.label(1, 2.0);
    ASSERT_TRUE(admission.admit(*cheap, view, context));
    EXPECT_TRUE(dear->dominated) << "the 2 evicts the 5";
    EXPECT_EQ(view.size(), 0U) << "evicted, and not yet stored";
}

/// @brief Under the relaxed rule, labels compare over the components the mask leaves in: one that
///        exact dominance would keep is dropped, and one evicted. A mask that leaves nothing out is
///        exact.
TEST(Policies, RelaxedAdmissionComparesOverTheComponentsLeftIn) {
    namespace pt = policies_test;
    pt::Fixture f(/*with_load=*/true);
    pt::List node;
    parts::NodeView<pt::List> view(&node);
    const parts::DominanceAdmission admission;
    const DominanceMask nothing_left_out(2);
    EXPECT_TRUE(parts::DominanceAdmission::is_exact(
        {.rule = parts::AdmissionRule::Relaxed, .dominance_mask = &nothing_left_out}));
    EXPECT_TRUE(parts::DominanceAdmission::is_exact({.rule = parts::AdmissionRule::Relaxed}));
    DominanceMask load_left_out(2);
    load_left_out.ignore(1);
    const parts::AdmissionContext context{.rule = parts::AdmissionRule::Relaxed,
                                          .dominance_mask = &load_left_out};
    EXPECT_FALSE(parts::DominanceAdmission::is_exact(context));

    auto* dear = f.label(1, 5.0, 1.0);
    ASSERT_TRUE(admission.admit(*dear, view, context));
    node.add_label(dear);
    auto* cheap = f.label(1, 3.0, 4.0);  // exactly, neither dominates the other
    ASSERT_TRUE(admission.admit(*cheap, view, context));
    EXPECT_TRUE(dear->dominated) << "the 3 evicts the 5, the load left out";
    node.add_label(cheap);
    EXPECT_FALSE(admission.admit(*f.label(1, 4.0, 0.0), view, context))
        << "the 3 dominates the 4, though the 4 carries less load";
    EXPECT_EQ(view.size(), 1U);
}

/// @brief Under the k-best rule, a node keeps at most k labels after dominance: at a full node, a
///        candidate takes the place of the costliest when it costs less, and is dropped otherwise;
///        one that evicts by dominance always finds room. Without a limit, the rule is exact.
TEST(Policies, KBestAdmissionKeepsTheCheapestKLabelsAtANode) {
    namespace pt = policies_test;
    pt::Fixture f(/*with_load=*/true);
    pt::List node;
    parts::NodeView<pt::List> view(&node);
    const parts::DominanceAdmission admission;
    EXPECT_TRUE(parts::DominanceAdmission::is_exact({.rule = parts::AdmissionRule::KBest}));
    const parts::AdmissionContext context{.rule = parts::AdmissionRule::KBest,
                                          .max_labels_per_node = 2};
    EXPECT_FALSE(parts::DominanceAdmission::is_exact(context));

    auto* dear = f.label(1, 5.0, 1.0);
    auto* cheap = f.label(1, 3.0, 4.0);
    for (auto* label : {dear, cheap}) {
        ASSERT_TRUE(admission.admit(*label, view, context));
        node.add_label(label);
    }
    auto* middle = f.label(1, 4.0, 2.0);
    ASSERT_TRUE(admission.admit(*middle, view, context))
        << "the node is full, and the 4 beats the 5";
    EXPECT_TRUE(dear->dominated);
    node.add_label(middle);
    EXPECT_FALSE(admission.admit(*f.label(1, 6.0, 0.0), view, context))
        << "the node is full, and the 6 beats neither label";
    EXPECT_EQ(view.size(), 2U);
    EXPECT_FALSE(cheap->dominated);
    EXPECT_FALSE(middle->dominated);

    ASSERT_TRUE(admission.admit(*f.label(1, 1.0, 0.0), view, context)) << "it dominates both";
    EXPECT_TRUE(cheap->dominated);
    EXPECT_TRUE(middle->dominated);
    EXPECT_EQ(view.size(), 0U);
}

/// @brief A node view reads a list or buckets, forward or backward: whether a label is dominated
///        there, and how many it holds.
TEST(Policies, ANodeViewReadsEitherContainerInEitherDirection) {
    namespace pt = policies_test;
    pt::Fixture f;

    pt::List list;
    list.add_label(f.label(1, 3.0));
    const parts::NodeView<pt::List> forward(&list);
    EXPECT_TRUE(forward.is_dominated(*f.label(1, 4.0)));
    EXPECT_FALSE(forward.is_dominated(*f.label(1, 2.0)));
    EXPECT_EQ(forward.size(), 1U);

    pt::BackwardList backward_list;
    backward_list.add_label(f.label(1, 3.0));
    const parts::NodeView<pt::BackwardList> backward(&backward_list);
    EXPECT_TRUE(backward.is_dominated(*f.label(1, 4.0))) << "the cost compares the same way";
    EXPECT_EQ(backward.size(), 1U);

    pt::Buckets buckets(/*bucket_width=*/10,
                        /*bucket_resource_index=*/0,
                        /*sort_resource_index=*/0);
    buckets.add_label(f.label(1, 3.0));
    buckets.add_label(f.label(1, 13.0));
    const parts::NodeView<pt::Buckets> by_bucket(&buckets);
    EXPECT_TRUE(by_bucket.is_dominated(*f.label(1, 14.0)));
    EXPECT_EQ(by_bucket.size(), 2U);
}

/// @brief The labels kept at their nodes: one container per node, a copy of the prototype; a label
///        stored at its node until erased; clear forgets every container.
TEST(Policies, KeepAtNodesStoresAndErasesAtEachNode) {
    namespace pt = policies_test;
    pt::Fixture f;
    static_assert(parts::Retention<parts::KeepAtNodes<pt::List>, pt::Composed, pt::List>);
    static_assert(!parts::Retention<pt::NoClear, pt::Composed, pt::List>,
                  "a retention without clear() does not satisfy the concept");
    static_assert(!parts::KeepAtNodes<pt::List>::records_at_take);

    parts::KeepAtNodes<pt::List> kept;
    kept.reset(3, pt::List{});
    EXPECT_EQ(kept.containers().size(), 3U);

    auto* label = f.label(1, 3.0);
    const auto position = kept.store(f.pos(1), label);
    ASSERT_EQ(kept.labels_at(f.pos(1)).size(), 1U);
    EXPECT_EQ(kept.labels_at(f.pos(1)).front(), label);
    EXPECT_EQ(kept.node(f.pos(1)).size(), 1U);
    EXPECT_TRUE(kept.labels_at(f.pos(0)).empty());

    kept.erase(position);
    EXPECT_TRUE(kept.labels_at(f.pos(1)).empty());

    kept.clear();
    EXPECT_TRUE(kept.containers().empty());
}

/// @brief AdmitAll keeps every label and says it is exact.
TEST(Policies, AdmitAllKeepsEveryLabelAndIsExact) {
    namespace pt = policies_test;
    pt::Fixture f;
    static_assert(parts::Admission<parts::AdmitAll, pt::Composed>);
    parts::KeepAtNodes<pt::List> kept;
    kept.reset(3, pt::List{});
    auto* cheap = f.label(1, 1.0);
    kept.store(f.pos(1), cheap);
    auto view = kept.node(f.pos(1));
    EXPECT_TRUE(parts::AdmitAll{}.admit(*f.label(1, 2.0), view, parts::AdmissionContext{}));
    EXPECT_FALSE(cheap->dominated);
    EXPECT_TRUE(parts::AdmitAll::is_exact(parts::AdmissionContext{}));
}

/// @brief KeepWhileWaiting stores nothing: its nodes are empty, solutions are collected when
///        taken, and the step releases each label it is done with.
TEST(Policies, KeepWhileWaitingStoresNothing) {
    namespace pt = policies_test;
    pt::Fixture f;
    using Waiting = parts::KeepWhileWaiting<pt::List>;
    static_assert(parts::Retention<Waiting, pt::Composed, pt::List>);
    static_assert(Waiting::records_at_take);
    static_assert(parts::releases_taken_labels_v<Waiting>);
    static_assert(!parts::releases_taken_labels_v<parts::KeepAtNodes<pt::List>>);
    static_assert(std::is_same_v<parts::Retain<parts::KeepWhileWaiting>::type<pt::List>, Waiting>);

    Waiting waiting;
    waiting.reset(3, pt::List{});
    auto* label = f.label(1, 3.0);
    waiting.erase(waiting.store(f.pos(1), label));
    EXPECT_TRUE(waiting.labels_at(f.pos(1)).empty());
    EXPECT_TRUE(waiting.containers().empty());
    auto view = waiting.node(f.pos(1));
    EXPECT_FALSE(view.is_dominated(*label));
    EXPECT_EQ(view.size(), 0U);
}

/// @brief The default observer is one, and records nothing.
TEST(Policies, TheDefaultObserverIsAnObserver) {
    static_assert(parts::Observer<parts::NoObserver, policies_test::Composed>);
    static_assert(parts::Admission<parts::DominanceAdmission, policies_test::Composed>);
}
