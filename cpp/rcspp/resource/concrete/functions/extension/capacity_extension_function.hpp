// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/extension/threshold_form.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

namespace rcspp {

/// @brief A capacity: a sum along the path that must stay under a per-node cap (a load, a duration,
///        any limited consumption).
///
/// Forward it adds like @c AdditionExtensionFunction. Backward it is a threshold: a backward label
/// holds how much the prefix may still have consumed at the node, `min(cap_u, b(v) - q)`, and
/// starts at the sink's cap. So the join only compares `forward <= backward`, and a capacity can be
/// the half-way clock, which an addition cannot.
///
/// The per-node capacities must be the ones the paired feasibility function enforces forward, or
/// the two directions solve different models (a backward or bidirectional solve refuses the
/// mismatch). Build
/// both from one @ref SharedNodeBounds:
///
/// @code
/// auto caps = make_node_bounds(0, capacity, per_node_caps);
/// CapacityExtensionFunction<IntResource> extension(caps);
/// MinMaxFeasibilityFunction<IntResource> feasibility(caps);
/// @endcode
///
/// or take them from the feasibility function, `CapacityExtensionFunction(feasibility.bounds())`.
///
/// @tparam ResourceType A NumericalResource-compatible type whose value type is arithmetic.
/// @tparam ValueType    Deduced value type of the resource (default: `ResourceType::get_value()`
///                      return type after decay).
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class CapacityExtensionFunction
    : public Clonable<
          CapacityExtensionFunction<ResourceType, ValueType>,
          TranslationThresholdForm<ResourceType, ExtensionFunction<ResourceType>, ValueType>,
          ExtensionFunction<ResourceType>> {
        // Backward extension subtracts, so an unsigned type would wrap around.
        static_assert(std::is_signed_v<ValueType>,
                      "CapacityExtensionFunction requires a signed value type");

    public:
        /// @brief A capacity with the same cap at every node.
        ///
        /// @param capacity The upper bound at every node.
        explicit CapacityExtensionFunction(ValueType capacity)
            : CapacityExtensionFunction(make_node_bounds(ValueType{0}, capacity)) {}

        /// @brief A capacity whose per-node caps are the upper ends of @p caps, shared with the
        ///        paired feasibility function.
        ///
        /// The lower ends are not read: a capacity does not wait.
        ///
        /// @param caps The per-node bounds, shared with the paired feasibility function; must not
        ///             be null.
        /// @throws std::invalid_argument If @p caps is null.
        explicit CapacityExtensionFunction(SharedNodeBounds<ValueType> caps)
            : caps_(std::move(caps)) {
            if (caps_ == nullptr) {
                throw std::invalid_argument("CapacityExtensionFunction: caps must not be null");
            }
        }

        /// @brief The shared bounds whose upper ends are the caps, to build the paired feasibility
        ///        function from.
        ///
        /// @return The shared bounds.
        [[nodiscard]] auto bounds() const -> const SharedNodeBounds<ValueType>& { return caps_; }

    protected:
        /// @brief None: a capacity does not wait.
        ///
        /// Not the bounds' lower end: a clamp at 0 would hide a negative load, which a
        /// bidirectional solve must see to refuse.
        ///
        /// @param node_id Index of the node the forward extension arrives at (unused).
        /// @return @c std::nullopt.
        [[nodiscard]] std::optional<ValueType> lower_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }

        /// @brief This node's cap, where backward labels are clamped and, at a sink, start.
        ///
        /// @param node_id Index of the node the backward extension arrives at.
        /// @return The upper end of the node's bounds.
        [[nodiscard]] std::optional<ValueType> upper_bound_at(size_t node_id) const final {
            return caps_->upper(node_id);
        }

    private:
        SharedNodeBounds<ValueType> caps_;
};
}  // namespace rcspp
