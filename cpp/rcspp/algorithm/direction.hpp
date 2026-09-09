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

/// @brief Compile-time policy carrying every direction-dependent operation of the labeling loop.
///
/// The labeling loop is direction-agnostic except in a handful of places: which arcs it walks,
/// which endpoint of an arc it lands on, which extension and feasibility test it applies, where a
/// search starts and where it ends. Those are gathered here so the loop body can be written once
/// against @c Dir and instantiated twice.
///
/// Members are static, so there is no per-call indirection: the policy is a type, not an object.
struct ForwardDirection {
        /// @brief The arcs to walk from @p node: outgoing, going forward.
        template <typename ResourceType>
        static auto arcs(const Graph<ResourceType>& graph, const Node<ResourceType>* node)
            -> std::span<Arc<ResourceType>*> {
            return graph.get_out_arcs(node);
        }

        /// @brief The endpoint an extension lands on: the arc's destination, going forward.
        template <typename ResourceType>
        static auto head(const Arc<ResourceType>& arc) -> Node<ResourceType>* {
            return arc.destination;
        }

        /// @brief The node id the reachability look-ahead is asked about.
        ///
        /// @c is_reachable is direction-neutral -- a predicate on an id, with no directional
        /// assumption -- so the whole difference between the two directions is which endpoint's id
        /// is passed. There is deliberately no @c is_back_reachable.
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
        static auto dominates(const Label<ResourceType>& lhs, const Label<ResourceType>& rhs)
            -> bool {
            return lhs <= rhs;
        }

        /// @brief Applies the starting value of a seed label. Forward labels start at the type
        ///        default -- nothing consumed -- so this is a no-op.
        template <typename ResourceType>
        static void seed(Label<ResourceType>& /*label*/) {}

        /// @brief The nodes a search of this direction starts from.
        template <typename ResourceType>
        static auto seeds(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_source_node_ids();
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
/// A backward label at a node stores the *latest* you can be there and still finish -- a deadline
/// -- rather than the earliest you can arrive, which is why it extends, tests feasibility and
/// compares with the backward forms rather than the forward ones.
struct BackwardDirection {
        /// @brief The arcs to walk from @p node: incoming, going backward.
        template <typename ResourceType>
        static auto arcs(const Graph<ResourceType>& graph, const Node<ResourceType>* node)
            -> std::span<Arc<ResourceType>*> {
            return graph.get_in_arcs(node);
        }

        /// @brief The endpoint an extension lands on: the arc's origin, going backward.
        template <typename ResourceType>
        static auto head(const Arc<ResourceType>& arc) -> Node<ResourceType>* {
            return arc.origin;
        }

        /// @brief The node id the reachability look-ahead is asked about.
        ///
        /// See @ref ForwardDirection::guard_node_id: the predicate is shared, only the id differs.
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
        /// Not simply the forward comparison: for a threshold resource a *later* deadline is more
        /// permissive, so the order reverses. Which resources reverse is derived from their
        /// extension function's declared shape.
        template <typename ResourceType>
        static auto dominates(const Label<ResourceType>& lhs, const Label<ResourceType>& rhs)
            -> bool {
            return lhs.back_dominates(rhs);
        }

        /// @brief Applies the starting value of a seed label.
        ///
        /// A backward label at a sink starts at that sink's upper bound -- its closing time, its
        /// capacity -- not at zero, and the number differs per sink.
        template <typename ResourceType>
        static void seed(Label<ResourceType>& label) {
            label.get_resource().apply_back_seed();
        }

        /// @brief The nodes a backward search starts from: the sinks.
        template <typename ResourceType>
        static auto seeds(const Graph<ResourceType>& graph) -> const std::vector<size_t>& {
            return graph.get_sink_node_ids();
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

/// @brief The shape a direction policy must have.
///
/// Asserted inside @c DirectionalDominanceAlgorithm against the types it is actually instantiated
/// with, so a policy that drifts out of shape fails at the point of use rather than at whichever
/// call site happens to be compiled first. Costs nothing at runtime.
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
        {
            Dir::template terminals<ResourceType>(graph)
        } -> std::same_as<const std::vector<size_t>&>;
    };

}  // namespace rcspp
