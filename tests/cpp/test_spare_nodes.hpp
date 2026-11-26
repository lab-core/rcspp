// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// detail::SpareNodes keeps the nodes a group of lists no longer uses, for their next insertions;
// LabelList keeps its nodes that way, and KeepAtNodes keeps its containers from solve to solve.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <list>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "test_label_pool.hpp"

namespace spare_nodes_test {

using Spare = rcspp::detail::SpareNodes<int>;
using List = std::list<int>;
using label_pool_test::Composed;
using Labels = rcspp::LabelList<Composed>;

/// @brief A container derived from LabelList with state of its own and no clear() of its own.
class DerivedLabels : public Labels {
    public:
        [[nodiscard]] DerivedLabels copy() const { return {}; }

    private:
        [[maybe_unused]] int seen_ = 0;
};

/// @brief A container derived from LabelList that keeps its labels in id order through the
///        protected insert_label and remove_label.
class OrderedLabels : public Labels {
    public:
        using LabelPosition = std::list<rcspp::Label<Composed>*>::iterator;

        [[nodiscard]] OrderedLabels copy() const { return {}; }

        LabelPosition add_label(rcspp::Label<Composed>* label) override {
            const auto later = std::ranges::find_if(labels_, [label](const auto* other) {
                return other->id > label->id;
            });
            return insert_label(later, label);
        }

        void erase_label(const LabelPosition& pos) override { remove_label(pos); }
};

/// @brief The values of @p list, in order.
inline std::vector<int> values(const List& list) {
    return {list.begin(), list.end()};
}

}  // namespace spare_nodes_test

/// @brief A node taken out of a list is the one the next insertion uses, into that list or
///        another of the group; the list order is the one std::list would give.
TEST(SpareNodes, ANodeTakenOutIsTheNextOneInserted) {
    namespace snt = spare_nodes_test;
    snt::Spare spare;
    snt::List first;
    snt::List second;
    spare.push_back(&first, 1);
    const auto two = spare.push_back(&first, 2);
    spare.push_back(&first, 3);
    const int* node = &*two;

    const auto after = spare.erase(&first, two);
    EXPECT_EQ(*after, 3);
    EXPECT_EQ(snt::values(first), (std::vector<int>{1, 3}));
    EXPECT_EQ(spare.size(), 1U);

    const auto four = spare.insert(&second, second.cend(), 4);
    EXPECT_EQ(&*four, node) << "the spare node is reused";
    EXPECT_EQ(*four, 4);
    EXPECT_EQ(spare.size(), 0U);
    EXPECT_EQ(snt::values(second), (std::vector<int>{4}));

    const auto zero = spare.insert(&first, first.cbegin(), 0);  // a new node: none is spare
    EXPECT_EQ(snt::values(first), (std::vector<int>{0, 1, 3}));
    EXPECT_EQ(*zero, 0);
}

/// @brief take_all empties a list into the spare nodes; the elements of other lists, and the
///        iterators to them, are untouched; release frees the spare nodes.
TEST(SpareNodes, TakeAllKeepsEveryNode) {
    namespace snt = spare_nodes_test;
    snt::Spare spare;
    snt::List first;
    snt::List second;
    for (int value = 0; value < 4; ++value) {
        spare.push_back(&first, value);
    }
    const auto kept = spare.push_back(&second, 10);

    spare.take_all(&first);
    EXPECT_TRUE(first.empty());
    EXPECT_EQ(spare.size(), 4U);
    EXPECT_EQ(*kept, 10);

    std::vector<const int*> reused;
    for (int value = 0; value < 4; ++value) {
        reused.push_back(&*spare.push_back(&second, value));
    }
    EXPECT_EQ(spare.size(), 0U);
    EXPECT_EQ(snt::values(second), (std::vector<int>{10, 0, 1, 2, 3}));

    spare.take_all(&second);
    EXPECT_EQ(spare.size(), 5U);
    spare.release();
    EXPECT_EQ(spare.size(), 0U);
    spare.push_back(&first, 7);  // after a release, a new node
    EXPECT_EQ(snt::values(first), (std::vector<int>{7}));
}

/// @brief A LabelList reuses the node of a label it erased, of the labels it found dominated, and
///        of all its labels after clear().
TEST(SpareNodes, ALabelListReusesItsNodes) {
    namespace snt = spare_nodes_test;
    namespace lpt = label_pool_test;
    auto graph = lpt::three_component_graph();
    auto pool = lpt::pool_for(graph.get());
    const auto labels = lpt::draw(pool.get(), graph->get_node(1), 6);
    snt::Labels list;

    const auto first = list.add_label(labels[0]);
    const auto* first_node = &*first;
    list.erase_label(first);
    EXPECT_TRUE(list.get_labels().empty());
    EXPECT_EQ(&*list.add_label(labels[1]), first_node);

    // Equal labels dominate one another: labels[1] removes the two others.
    const auto* second_node = &*list.add_label(labels[2]);
    const auto* third_node = &*list.add_label(labels[3]);
    EXPECT_EQ(list.remove_dominated_labels(*labels[1]), 2U);
    EXPECT_EQ(list.get_labels().size(), 1U);
    const auto* reused_a = &*list.add_label(labels[4]);
    const auto* reused_b = &*list.add_label(labels[5]);
    EXPECT_TRUE((reused_a == second_node && reused_b == third_node) ||
                (reused_a == third_node && reused_b == second_node));

    list.clear();
    EXPECT_TRUE(list.get_labels().empty());
    std::vector<const rcspp::Label<snt::Composed>* const*> after;
    for (auto* label : labels) {
        after.push_back(&*list.add_label(label));
    }
    // The three nodes the list held are reused, before any new one.
    for (const auto* node : {first_node, reused_a, reused_b}) {
        EXPECT_NE(std::ranges::find(after, node), after.end());
    }
}

/// @brief A derived container that orders its labels itself reuses the list's nodes through
///        insert_label and remove_label.
TEST(SpareNodes, ADerivedContainerReusesNodesThroughTheHelpers) {
    namespace snt = spare_nodes_test;
    namespace lpt = label_pool_test;
    auto graph = lpt::three_component_graph();
    auto pool = lpt::pool_for(graph.get());
    const auto labels = lpt::draw(pool.get(), graph->get_node(1), 3);
    snt::OrderedLabels list;

    const auto position = list.add_label(labels[2]);
    const auto* node = &*position;
    list.erase_label(position);
    EXPECT_EQ(&*list.add_label(labels[1]), node) << "the erased label's node";
    list.add_label(labels[0]);
    ASSERT_EQ(list.get_labels().size(), 2U);
    EXPECT_EQ(list.get_labels().front(), labels[0]);
    EXPECT_EQ(list.get_labels().back(), labels[1]);

    // Cleared through the base, as through itself.
    static_cast<snt::Labels&>(list).clear();
    EXPECT_TRUE(list.get_labels().empty());
}

/// @brief KeepAtNodes empties its LabelLists for a new solve with as many nodes, keeping the same
///        containers; another number of nodes, or a container that only inherits clear(), makes
///        new ones.
TEST(SpareNodes, KeepAtNodesKeepsItsContainers) {
    namespace snt = spare_nodes_test;
    namespace lpt = label_pool_test;
    static_assert(rcspp::parts::declares_own_clear_v<snt::Labels>);
    static_assert(rcspp::parts::declares_own_clear_v<
                  rcspp::LabelBuckets<rcspp::RealResource, rcspp::RealResource, snt::Composed>>);
    static_assert(!rcspp::parts::declares_own_clear_v<snt::DerivedLabels>,
                  "an inherited clear() would leave what the derived container adds");

    auto graph = lpt::three_component_graph();
    auto pool = lpt::pool_for(graph.get());
    const auto labels = lpt::draw(pool.get(), graph->get_node(1), 2);
    rcspp::parts::KeepAtNodes<snt::Labels> keep;
    const snt::Labels prototype;

    keep.reset(3, prototype);
    const auto* container = keep.containers().data();
    const auto* node = &*keep.store(1, labels[0]);

    keep.reset(3, prototype);
    EXPECT_EQ(keep.containers().data(), container) << "the same containers";
    EXPECT_TRUE(keep.labels_at(1).empty());
    EXPECT_EQ(&*keep.store(1, labels[1]), node) << "the container kept its node";

    keep.reset(4, prototype);
    EXPECT_EQ(keep.containers().size(), 4U);
    EXPECT_TRUE(keep.labels_at(1).empty());
}
