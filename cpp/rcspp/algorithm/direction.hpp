// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <vector>

#include "rcspp/graph/arc.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/graph/node.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief Compile-time policy holding the direction-dependent operations of the labeling loop.
///
/// The loop is written once against @c Dir and instantiated for each direction.
struct ForwardDirection {
        /// @brief Direction tag for `if constexpr` where a policy member does not suffice.
        static constexpr bool backward = false;

        /// @brief The arcs to walk from @p node: outgoing, going forward.
        template <typename ResourceType>
        static auto arcs(const Graph<ResourceType>& graph,
                         const Node<ResourceType>* node) -> std::span<Arc<ResourceType>*> {
            return graph.get_out_arcs(node);
        }

        /// @brief The endpoint an extension lands on: the arc's destination, going forward.
        template <typename ResourceType>
        static auto head(const Arc<ResourceType>& arc) -> Node<ResourceType>* {
            return arc.destination;
        }

        /// @brief The node id passed to the reachability look-ahead (the arc's destination).
        ///
        /// Both directions share one @c is_reachable, which is only valid if it is a predicate on
        /// the node id. One that reads the label's own collected set must declare
        /// @c MergeRule::Unspecified, as @c ReachableFeasibilityFunction does.
        template <typename ResourceType>
        static auto guard_node_id(const Arc<ResourceType>& arc) -> size_t {
            return arc.destination->id;
        }

        /// @brief Extends @p label along @p arc into @p extended_label.
        template <typename ResourceType>
        static void extend(const Label<ResourceType>& label, const Arc<ResourceType>& arc,
                           Label<ResourceType>* extended_label) {
            label.extend(arc, extended_label);
        }

        /// @brief Whether @p label satisfies the feasibility constraints for this direction.
        template <typename ResourceType>
        static auto feasible(const Label<ResourceType>& label) -> bool {
            return label.is_feasible();
        }

        /// @brief Whether @p lhs dominates @p rhs in this direction.
        template <typename ResourceType>
        static auto dominates(const Label<ResourceType>& lhs,
                              const Label<ResourceType>& rhs) -> bool {
            return lhs <= rhs;
        }

        /// @brief Applies the starting value of a seed label (no-op going forward).
        template <typename ResourceType>
        static void seed(Label<ResourceType>& /*label*/) {}

        /// @brief The nodes a search of this direction starts from.
        template <typename ResourceType>
        static auto seeds(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_source_node_ids();
        }

        /// @brief Whether @p node is a node this direction starts from.
        ///
        /// Used to keep sources and sinks out of a path's interior.
        template <typename ResourceType>
        static auto is_seed(const Node<ResourceType>* node) -> bool {
            return node->source;
        }

        /// @brief Whether @p node ends a complete path for this direction.
        template <typename ResourceType>
        static auto is_terminal(const Node<ResourceType>* node) -> bool {
            return node->sink;
        }

        /// @brief The nodes a search of this direction ends at.
        template <typename ResourceType>
        static auto terminals(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_sink_node_ids();
        }
};

/// @brief The mirror of @ref ForwardDirection: walk in-arcs, land on origins, extend backward.
///
/// A backward label stores the latest value at which the rest of the path is still feasible
/// (a deadline), so it uses the backward extension, feasibility and dominance forms.
struct BackwardDirection {
        /// @brief Tag readable in an `if constexpr`. See @ref ForwardDirection::backward.
        static constexpr bool backward = true;

        /// @brief The arcs to walk from @p node: incoming, going backward.
        template <typename ResourceType>
        static auto arcs(const Graph<ResourceType>& graph,
                         const Node<ResourceType>* node) -> std::span<Arc<ResourceType>*> {
            return graph.get_in_arcs(node);
        }

        /// @brief The endpoint an extension lands on: the arc's origin, going backward.
        template <typename ResourceType>
        static auto head(const Arc<ResourceType>& arc) -> Node<ResourceType>* {
            return arc.origin;
        }

        /// @brief The node id passed to the reachability look-ahead (the arc's origin).
        template <typename ResourceType>
        static auto guard_node_id(const Arc<ResourceType>& arc) -> size_t {
            return arc.origin->id;
        }

        /// @brief Extends @p label backward along @p arc into @p extended_label.
        template <typename ResourceType>
        static void extend(const Label<ResourceType>& label, const Arc<ResourceType>& arc,
                           Label<ResourceType>* extended_label) {
            label.extend_back(arc, extended_label);
        }

        /// @brief Whether @p label satisfies the backward feasibility constraints.
        template <typename ResourceType>
        static auto feasible(const Label<ResourceType>& label) -> bool {
            return label.is_back_feasible();
        }

        /// @brief Whether @p lhs dominates @p rhs going backward.
        ///
        /// For threshold resources a later deadline is better, so the order reverses.
        template <typename ResourceType>
        static auto dominates(const Label<ResourceType>& lhs,
                              const Label<ResourceType>& rhs) -> bool {
            return lhs.back_dominates(rhs);
        }

        /// @brief Applies the starting value of a seed label: the sink's upper bound, not zero.
        template <typename ResourceType>
        static void seed(Label<ResourceType>& label) {
            label.get_resource().apply_back_seed();
        }

        /// @brief The nodes a backward search starts from: the sinks.
        template <typename ResourceType>
        static auto seeds(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_sink_node_ids();
        }

        /// @brief Whether @p node is a node this direction starts from (a sink).
        template <typename ResourceType>
        static auto is_seed(const Node<ResourceType>* node) -> bool {
            return node->sink;
        }

        /// @brief Whether @p node ends a complete path for a backward search: a source.
        template <typename ResourceType>
        static auto is_terminal(const Node<ResourceType>* node) -> bool {
            return node->source;
        }

        /// @brief The nodes a backward search ends at: the sources.
        template <typename ResourceType>
        static auto terminals(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_source_node_ids();
        }
};

/// @brief The interface a direction policy must provide.
///
/// @tparam Dir          The candidate policy.
/// @tparam ResourceType The resource type it will be used with.
template <typename Dir, typename ResourceType>
concept DirectionPolicy =
    requires(const Graph<ResourceType>& graph, const Node<ResourceType>* node,
             const Arc<ResourceType>& arc, const Label<ResourceType>& label,
             Label<ResourceType>& mutable_label, Label<ResourceType>* extended_label) {
        {
            Dir::template arcs<ResourceType>(graph, node)
        } -> std::same_as<std::span<Arc<ResourceType>*>>;
        { Dir::template head<ResourceType>(arc) } -> std::same_as<Node<ResourceType>*>;
        { Dir::template guard_node_id<ResourceType>(arc) } -> std::same_as<size_t>;
        Dir::template extend<ResourceType>(label, arc, extended_label);
        { Dir::template feasible<ResourceType>(label) } -> std::same_as<bool>;
        { Dir::template dominates<ResourceType>(label, label) } -> std::same_as<bool>;
        Dir::template seed<ResourceType>(mutable_label);
        { Dir::template seeds<ResourceType>(graph) } -> std::same_as<const std::vector<size_t>&>;
        { Dir::template is_terminal<ResourceType>(node) } -> std::same_as<bool>;
        { Dir::template is_seed<ResourceType>(node) } -> std::same_as<bool>;
        {
            Dir::template terminals<ResourceType>(graph)
        } -> std::same_as<const std::vector<size_t>&>;
        { Dir::backward } -> std::convertible_to<bool>;
    };

}  // namespace rcspp
