// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/backward_form.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Additive forward, threshold backward: for capacity- and duration-like resources.
///
/// Forward this is the same accumulation as `AdditionExtensionFunction`. Backward it inverts:
/// `b(u) = min(capacity_u, b(v) - q)`. Both formulas and the clamp discipline come from
/// `TranslationThresholdForm`; this class only answers *what the bounds are*, which for a budget
/// is a ceiling at every node and no floor anywhere.
///
/// This is a separate class from `AdditionExtensionFunction` because the two agree forward and
/// diverge backward: cost *accumulates* and so must keep adding in both directions, while a
/// capacity is a *threshold* and must subtract. `AdditionExtensionFunction` is the cost slot's
/// extension function in every example and binding, so it keeps inheriting the default
/// `extend_back`.
///
/// @note Known wart: the per-node bounds duplicate the map that `MinMaxFeasibilityFunction`
///       also holds. That is an existing pattern in this library -- `TimeWindowExtensionFunction`
///       and `TimeWindowFeasibilityFunction` are already constructed with the same window map --
///       so it is consistent rather than novel. Making the map optional keeps the common
///       uniform-capacity case free of it entirely.
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
        // Kept, and stricter than the form's conditional `kind`: a budget cannot be instantiated
        // for an unsigned type at all, whereas TimeWindowExtensionFunction can and declines to
        // declare. Backward extension subtracts the arc consumption, so the value type must be
        // able to go negative; an unsigned type would wrap to a huge positive value that reads
        // as a very loose budget.
        static_assert(std::is_signed_v<ValueType>,
                      "BudgetExtensionFunction requires a signed value type");

    public:
        /// @brief Constructs a BudgetExtensionFunction with optional per-node upper bounds.
        ///
        /// @param max_by_node_id Per-node upper bounds. May be empty: with a *uniform* capacity
        ///                       the clamp never binds (`b(v) <= Q` implies `b(v) - q <= Q`), so
        ///                       the map is only needed when per-node limits exist.
        /// @param default_max    Bound used at nodes absent from the map. Defaults to half of the
        ///                       value type's maximum so the forward addition cannot overflow.
        explicit BudgetExtensionFunction(
            std::map<size_t, ValueType> max_by_node_id = {},
            ValueType default_max = std::numeric_limits<ValueType>::max() / 2)
            : max_by_node_id_(max_by_node_id.empty()
                                  ? nullptr
                                  : std::make_shared<const std::map<size_t, ValueType>>(
                                        std::move(max_by_node_id))),
              default_max_(default_max) {}

    protected:
        /// @brief A budget has no forward clamp: consumption accumulates without waiting.
        ///
        /// @param node_id Index of the node the forward extension arrives at (unused).
        /// @return @c std::nullopt.
        [[nodiscard]] std::optional<ValueType> lower_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }

        /// @brief This node's capacity, defaulting to the constructor's bound.
        ///
        /// @param node_id Index of the node the backward extension arrives at.
        /// @return The node's upper bound, or @c default_max_ if it has none.
        [[nodiscard]] std::optional<ValueType> upper_bound_at(size_t node_id) const final {
            if (max_by_node_id_ == nullptr) {
                return default_max_;
            }
            auto it = max_by_node_id_->find(node_id);
            return it != max_by_node_id_->end() ? it->second : default_max_;
        }

    private:
        std::shared_ptr<const std::map<size_t, ValueType>> max_by_node_id_;
        ValueType default_max_;
};
}  // namespace rcspp
