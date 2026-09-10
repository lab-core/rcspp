// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

/// @brief How two half-paths are tested for compatibility at a join.
enum class MergeRule {
    Unspecified,     ///< not declared -- a bidirectional solve will refuse to run
    AlwaysTrue,      ///< this resource never blocks a join
    DominanceOrder,  ///< the resource's own forward comparison: check_dominance(f, b)
    // `Disjoint` retired in step 6: a container resource declares `Custom` and inherits the body
    // from DisjointMergeForm, so the rule no longer has to be interpreted by `Resource`.
    Custom,  ///< the function writes its own can_be_merged body
};

/// @brief Where a feasibility function's backward seed sits, as far as the *type* can tell.
///
/// Three-valued on purpose. @c MinMaxFeasibilityFunction chooses its end from a constructor
/// argument, so no type-level answer exists for it -- and guessing either way is wrong:
///  - guessing @c Ceiling rejects `MinMaxFeasibilityFunction(min, max, /*increasing=*/false)`,
///    which seeds at the *minimum* and is exactly right for an accumulation. The reverse-graph
///    oracle's load resource is built that way on purpose.
///  - guessing @c Never makes the check silent for the one class that produced the defect.
///
/// So @c Unknown is a first-class answer, and it falls through to
/// @c BidirectionalDominanceAlgorithm::seeds_itself_out_of_range -- which asks the sharper
/// question anyway: *is the seed strictly dominated by the unseeded state, per this component's
/// own dominance function*. The runtime probe is the better check; the @c static_assert in
/// @c ResourceGraph's typed @c add_resource is a cheap subset of it, not a replacement.
enum class BackSeedEnd {
    Never,    ///< back_seed_value() is always nullopt
    Ceiling,  ///< always seeds at an upper bound -- incoherent with an accumulating extension
    Unknown,  ///< depends on runtime state; defer to the setup validation
};

/// @brief What a feasibility function's type says about its backward seed.
///
/// Opt-in, like @c ExtensionFunction::kind: an unmigrated or user-written function is
/// @c Unknown and is therefore never falsely rejected.
///
/// A struct with partial specialisations, mirroring @c BackwardKindOf in
/// @c extension_function.hpp -- same shape, same reason, so there is one idiom in the tree
/// rather than two. Each concrete function specialises this at namespace scope in its own
/// header, so the answer sits with the class it describes.
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
        /// Used in bidirectional labeling to check join compatibility.
        ///
        /// @param resource      The forward label's resource value.
        /// @param back_resource The backward label's resource value.
        /// @return @c true if the two labels can be merged into a feasible path.
        /// @throws std::runtime_error If not overridden in a derived class.
        [[nodiscard]] virtual auto can_be_merged(const ResourceType& resource,
                                                 const ResourceType& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        /// @brief Which merge test this resource uses. See @c MergeRule.
        ///
        /// @return The merge rule declared by this feasibility function.
        [[nodiscard]] virtual MergeRule merge_rule() const { return MergeRule::Unspecified; }

        /// @brief The value a backward label starts with at this node, if any.
        ///
        /// A backward label at a sink does not start at zero -- it starts at that node's *upper*
        /// bound (its closing time, its capacity). Return @c std::nullopt to seed at the type
        /// default, which is correct for cost and for every container resource.
        ///
        /// @return The seed value for a backward label at this node, or @c std::nullopt.
        [[nodiscard]] virtual auto back_seed_value() const -> std::optional<ResourceType> {
            return std::nullopt;
        }

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
        /// Defaults to calling @c is_feasible(). Override for asymmetric constraints.
        ///
        /// @param resource The current label holding all component resource values.
        /// @return @c true if the backward feasibility constraint is satisfied.
        [[nodiscard]] virtual auto is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool {
            return is_feasible(resource);
        }

        /// @brief Returns whether a forward and a backward composed label can be merged.
        ///
        /// @param resource      The forward label.
        /// @param back_resource The backward label.
        /// @return @c true if merging the two labels yields a feasible path.
        /// @throws std::runtime_error If not overridden in a derived class.
        [[nodiscard]] virtual auto can_be_merged(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        /// @brief Which merge test this resource uses. See @c MergeRule.
        ///
        /// @return The merge rule declared by this feasibility function.
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
