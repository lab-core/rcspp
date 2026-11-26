// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The label pool constructs its labels in place, in blocks that never reallocate: a label never
// moves, released labels are reused in place, clear() keeps the blocks and release() frees them.
// The blocks grow, from kFirstBlockLabels labels up to kMaxBlockLabels.

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <set>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace label_pool_test {

using rcspp::RealResource;
using Composed = rcspp::ResourceTypeComposition<RealResource>;
using Pool = rcspp::LabelPool<Composed>;

/// @brief A source and a sink, and @p components RealResource components.
inline std::unique_ptr<rcspp::ResourceGraph<RealResource>> component_graph(size_t components) {
    auto graph = std::make_unique<rcspp::ResourceGraph<RealResource>>();
    for (size_t component = 0; component < components; ++component) {
        graph->add_resource<RealResource>(
            std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
            std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
            std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
            std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    }
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1, /*source=*/false, /*sink=*/true);
    return graph;
}

/// @brief A source and a sink, and three RealResource components.
inline std::unique_ptr<rcspp::ResourceGraph<RealResource>> three_component_graph() {
    return component_graph(3);
}

/// @brief The components of the graph that checks a new label's reserved vector. Five: grown one
///        component at a time, a vector of five ends with a capacity of 6 on MSVC (1, 2, 3, 4,
///        6) and of 8 with libstdc++ and libc++, so only a reservation gives exactly 5.
inline constexpr size_t kReservedComponents = 5;

inline std::unique_ptr<Pool> pool_for(rcspp::ResourceGraph<RealResource>* graph) {
    return std::make_unique<Pool>(
        std::make_unique<rcspp::LabelFactory<Composed>>(&graph->get_resource_factory()));
}

/// @brief Draws @p count labels at @p node.
inline std::vector<rcspp::Label<Composed>*> draw(Pool* pool, const rcspp::Node<Composed>* node,
                                                 size_t count) {
    std::vector<rcspp::Label<Composed>*> labels;
    labels.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        labels.push_back(&pool->get_next_label(node));
    }
    return labels;
}

}  // namespace label_pool_test

/// @brief Labels made across several blocks keep their addresses, and release_all_labels hands
///        the same objects out again. A block that had grown past its reservation would have
///        moved its labels: the blocks' capacity is checked before any label is read.
TEST(LabelPool, LabelsNeverMoveAndAreReusedInPlace) {
    namespace lpt = label_pool_test;
    auto graph = lpt::three_component_graph();
    auto pool = lpt::pool_for(graph.get());
    const auto* source = graph->get_node(0);
    // Five full blocks (1 + 2 + 4 + 8 + 16 times the first) and 7 labels of the sixth.
    const size_t count = (31 * lpt::Pool::kFirstBlockLabels) + 7;

    const auto made = lpt::draw(pool.get(), source, count);
    // Six blocks, of 1, 2, 4, 8, 16 and 32 times the first: none grew past its reservation.
    ASSERT_EQ(pool->get_label_capacity(), 63 * lpt::Pool::kFirstBlockLabels);
    const std::set<rcspp::Label<lpt::Composed>*> addresses(made.begin(), made.end());
    ASSERT_EQ(addresses.size(), count);
    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(made[i]->id, i);
        EXPECT_EQ(made[i]->get_end_node(), source);
    }
    EXPECT_EQ(pool->get_nb_total_labels(), count);
    EXPECT_EQ(pool->get_nb_created_labels(), static_cast<int64_t>(count));
    EXPECT_TRUE(pool->check_ref_count_consistency());

    pool->release_all_labels();
    const auto again = lpt::draw(pool.get(), graph->get_node(1), count);
    EXPECT_EQ(std::set<rcspp::Label<lpt::Composed>*>(again.begin(), again.end()), addresses);
    EXPECT_EQ(pool->get_nb_total_labels(), count);
    EXPECT_EQ(pool->get_nb_created_labels(), static_cast<int64_t>(count));
    EXPECT_EQ(pool->get_nb_reused_labels(), static_cast<int64_t>(count));
}

/// @brief clear() destroys the labels and keeps the blocks: the next labels are made at the same
///        addresses. release() frees the blocks.
TEST(LabelPool, ClearKeepsTheBlocksAndReleaseFreesThem) {
    namespace lpt = label_pool_test;
    auto graph = lpt::three_component_graph();
    auto pool = lpt::pool_for(graph.get());
    const auto* source = graph->get_node(0);
    const size_t count = lpt::Pool::kFirstBlockLabels + 1;

    const auto first = lpt::draw(pool.get(), source, count);
    const size_t capacity = pool->get_label_capacity();
    pool->clear();
    EXPECT_EQ(pool->get_nb_total_labels(), 0U);
    EXPECT_EQ(pool->get_nb_available_labels(), 0U);
    EXPECT_EQ(pool->get_label_capacity(), capacity);

    const auto second = lpt::draw(pool.get(), source, count);
    EXPECT_EQ(second, first);
    EXPECT_EQ(pool->get_nb_total_labels(), count);
    EXPECT_EQ(pool->get_label_capacity(), capacity);

    pool->release();
    EXPECT_EQ(pool->get_nb_total_labels(), 0U);
    EXPECT_EQ(pool->get_label_capacity(), 0U);
    const auto third = lpt::draw(pool.get(), source, 1);
    EXPECT_EQ(third.front()->get_end_node(), source);
    EXPECT_EQ(pool->get_nb_total_labels(), 1U);
    EXPECT_EQ(pool->get_label_capacity(), lpt::Pool::kFirstBlockLabels);  // growth starts again
}

/// @brief The first block holds kFirstBlockLabels labels and each next one twice as many as the
///        one before, up to kMaxBlockLabels: a pool that makes a few labels takes a small block.
TEST(LabelPool, BlocksDoubleUpToTheCap) {
    namespace lpt = label_pool_test;
    auto graph = lpt::three_component_graph();
    auto pool = lpt::pool_for(graph.get());
    const auto* source = graph->get_node(0);

    EXPECT_EQ(pool->get_label_capacity(), 0U);  // no block before the first label
    lpt::draw(pool.get(), source, 1);
    EXPECT_EQ(pool->get_label_capacity(), lpt::Pool::kFirstBlockLabels);

    // Fill the blocks, then make one label more: each new block is twice the last, up to the cap.
    size_t made = 1;
    size_t capacity = lpt::Pool::kFirstBlockLabels;
    size_t block = lpt::Pool::kFirstBlockLabels;
    while (block < lpt::Pool::kMaxBlockLabels) {
        lpt::draw(pool.get(), source, capacity + 1 - made);
        made = capacity + 1;
        block *= 2;
        capacity += block;
        ASSERT_EQ(pool->get_label_capacity(), capacity);
    }
    lpt::draw(pool.get(), source, capacity + 1 - made);
    EXPECT_EQ(pool->get_label_capacity(), capacity + lpt::Pool::kMaxBlockLabels);
}

/// @brief A new label's resource copies its node's, with its component vector reserved to the
///        number of components (see @c kReservedComponents for why five).
TEST(LabelPool, ANewLabelCopiesItsNodesResourceIntoReservedVectors) {
    namespace lpt = label_pool_test;
    auto graph = lpt::component_graph(lpt::kReservedComponents);
    auto pool = lpt::pool_for(graph.get());

    auto& label = pool->get_next_label(graph->get_node(1));
    const auto& components = label.get_resource().get_components<RealResource>();
    EXPECT_EQ(components.size(), lpt::kReservedComponents);
    EXPECT_EQ(components.capacity(), lpt::kReservedComponents);
    EXPECT_EQ(label.get_resource().get_cost(), 0.0);
}
