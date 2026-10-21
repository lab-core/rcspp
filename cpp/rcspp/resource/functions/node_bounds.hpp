// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>

namespace rcspp {

/// @brief Per-node `[lower, upper]` bounds of a scalar resource: a time window, a capacity.
///
/// Problem data, not behaviour: an extension function and a feasibility function that must agree
/// on a node's bounds read the same object, handed to both at construction (see
/// @ref SharedNodeBounds). Immutable once built, so it is safe to share across per-arc clones,
/// per-node clones and graph clones.
///
/// @tparam ValueType The resource's arithmetic value type.
template <typename ValueType>
class NodeBounds {
    public:
        /// @brief Constructs bounds with a default window and optional per-node overrides.
        ///
        /// @param default_lower Lower bound at every node absent from @p by_node.
        /// @param default_upper Upper bound at every node absent from @p by_node.
        /// @param by_node       Per-node `{lower, upper}` windows. May be empty.
        NodeBounds(ValueType default_lower, ValueType default_upper,
                   std::map<size_t, std::pair<ValueType, ValueType>> by_node = {})
            : default_lower_(default_lower),
              default_upper_(default_upper),
              by_node_(std::move(by_node)) {}

        /// @brief The `{lower, upper}` window at @p node_id: its override, or the defaults.
        ///
        /// @param node_id Index of the node.
        /// @return The node's window.
        [[nodiscard]] std::pair<ValueType, ValueType> at(size_t node_id) const {
            auto it = by_node_.find(node_id);
            return it != by_node_.end()
                       ? it->second
                       : std::pair<ValueType, ValueType>{default_lower_, default_upper_};
        }

        /// @brief The lower bound at @p node_id.
        ///
        /// @param node_id Index of the node.
        /// @return The node's lower bound.
        [[nodiscard]] ValueType lower(size_t node_id) const { return at(node_id).first; }

        /// @brief The upper bound at @p node_id.
        ///
        /// @param node_id Index of the node.
        /// @return The node's upper bound.
        [[nodiscard]] ValueType upper(size_t node_id) const { return at(node_id).second; }

        /// @brief The lower bound at nodes without an override.
        [[nodiscard]] ValueType default_lower() const { return default_lower_; }

        /// @brief The upper bound at nodes without an override.
        [[nodiscard]] ValueType default_upper() const { return default_upper_; }

        /// @brief The per-node overrides.
        [[nodiscard]] const std::map<size_t, std::pair<ValueType, ValueType>>& by_node() const {
            return by_node_;
        }

        /// @brief Whether every node has the default window.
        [[nodiscard]] bool is_uniform() const { return by_node_.empty(); }

    private:
        ValueType default_lower_;
        ValueType default_upper_;
        std::map<size_t, std::pair<ValueType, ValueType>> by_node_;
};

/// @brief Node bounds shared, read-only, by the functions that read them.
///
/// @tparam ValueType The resource's arithmetic value type.
template <typename ValueType>
using SharedNodeBounds = std::shared_ptr<const NodeBounds<ValueType>>;

/// @brief Builds a @ref SharedNodeBounds.
///
/// @code
/// auto caps = rcspp::make_node_bounds(0, capacity, per_node_caps);
/// graph.add_resource<IntResource>(
///     std::make_unique<CapacityExtensionFunction<IntResource>>(caps),
///     std::make_unique<MinMaxFeasibilityFunction<IntResource>>(caps), ...);
/// @endcode
///
/// @tparam ValueType The resource's arithmetic value type, deduced from the two defaults, which
/// must
///           agree (write `0.0`, not `0`, for a real resource).
/// @param default_lower Lower bound at every node absent from @p by_node.
/// @param default_upper Upper bound at every node absent from @p by_node.
/// @param by_node       Per-node `{lower, upper}` windows. May be empty.
/// @return The shared bounds.
template <typename ValueType>
[[nodiscard]] SharedNodeBounds<ValueType> make_node_bounds(
    ValueType default_lower, ValueType default_upper,
    std::map<size_t, std::pair<std::type_identity_t<ValueType>, std::type_identity_t<ValueType>>>
        by_node = {}) {
    return std::make_shared<const NodeBounds<ValueType>>(default_lower,
                                                         default_upper,
                                                         std::move(by_node));
}

}  // namespace rcspp
