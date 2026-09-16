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
    DominanceOrder,  ///< the resource's own forward comparison: check_dominance(f, b)
    // `Disjoint` retired in step 6: a container resource declares `Custom` and inherits the body
    // from DisjointMergeForm, so the rule no longer has to be interpreted by `Resource`.
    Custom,  ///< the function writes its own can_be_merged body
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
        /// May depend on @ref backward_kind(): a bound-style backward value is compared against
        /// the forward value, while an accumulating one has to be added to it, and those are
        /// different rules on the same feasibility function. See @c MinMaxFeasibilityFunction.
        ///
        /// @return The merge rule declared by this feasibility function.
        [[nodiscard]] virtual MergeRule merge_rule() const { return MergeRule::Unspecified; }

        /// @brief Records how the paired extension function extends backwards.
        ///
        /// Derived -- never declared by hand. @c ResourceGraph::add_resource sets it from the
        /// extension function's @c backward_kind(), exactly as it sets
        /// @c DominanceFunction::set_backward_reversed from the same value, so the three can never
        /// disagree about one model's shape.
        ///
        /// A feasibility function needs it because a merge test reads a *backward* value and the
        /// kind is what says which quantity that value is. @c MinMaxFeasibilityFunction is the
        /// case that motivated it: paired with a threshold extension its backward label carries a
        /// remaining-capacity ceiling, so the merge test is `consumed <= ceiling`; paired with an
        /// accumulating one the backward label carries the suffix's own consumption, so the test
        /// is `prefix + suffix <= capacity`. The same two values, two incompatible readings, and
        /// only the extension function knows which applies.
        ///
        /// @param kind The paired extension function's declared backward kind.
        void set_backward_kind(BackwardKind kind) { backward_kind_ = kind; }

        /// @brief How the paired extension function extends backwards.
        ///
        /// @c Unspecified until @c ResourceGraph::add_resource pairs this function with an
        /// extension function -- which is also the state a directly constructed function stays
        /// in, so a @c merge_rule() that reads this must treat @c Unspecified as "refuse" rather
        /// than guessing a default.
        ///
        /// @return The declared backward kind of the paired extension function.
        [[nodiscard]] BackwardKind backward_kind() const { return backward_kind_; }

        /// @brief Whether @c can_be_merged may refuse a pair the model would actually accept.
        ///
        /// A merge rule must never *accept* an infeasible splice -- that is not negotiable, and no
        /// rule here does. But a rule that cannot decide exactly from two values may refuse a
        /// feasible one, and an over-strict join is not the harmless direction it appears to be: it
        /// does not corrupt a bound, it makes `bidirectional` answer a stricter question than
        /// `simple` on the same model, with `half_way_point` deciding how much stricter.
        ///
        /// Declaring @c true asks the joiner to verify a refusal -- by replaying the merged path
        /// through the real extenders -- before dropping the pair. That costs `O(path length)` per
        /// refusal, so it is opt-in and should be declared only for the configurations that
        /// actually need it: @c SizeFeasibilityFunction, the one rule that answers @c true, does so
        /// only when per-node caps are configured, because with a uniform cap its test is exact.
        ///
        /// @return @c true when a refusal is worth verifying rather than trusting.
        [[nodiscard]] virtual bool merge_refusal_may_be_conservative() const { return false; }

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

        /// @brief How the paired extension function extends backwards. See @ref set_backward_kind.
        ///
        /// Set on the model's prototype, which @c ResourceFactory::create_resource clones per
        /// node; @c Clonable::clone() copy-constructs, so every clone inherits it. Protected so a
        /// derived @c merge_rule() can read it without a virtual call.
        BackwardKind backward_kind_ = BackwardKind::Unspecified;
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

        /// @brief Whether any component's @c can_be_merged may refuse a feasible pair.
        ///
        /// See the scalar specialisation. Fanned out with OR: one conservative component is enough
        /// to make the composed refusal worth verifying.
        ///
        /// @return @c true when a refusal is worth verifying rather than trusting.
        [[nodiscard]] virtual bool merge_refusal_may_be_conservative() const { return false; }

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
