// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

/// @brief How the bidirectional join tests whether a forward and a backward label of this resource
///        fit together.
///
/// The join pairs a forward and a backward label that meet at a node; every component must accept
/// the pair. Declared by the feasibility function; a bidirectional solve refuses @c Unspecified.
enum class JoinRule {
    Unspecified,  ///< no join test: a bidirectional solve refuses the resource
    AlwaysTrue,   ///< never blocks a join (a cost, an unconstrained set)
    ValueOrder,   ///< the forward value is at most the backward one: `f <= b`
    Custom,       ///< the feasibility function's own can_be_joined decides
};

/// @brief Abstract base class defining the feasibility function for a resource type.
///
/// A feasibility function determines whether a label's accumulated resource
/// value satisfies the constraints of the problem (e.g., time-window or
/// capacity constraints) at a given node.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class FeasibilityFunction {
    public:
        virtual ~FeasibilityFunction() = default;

        /// @brief Returns whether the given resource value is feasible in the forward direction.
        ///
        /// Must handle any value of its type, the infinities included: a bidirectional setup asks
        /// it about a value above every bound, and about each value the extension clamps a
        /// backward label to or starts one at, and the value just above it.
        ///
        /// @param resource The accumulated resource value to test.
        /// @return @c true if the resource satisfies the forward feasibility constraint.
        [[nodiscard]] virtual auto is_feasible(const ResourceType& resource) -> bool = 0;

        /// @brief Whether a backward label's value is feasible at this node: the backward search's
        ///        feasibility test.
        ///
        /// Defaults to @c is_feasible, which suits a backward value that reads like a forward one
        /// (a sum). A threshold's backward value is a limit, so override it: a time window tests
        /// only the opening time, since the clamp already enforces the closing time.
        ///
        /// Must handle any value of its type, the infinities included: a bidirectional setup asks
        /// it about a value below every bound, to learn whether it tests a floor, and about the
        /// lowest value a forward label can hold at the node, to check that floor.
        ///
        /// @param resource The accumulated resource value to test.
        /// @return @c true if the resource satisfies the backward feasibility constraint.
        [[nodiscard]] virtual auto is_back_feasible(const ResourceType& resource) -> bool {
            return is_feasible(resource);
        }

        /// @brief Whether a forward and a backward label meeting at this node fit together for this
        ///        resource. Called by the join only when @c join_rule() is @c Custom.
        ///
        /// Must be exact: the join trusts both answers, so a wrong "no" loses paths and a wrong
        /// "yes" returns infeasible ones. A function that cannot decide from the two values alone
        /// must declare @c JoinRule::Unspecified instead.
        ///
        /// @param resource      The forward label's resource value.
        /// @param back_resource The backward label's resource value.
        /// @return @c true if the two labels can be joined into a feasible path.
        /// @throws std::runtime_error If not overridden in a derived class.
        [[nodiscard]] virtual auto can_be_joined(const ResourceType& resource,
                                                 const ResourceType& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_joined not implemented");
        };

        /// @brief Which join test the bidirectional join applies to this resource.
        ///
        /// May depend on @c backward_kind(): MinMax compares under a threshold and adds under an
        /// accumulation. @c Unspecified by default, so a bidirectional solve refuses a function
        /// that declares none.
        ///
        /// @return The join rule declared by this feasibility function.
        [[nodiscard]] virtual JoinRule join_rule() const { return JoinRule::Unspecified; }

        /// @brief Records the paired extension function's backward kind. Called by
        ///        @c ResourceGraph::add_resource, not by hand.
        ///
        /// The feasibility function reads it to choose its join test and to know what a backward
        /// value means: a limit (@c Threshold) or a sum (@c Accumulate).
        ///
        /// @param kind The paired extension function's declared backward kind.
        void set_backward_kind(BackwardKind kind) { backward_kind_ = kind; }

        /// @brief The paired extension function's backward kind; @c Unspecified until the resource
        ///        is added to a graph.
        ///
        /// @return The declared backward kind of the paired extension function.
        [[nodiscard]] BackwardKind backward_kind() const { return backward_kind_; }

        /// @brief Whether this function's test only has a backward reading when the memory
        ///        excludes the node it sits on.
        ///
        /// Declare @c true when @c is_feasible asks whether the current node is already in the
        /// label's memory (the ng-route condition). That only works backward with
        /// @c BackwardKind::ArcEndpoints; under @c ArcValue every backward label would be
        /// rejected. A bidirectional solve refuses the mismatched pairing at setup.
        ///
        /// @return @c true when only an @c ArcEndpoints extension can supply this function's
        /// memory.
        [[nodiscard]] virtual bool requires_arc_endpoints() const { return false; }

        /// @brief Whether this function's join test is exact only if the value is a running sum
        ///        that no arc lowers.
        ///
        /// True for a test that adds the two halves: their sum bounds the path's largest value only
        /// if the value never decreases, and is the path's value only if the extension is a sum. A
        /// bidirectional setup then refuses a negative consumption on any arc of this resource, and
        /// an accumulating extension that does not add.
        ///
        /// @return @c true if the backward reading needs non-negative consumptions.
        [[nodiscard]] virtual auto requires_nondecreasing() const -> bool { return false; }

        /// @brief Returns whether the label can potentially reach a destination node.
        ///
        /// Used as a pruning test during label propagation. Defaults to @c true.
        ///
        /// @param resource            The current label holding the resource value.
        /// @param destination_node_id Index of the destination node to reach.
        /// @return @c true if reaching @p destination_node_id is still possible.
        virtual auto is_reachable(const Resource<ResourceType>& resource,
                                  size_t destination_node_id) -> bool {
            return true;
        }

        /// @brief Creates a polymorphic copy of this feasibility function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<FeasibilityFunction> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new feasibility function instance ready for use at @p node_id.
        virtual auto create(const size_t node_id) -> std::unique_ptr<FeasibilityFunction> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }

        /// @brief Resets this function's state for a new node.
        ///
        /// @param node_id Index of the node to reset for.
        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        /// @brief Optional node-specific preprocessing hook.
        ///
        /// Called by @c create() and @c reset(). Override to cache node-dependent data.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}

        /// @brief The paired extension function's backward kind, set by add_resource and copied
        ///        into every per-node clone.
        BackwardKind backward_kind_ = BackwardKind::Unspecified;
};

/// @brief Thrown by a composed function whose backward members (@c is_back_feasible and
///        @c can_be_joined here) are not overridden.
///
/// The bidirectional setup calls each once before searching, and turns this exception into a
/// refusal that names the missing override.
class NoBackwardFeasibility : public std::logic_error {
    public:
        using std::logic_error::logic_error;
};

/// @brief Specialization of @c FeasibilityFunction for composed resource types.
///
/// When @c ResourceType is a @c ResourceTypeComposition, the feasibility function
/// receives the full @c Resource object so it can inspect all component values.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
// Specialization for ResourceTypeComposition: functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~FeasibilityFunction() = default;

        /// @brief Returns whether the composed resource is feasible in the forward direction.
        ///
        /// @param resource The current label holding all component resource values.
        /// @return @c true if all relevant constraints are satisfied.
        [[nodiscard]] virtual auto is_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool = 0;

        /// @brief Whether a composed backward label is feasible.
        ///
        /// The default throws @c NoBackwardFeasibility, so a forward-only composition still
        /// compiles and a bidirectional setup refuses it; @c CompositionFeasibilityFunction tests
        /// each component.
        ///
        /// @param resource The current label holding all component resource values.
        /// @return @c true if the backward feasibility constraint is satisfied.
        /// @throws NoBackwardFeasibility unless overridden.
        [[nodiscard]] virtual auto is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/) -> bool {
            throw NoBackwardFeasibility(
                "this composition feasibility function has no backward form: override "
                "is_back_feasible, as CompositionFeasibilityFunction does");
        }

        /// @brief Whether a forward and a backward composed label fit together.
        ///
        /// The default throws @c NoBackwardFeasibility; @c CompositionFeasibilityFunction asks each
        /// component. Must be exact, like the scalar form.
        ///
        /// @param resource      The forward label.
        /// @param back_resource The backward label.
        /// @return @c true if joining the two labels yields a feasible path.
        /// @throws NoBackwardFeasibility unless overridden.
        [[nodiscard]] virtual auto can_be_joined(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*back_resource*/) -> bool {
            throw NoBackwardFeasibility(
                "this composition feasibility function has no join test: override "
                "can_be_joined, as CompositionFeasibilityFunction does");
        };

        /// @brief Not read: a composition has no rule of its own. The bidirectional setup reads
        ///        each component's @c join_rule() instead.
        ///
        /// @return @c JoinRule::Unspecified unless overridden.
        [[nodiscard]] virtual JoinRule join_rule() const { return JoinRule::Unspecified; }

        /// @brief Returns whether the label can potentially reach a destination node.
        ///
        /// Defaults to @c true. Override to add reachability pruning.
        ///
        /// @param resource            The current label.
        /// @param destination_node_id Index of the destination node.
        /// @return @c true if reaching @p destination_node_id is still possible.
        virtual auto is_reachable(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            size_t /*destination_node_id*/) -> bool {
            return true;
        }

        /// @brief Creates a polymorphic copy of this feasibility function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new feasibility function instance ready for use at @p node_id.
        virtual auto create(const size_t node_id)
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }

        /// @brief Resets this function's state for a new node.
        ///
        /// @param node_id Index of the node to reset for.
        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        /// @brief Optional node-specific preprocessing hook.
        ///
        /// Called by @c create() and @c reset(). Override to cache node-dependent data.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
