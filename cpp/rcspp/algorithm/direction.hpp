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
/// The loop is written once against @c Direction and instantiated for each direction.
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

        /// @brief Whether resource @p lhs dominates @p rhs in this direction.
        template <typename ResourceType>
        static auto dominates(const Resource<ResourceType>& lhs,
                              const Resource<ResourceType>& rhs) -> bool {
            return lhs <= rhs;
        }

        /// @brief Whether resource @p lhs comes before @p rhs in this direction's dominance order,
        ///        shifted by @p delta (@c Resource::is_lower). @c LabelBuckets places values on
        ///        its bucket axis with it; @p delta may be negative.
        template <typename ResourceType>
        static auto is_lower(const Resource<ResourceType>& lhs, const Resource<ResourceType>& rhs,
                             double delta = 0) -> bool {
            return lhs.is_lower(rhs, delta);
        }

        /// @brief Gives a start label its starting value (no-op going forward).
        template <typename ResourceType>
        static void apply_start(const Graph<ResourceType>& /*graph*/,
                                const Node<ResourceType>* /*node*/,
                                Label<ResourceType>& /*label*/) {}

        /// @brief The nodes a search of this direction starts from.
        template <typename ResourceType>
        static auto start_nodes(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_source_node_ids();
        }

        /// @brief Whether @p node is a node this direction starts from.
        ///
        /// Used to keep sources and sinks out of a path's interior.
        template <typename ResourceType>
        static auto is_start_node(const Node<ResourceType>* node) -> bool {
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

        /// @brief Whether resource @p lhs dominates @p rhs going backward.
        template <typename ResourceType>
        static auto dominates(const Resource<ResourceType>& lhs,
                              const Resource<ResourceType>& rhs) -> bool {
            return lhs.back_dominates(rhs);
        }

        /// @brief Whether resource @p lhs comes before @p rhs in the backward dominance order,
        ///        shifted by @p delta (@c Resource::is_back_lower). A threshold's backward order
        ///        is reversed, so "before" is the larger value; @p delta may be negative.
        template <typename ResourceType>
        static auto is_lower(const Resource<ResourceType>& lhs, const Resource<ResourceType>& rhs,
                             double delta = 0) -> bool {
            return lhs.is_back_lower(rhs, delta);
        }

        /// @brief Gives a start label at sink @p node its starting value: the one the
        ///        extension of its @ref start_arc sets (see @c ExtensionFunction::start_back).
        ///
        /// A sink no live arc enters keeps the type default, as no backward step leaves it.
        template <typename ResourceType>
        static void apply_start(const Graph<ResourceType>& graph, const Node<ResourceType>* node,
                                Label<ResourceType>& label) {
            if (const auto* arc = start_arc(graph, node); arc != nullptr) {
                arc->extender->start_back(&label.get_resource());
            }
        }

        /// @brief The arc a backward start label at sink @p node reads its start from: the first
        ///        live arc entering it that carries an extender.
        ///
        /// Every arc carries the same functions, so any one sets the same start. Whoever checks
        /// that start (the half-way validation) reads it from this arc too.
        ///
        /// @return The arc, or @c nullptr when no live arc with an extender enters @p node.
        template <typename ResourceType>
        static auto start_arc(const Graph<ResourceType>& graph,
                              const Node<ResourceType>* node) -> const Arc<ResourceType>* {
            for (const auto* arc : graph.get_in_arcs(node)) {
                if (arc->extender != nullptr) {
                    return arc;
                }
            }
            return nullptr;
        }

        /// @brief The nodes a backward search starts from: the sinks.
        template <typename ResourceType>
        static auto start_nodes(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_sink_node_ids();
        }

        /// @brief Whether @p node is a node this direction starts from (a sink).
        template <typename ResourceType>
        static auto is_start_node(const Node<ResourceType>* node) -> bool {
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
/// @tparam Direction    The candidate policy.
/// @tparam ResourceType The resource type it will be used with.
template <typename Direction, typename ResourceType>
concept DirectionPolicy =
    requires(const Graph<ResourceType>& graph, const Node<ResourceType>* node,
             const Arc<ResourceType>& arc, const Label<ResourceType>& label,
             Label<ResourceType>& mutable_label, Label<ResourceType>* extended_label) {
        {
            Direction::template arcs<ResourceType>(graph, node)
        } -> std::same_as<std::span<Arc<ResourceType>*>>;
        { Direction::template head<ResourceType>(arc) } -> std::same_as<Node<ResourceType>*>;
        Direction::template extend<ResourceType>(label, arc, extended_label);
        { Direction::template feasible<ResourceType>(label) } -> std::same_as<bool>;
        { Direction::template dominates<ResourceType>(label, label) } -> std::same_as<bool>;
        Direction::template apply_start<ResourceType>(graph, node, mutable_label);
        {
            Direction::template start_nodes<ResourceType>(graph)
        } -> std::same_as<const std::vector<size_t>&>;
        { Direction::template is_terminal<ResourceType>(node) } -> std::same_as<bool>;
        { Direction::template is_start_node<ResourceType>(node) } -> std::same_as<bool>;
        {
            Direction::template terminals<ResourceType>(graph)
        } -> std::same_as<const std::vector<size_t>&>;
        { Direction::backward } -> std::convertible_to<bool>;
    };

}  // namespace rcspp
