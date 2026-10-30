// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The parts that decide what a node keeps, driven by hand: the admission rule on a node view, the
// node view of both containers in both directions, and the labels kept at their nodes.

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace policies_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;
using BackwardList = LabelList<Composed, BackwardDirection>;
using Buckets = LabelBuckets<RealResource, RealResource, Composed>;

/// @brief A line 0 -> 1 -> 2 whose one resource is the cost, and a pool for its labels.
class Fixture {
    public:
        Fixture() : graph_(std::make_unique<ResourceGraph<RealResource>>()) {
            graph_->add_resource<RealResource>(
                std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
            graph_->add_node(0, /*source=*/true, /*sink=*/false);
            graph_->add_node(1);
            graph_->add_node(2, /*source=*/false, /*sink=*/true);
            graph_->add_arc<RealResource>({1.0}, 0, 1, 1.0);
            graph_->add_arc<RealResource>({1.0}, 1, 2, 1.0);
            graph_->sort_nodes();
            pool_ = std::make_unique<LabelPool<Composed>>(
                std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory()));
        }

        /// @brief A label at node @p node_id whose resource (its cost) is @p cost.
        Label<Composed>* label(size_t node_id, double cost) {
            auto& made = pool_->get_next_label(graph_->get_node(node_id));
            made.get_resource().get_component<RealResource>(0).set_value(cost);
            made.dominated = false;
            return &made;
        }

        /// @brief The position of node @p node_id.
        [[nodiscard]] size_t pos(size_t node_id) const { return graph_->get_node(node_id)->pos(); }

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
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
    const parts::AdmissionContext ctx;
    EXPECT_TRUE(parts::DominanceAdmission::exact(ctx));

    auto* dear = f.label(1, 5.0);
    ASSERT_TRUE(admission.admit(*dear, view, ctx)) << "an empty node keeps it";
    node.add_label(dear);
    EXPECT_FALSE(admission.admit(*f.label(1, 6.0), view, ctx)) << "dominated by the 5";
    EXPECT_EQ(view.size(), 1U);

    auto* cheap = f.label(1, 2.0);
    ASSERT_TRUE(admission.admit(*cheap, view, ctx));
    EXPECT_TRUE(dear->dominated) << "the 2 evicts the 5";
    EXPECT_EQ(view.size(), 0U) << "evicted, and not yet stored";
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
    static_assert(parts::Retention<parts::KeepAtNodes<pt::List>, pt::Composed>);
    static_assert(!parts::KeepAtNodes<pt::List>::collects_at_take);

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

/// @brief The default observer is one, and records nothing.
TEST(Policies, TheDefaultObserverIsAnObserver) {
    static_assert(parts::Observer<parts::NoObserver, policies_test::Composed>);
    static_assert(parts::Admission<parts::DominanceAdmission, policies_test::Composed>);
}
