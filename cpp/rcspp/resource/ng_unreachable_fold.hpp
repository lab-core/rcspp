// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/label_strengthening.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/concrete/functions/extension/ng-path_extension_function.hpp"
#include "rcspp/resource/functions/extension/threshold_form.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

/// @brief The type-independent face of @ref NgUnreachableFold, which `ResourceGraph` stores.
///
/// @tparam ResourceTypes The graph's resource types.
template <typename... ResourceTypes>
class NgUnreachableFoldBase : public LabelStrengthening<ResourceTypeComposition<ResourceTypes...>> {
    public:
        /// @brief The composed resource type of the graph's labels.
        using Composition = ResourceTypeComposition<ResourceTypes...>;

        /// @brief Reads the graph as this solve will search it: its live arcs, its ng table and
        ///        its threshold resources.
        ///
        /// Call after the solve's reductions and after `Graph::build_csr`, since it walks the
        /// arcs a node's labels extend along and indexes nodes by position.
        ///
        /// @param graph   The graph about to be searched.
        /// @param factory The graph's resource factory.
        virtual void prepare(const Graph<Composition>& graph,
                             ResourceCompositionFactory<ResourceTypes...>& factory) = 0;

        /// @brief A copy with the same settings and no prepared data, for a cloned graph.
        ///
        /// @return The copy.
        [[nodiscard]] virtual std::unique_ptr<NgUnreachableFoldBase> clone() const = 0;
};

/// @brief Adds to a forward label's ng memory every ng-neighbour of its node that it can no
///        longer reach, so that dominance discards more labels.
///
/// A neighbour `x` of node `j` is unreachable from a label at `j` when, for some threshold
/// resource (a component whose extension function is a @c TranslationThresholdForm, such as a
/// time window or a capacity), the label's value plus the least consumption of any path from
/// `j` to `x` exceeds the resource's upper bound at `x`. Such a label has no feasible completion
/// through `x`, so remembering `x` forbids nothing feasible: the solutions are unchanged. Least
/// consumptions are shortest paths over the live arcs, so the test holds whatever the arcs
/// consume, without assuming the triangle inequality.
///
/// Applied to forward labels only. A threshold resource with a negative arc consumption is
/// ignored, since a longer path could then consume less.
///
/// @tparam NgResource    The container resource type of the ng memory.
/// @tparam E             The element type of the ng neighbourhood sets.
/// @tparam ResourceTypes The graph's resource types.
template <typename NgResource, typename E, typename... ResourceTypes>
class NgUnreachableFold final : public NgUnreachableFoldBase<ResourceTypes...> {
        using Composition = ResourceTypeComposition<ResourceTypes...>;
        static constexpr size_t kNgTypeIndex =
            static_cast<size_t>(ComponentTypeIndex_v<NgResource, ResourceTypes...>);
        /// A fold needs the arrival to exceed the bound by more than this, relative to the bound,
        /// so that rounding never folds a node a solve would still reach.
        static constexpr double kTolerance = 1e-9;
        /// Marks a position that is not a target of the node being searched from.
        static constexpr size_t kNoTarget = std::numeric_limits<size_t>::max();

    public:
        /// @brief A fold on component @p ng_slot of type @p NgResource.
        ///
        /// @param ng_slot Which component of type @p NgResource is the ng memory (0 for the first).
        explicit NgUnreachableFold(size_t ng_slot) : ng_slot_(ng_slot) {}

        /// @brief The ng-path prototype of component @p ng_slot.
        ///
        /// @param factory The graph's resource factory.
        /// @param ng_slot Which component of type @p NgResource is the ng memory.
        /// @return The prototype, whose `neighborhoods()` is the current table.
        /// @throws std::out_of_range     when there is no such component.
        /// @throws std::invalid_argument when it is not an @c NgPathExtensionFunction.
        static const NgPathExtensionFunction<NgResource, E>& ng_prototype(
            ResourceCompositionFactory<ResourceTypes...>& factory, size_t ng_slot) {
            auto& factories = factory.template get_components<kNgTypeIndex>();
            if (ng_slot >= factories.size()) {
                throw std::out_of_range(
                    "enable_ng_unreachable_fold: component " + std::to_string(ng_slot) +
                    " does not exist: the model has " + std::to_string(factories.size()) +
                    " of that resource type");
            }
            const auto* ng = dynamic_cast<const NgPathExtensionFunction<NgResource, E>*>(
                &factories[ng_slot]->extension_function());
            if (ng == nullptr) {
                throw std::invalid_argument(
                    "enable_ng_unreachable_fold: component " + std::to_string(ng_slot) +
                    " is not an ng-path resource (NgPathExtensionFunction)");
            }
            return *ng;
        }

        void prepare(const Graph<Composition>& graph,
                     ResourceCompositionFactory<ResourceTypes...>& factory) override {
            thresholds_.clear();
            targets_by_pos_.assign(graph.get_number_of_nodes(), {});
            node_by_pos_.assign(graph.get_number_of_nodes(), nullptr);
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node != nullptr) {
                    node_by_pos_[node->pos()] = node;
                }
            }

            collect_targets(graph, ng_prototype(factory, ng_slot_).neighborhoods());
            [&]<size_t... I>(std::index_sequence<I...>) {
                (collect_thresholds<I>(graph, factory), ...);
            }(std::index_sequence_for<ResourceTypes...>{});
            values_.assign(thresholds_.size(), 0.0);
        }

        size_t strengthen_forward(Label<Composition>* label) override {
            if (thresholds_.empty()) {
                return 0;
            }
            const auto& targets = targets_by_pos_[label->get_end_node()->pos()];
            if (targets.empty()) {
                return 0;
            }
            auto& resource = label->get_resource();
            for (size_t t = 0; t < thresholds_.size(); ++t) {
                values_[t] = thresholds_[t].read(resource, thresholds_[t].slot);
            }
            auto& memory = resource.template get_component<NgResource>(ng_slot_).get_value();
            size_t folded = 0;
            for (const auto& target : targets) {
                if (memory.contains(target.member)) {
                    continue;
                }
                for (size_t t = 0; t < values_.size(); ++t) {
                    const double bound = target.upper_bound[t];
                    const double arrival = values_[t] + target.least_consumption[t];
                    if (arrival > bound + (kTolerance * std::max(1.0, std::abs(bound)))) {
                        memory.add(target.member);
                        ++folded;
                        break;
                    }
                }
            }
            return folded;
        }

        [[nodiscard]] std::unique_ptr<NgUnreachableFoldBase<ResourceTypes...>> clone()
            const override {
            return std::make_unique<NgUnreachableFold>(ng_slot_);
        }

    private:
        /// One threshold resource: where it is, and how to read a label's value of it.
        struct Threshold {
                size_t slot;
                double (*read)(const Resource<Composition>& resource, size_t slot);
        };

        /// One ng-neighbour of a node, with its least consumption from that node and its upper
        /// bound, one entry per threshold resource, in the order of @ref thresholds_.
        struct Target {
                size_t node_id;
                typename NgResource::ValueType member;
                std::vector<double> least_consumption;
                std::vector<double> upper_bound;
        };

        /// @brief A label's value of the threshold component @p slot of type @p R.
        template <typename R>
        static double read_value(const Resource<Composition>& resource, size_t slot) {
            return static_cast<double>(
                resource.template get_component<R>(slot).get_value().get_value());
        }

        /// @brief What @p arc consumes of the component @p slot of type @p R.
        template <typename R>
        static double arc_value(const Arc<Composition>& arc, size_t slot) {
            return static_cast<double>(
                arc.extender->template get_component<R>(slot).get_value().get_value());
        }

        /// @brief Lists, for every node that is not a sink, its ng-neighbours other than itself
        ///        that are nodes of the graph and neither sources nor sinks.
        ///
        /// @param graph The graph.
        /// @param table Each node's ng-neighbourhood.
        template <typename Table>
        void collect_targets(const Graph<Composition>& graph, const Table& table) {
            for (const auto& [node_id, members] : table) {
                const auto* node = graph.get_node(node_id);
                if (node == nullptr || node->sink) {
                    continue;
                }
                auto& targets = targets_by_pos_[node->pos()];
                for (const auto& member : members) {
                    const auto target_id = static_cast<size_t>(member);
                    const auto* target = graph.get_node(target_id);
                    if (target_id == node_id || target == nullptr || target->source ||
                        target->sink) {
                        continue;
                    }
                    targets.push_back(
                        {.node_id = target_id,
                         .member = static_cast<typename NgResource::ValueType>(target_id),
                         .least_consumption = {},
                         .upper_bound = {}});
                }
            }
        }

        /// @brief Adds every threshold component of type slot @p I.
        ///
        /// @param graph   The graph.
        /// @param factory The graph's resource factory.
        template <size_t I>
        void collect_thresholds(const Graph<Composition>& graph,
                                ResourceCompositionFactory<ResourceTypes...>& factory) {
            using R = std::tuple_element_t<I, std::tuple<ResourceTypes...>>;
            if constexpr (is_numerical_resource_v<R>) {
                auto& factories = factory.template get_components<I>();
                for (size_t slot = 0; slot < factories.size(); ++slot) {
                    const auto* form =
                        dynamic_cast<const TranslationThresholdForm<R, ExtensionFunction<R>>*>(
                            &factories[slot]->extension_function());
                    if (form != nullptr) {
                        add_threshold<R>(graph, *form, slot);
                    }
                }
            }
        }

        /// @brief Adds threshold component @p slot of type @p R: its least consumption from each
        ///        node to each of that node's targets, and its upper bound at each target.
        ///
        /// @param graph The graph.
        /// @param form  The component's extension function.
        /// @param slot  Which component of type @p R it is.
        template <typename R, typename Form>
        void add_threshold(const Graph<Composition>& graph, const Form& form, size_t slot) {
            bool negative = false;
            graph.for_each_arc([&](const Arc<Composition>& arc) {
                const double value = arc_value<R>(arc, slot);
                if (std::isfinite(value) && value < 0.0) {
                    negative = true;
                }
            });
            if (negative) {
                LOG_DEBUG(
                    "NgUnreachableFold: a threshold resource has a negative arc consumption, so it "
                    "folds nothing in this solve\n");
                return;
            }
            thresholds_.push_back({.slot = slot, .read = &read_value<R>});

            const double infinity = std::numeric_limits<double>::infinity();
            std::vector<double> distance(node_by_pos_.size(), infinity);
            std::vector<size_t> target_index(node_by_pos_.size(), kNoTarget);
            using Entry = std::pair<double, size_t>;  // (distance, position)
            for (size_t origin = 0; origin < node_by_pos_.size(); ++origin) {
                auto& targets = targets_by_pos_[origin];
                if (targets.empty()) {
                    continue;
                }
                std::ranges::fill(distance, infinity);
                for (size_t k = 0; k < targets.size(); ++k) {
                    target_index[graph.get_node(targets[k].node_id)->pos()] = k;
                }
                size_t remaining = targets.size();
                std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
                distance[origin] = 0.0;
                queue.emplace(0.0, origin);
                while (!queue.empty() && remaining > 0) {
                    const auto [reached, position] = queue.top();
                    queue.pop();
                    if (reached > distance[position]) {
                        continue;
                    }
                    if (position != origin && target_index[position] != kNoTarget) {
                        --remaining;
                    }
                    for (const auto* arc : graph.get_out_arcs(node_by_pos_[position])) {
                        const double value = arc_value<R>(*arc, slot);
                        if (!std::isfinite(value)) {
                            continue;
                        }
                        const size_t next = arc->destination->pos();
                        if (reached + value < distance[next]) {
                            distance[next] = reached + value;
                            queue.emplace(distance[next], next);
                        }
                    }
                }
                for (auto& target : targets) {
                    const size_t position = graph.get_node(target.node_id)->pos();
                    target.least_consumption.push_back(distance[position]);
                    const auto bound = form.node_upper_bound(target.node_id);
                    target.upper_bound.push_back(bound ? static_cast<double>(*bound) : infinity);
                    target_index[position] = kNoTarget;
                }
            }
        }

        size_t ng_slot_;
        std::vector<Threshold> thresholds_;
        std::vector<std::vector<Target>> targets_by_pos_;
        std::vector<const Node<Composition>*> node_by_pos_;
        std::vector<double> values_;  ///< Scratch: the label's threshold values.
};

}  // namespace rcspp
