// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/backward_form.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

namespace rcspp {

/// @brief Additive forward, threshold backward: for capacity- and duration-like resources.
///
/// Forward it adds like `AdditionExtensionFunction`; backward it inverts:
/// `b(u) = min(capacity_u, b(v) - q)`. The formulas come from `TranslationThresholdForm`; this
/// class only supplies the bounds: a ceiling at every node and no floor.
///
/// The per-node capacities must be the ones the paired feasibility function enforces forward, or
/// the two searches solve different models (a bidirectional solve refuses the mismatch). Build
/// both from one @ref SharedNodeBounds:
///
/// @code
/// auto caps = make_node_bounds(0, capacity, per_node_caps);
/// BudgetExtensionFunction<IntResource> extension(caps);
/// MinMaxFeasibilityFunction<IntResource> feasibility(caps);
/// @endcode
///
/// or take them from the feasibility function, `BudgetExtensionFunction(feasibility.bounds())`.
/// @c presets::add_budget_resource does the first.
///
/// @tparam ResourceType A NumericalResource-compatible type whose value type is arithmetic.
/// @tparam ValueType    Deduced value type of the resource (default: `ResourceType::get_value()`
///                      return type after decay).
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class BudgetExtensionFunction
    : public Clonable<
          BudgetExtensionFunction<ResourceType, ValueType>,
          TranslationThresholdForm<ResourceType, ExtensionFunction<ResourceType>, ValueType>,
          ExtensionFunction<ResourceType>> {
        // Backward extension subtracts, so an unsigned type would wrap around.
        static_assert(std::is_signed_v<ValueType>,
                      "BudgetExtensionFunction requires a signed value type");

    public:
        /// @brief Constructs a budget with the same capacity at every node.
        ///
        /// @param capacity The upper bound at every node.
        explicit BudgetExtensionFunction(ValueType capacity)
            : BudgetExtensionFunction(make_node_bounds(ValueType{0}, capacity)) {}

        /// @brief Constructs a budget whose per-node capacities are the upper ends of @p caps.
        ///
        /// Only the upper ends are read. A budget does not wait, so the lower ends, typically 0,
        /// are not a forward clamp; see @ref lower_bound_at.
        ///
        /// @param caps The per-node bounds, shared with the paired feasibility function; must not
        ///             be null.
        /// @throws std::invalid_argument If @p caps is null.
        explicit BudgetExtensionFunction(SharedNodeBounds<ValueType> caps)
            : caps_(std::move(caps)) {
            if (caps_ == nullptr) {
                throw std::invalid_argument("BudgetExtensionFunction: caps must not be null");
            }
        }

        /// @brief The per-node bounds whose upper ends are this budget's capacities.
        ///
        /// @return The shared bounds.
        [[nodiscard]] auto bounds() const -> const SharedNodeBounds<ValueType>& { return caps_; }

    protected:
        /// @brief A budget has no forward clamp: consumption accumulates without waiting.
        ///
        /// Deliberately not the lower end of the bounds: a clamp at 0 would silently raise a
        /// negative load to 0, where a bidirectional solve refuses it.
        ///
        /// @param node_id Index of the node the forward extension arrives at (unused).
        /// @return @c std::nullopt.
        [[nodiscard]] std::optional<ValueType> lower_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }

        /// @brief This node's capacity.
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
