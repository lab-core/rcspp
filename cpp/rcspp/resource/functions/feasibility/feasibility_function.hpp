// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

/// @brief How two half-paths are tested for compatibility at a join.
enum class MergeRule {
    Unspecified,     ///< not declared -- a bidirectional solve will refuse to run
    AlwaysTrue,      ///< this resource never blocks a join
    DominanceOrder,  ///< the forward value against the backward bound, as values: f <= b
    Custom,          ///< the function writes its own can_be_merged body
};

/// @brief Where a feasibility function's backward seed sits, as far as the type can tell.
///
/// @c Unknown is needed because some functions (e.g. @c MinMaxFeasibilityFunction) choose their
/// seed end at construction; those are checked at setup instead.
enum class BackSeedEnd {
    Never,    ///< back_seed_value() is always nullopt
    Ceiling,  ///< always seeds at an upper bound -- incoherent with an accumulating extension
    Unknown,  ///< depends on runtime state; defer to the setup validation
};

/// @brief What a feasibility function's type says about its backward seed.
///
/// Defaults to @c Unknown; concrete functions specialise it in their own headers.
template <typename T>
struct BackSeedEndOf {
        static constexpr BackSeedEnd value = BackSeedEnd::Unknown;
};

/// @brief The backward-seed end @p T publishes, or @c Unknown if its type cannot say.
template <typename T>
inline constexpr BackSeedEnd back_seed_end_v = BackSeedEndOf<T>::value;

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
        /// @param resource The accumulated resource value to test.
        /// @return @c true if the resource satisfies the forward feasibility constraint.
        [[nodiscard]] virtual auto is_feasible(const ResourceType& resource) -> bool = 0;

        /// @brief Returns whether the given resource value is feasible in the backward direction.
        ///
        /// Defaults to calling @c is_feasible(). Override for asymmetric constraints.
        ///
        /// @param resource The accumulated resource value to test.
        /// @return @c true if the resource satisfies the backward feasibility constraint.
        [[nodiscard]] virtual auto is_back_feasible(const ResourceType& resource) -> bool {
            return is_feasible(resource);
        }

        /// @brief Returns whether a forward and a backward label can be merged at this node.
        ///
        /// Used in bidirectional labeling to check join compatibility. The answer must be exact:
        /// the joiner trusts both a "yes" and a "no". A function that cannot decide from two
        /// values alone must declare @c MergeRule::Unspecified so that bidirectional solves refuse.
        ///
        /// @param resource      The forward label's resource value.
        /// @param back_resource The backward label's resource value.
        /// @return @c true if the two labels can be merged into a feasible path.
        /// @throws std::runtime_error If not overridden in a derived class.
        [[nodiscard]] virtual auto can_be_merged(const ResourceType& resource,
                                                 const ResourceType& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        /// @brief Which merge test this resource uses. May depend on @ref backward_kind().
        ///
        /// @return The merge rule declared by this feasibility function.
        [[nodiscard]] virtual MergeRule merge_rule() const { return MergeRule::Unspecified; }

        /// @brief Records how the paired extension function extends backwards.
        ///
        /// Set by @c ResourceGraph::add_resource, not by hand. The merge test needs it to know
        /// whether a backward value is a bound (compare) or an accumulation (add).
        ///
        /// @param kind The paired extension function's declared backward kind.
        void set_backward_kind(BackwardKind kind) { backward_kind_ = kind; }

        /// @brief How the paired extension function extends backwards.
        ///
        /// @c Unspecified until paired; a @c merge_rule() reading it must then refuse.
        ///
        /// @return The declared backward kind of the paired extension function.
        [[nodiscard]] BackwardKind backward_kind() const { return backward_kind_; }

        /// @brief Whether this function's test only has a backward reading when the memory
        ///        excludes the node it sits on.
        ///
        /// Declare @c true when @c is_feasible asks whether the current node is already in the
        /// label's memory (the ng-route condition). That only works backward with
        /// @c BackwardKind::EndpointMirror; under @c ArcValue every backward label would be
        /// rejected. A bidirectional solve refuses the mismatched pairing at setup.
        ///
        /// @return @c true when only an endpoint mirror can supply this function's memory.
        [[nodiscard]] virtual bool requires_endpoint_mirror() const { return false; }

        /// @brief The value a backward label starts with at this node (typically its upper
        ///        bound), or @c std::nullopt to start at the type default.
        ///
        /// @return The seed value for a backward label at this node, or @c std::nullopt.
        [[nodiscard]] virtual auto back_seed_value() const -> std::optional<ResourceType> {
            return std::nullopt;
        }

        /// @brief The largest value this function admits at @p node_id, if it bounds it there.
        ///
        /// Handed to a @c Threshold extension function to clamp backward labels. Called on a clone
        /// preprocessed for @p node_id, so returning a bound cached in @c preprocess() is correct.
        /// A bidirectional solve refuses a model whose clamp at some node fails that node's
        /// @c is_back_feasible.
        ///
        /// @param node_id Index of the node.
        /// @return This node's upper bound, or @c std::nullopt.
        [[nodiscard]] virtual auto ceiling_at(size_t /*node_id*/) const
            -> std::optional<ResourceType> {
            return std::nullopt;
        }

        /// @brief The smallest backward value @c is_back_feasible admits at @p node_id, if it
        ///        tests one.
        ///
        /// Read at setup: a floor the paired @c Threshold extension does not clamp forward rejects
        /// deadlines a forward path can meet, so a bidirectional solve refuses it. Must work for
        /// any node without preprocessing.
        ///
        /// @param node_id Index of the node.
        /// @return This node's backward floor, or @c std::nullopt.
        [[nodiscard]] virtual auto back_floor_at(size_t /*node_id*/) const
            -> std::optional<ResourceType> {
            return std::nullopt;
        }

        /// @brief Whether this function's backward reading assumes the value never decreases
        ///        along an arc.
        ///
        /// When it does, a bidirectional solve refuses a model with a negative consumption on
        /// any arc for this resource.
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

        /// @brief How the paired extension function extends backwards. Copied into every clone.
        BackwardKind backward_kind_ = BackwardKind::Unspecified;
};

/// @brief Thrown by a composed feasibility function that has no backward form.
///
/// A bidirectional solve checks for this once at setup and refuses the model.
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

        /// @brief Returns whether the composed resource is feasible in the backward direction.
        ///
        /// Not pure, so forward-only user compositions still compile; the default throws.
        /// @c CompositionFeasibilityFunction tests each component backward.
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

        /// @brief Returns whether a forward and a backward composed label can be merged.
        ///
        /// The answer must be exact: the joiner trusts both a "yes" and a "no", and no setup check
        /// can tell an override that refuses too much. @c CompositionFeasibilityFunction merges
        /// component by component.
        ///
        /// @param resource      The forward label.
        /// @param back_resource The backward label.
        /// @return @c true if merging the two labels yields a feasible path.
        /// @throws NoBackwardFeasibility unless overridden.
        [[nodiscard]] virtual auto can_be_merged(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*back_resource*/) -> bool {
            throw NoBackwardFeasibility(
                "this composition feasibility function has no merge test: override "
                "can_be_merged, as CompositionFeasibilityFunction does");
        };

        /// @brief Always @c Unspecified; a bidirectional solve does not read it.
        ///
        /// Setup reads each component's @c merge_rule() instead, and checks that this function
        /// has a merge test by probing @c can_be_merged once.
        ///
        /// @return @c MergeRule::Unspecified unless overridden.
        [[nodiscard]] virtual MergeRule merge_rule() const { return MergeRule::Unspecified; }

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
